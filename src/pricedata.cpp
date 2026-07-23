#include "pricedata.h"

#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonParseError>
#include <QTimeZone>

#include <cmath>
#include <limits>

QDateTime PriceData::quarterStart(const QDateTime &utc)
{
    const qint64 seconds = utc.toUTC().toMSecsSinceEpoch() / 1000;
    return QDateTime::fromMSecsSinceEpoch((seconds - ((seconds % 900 + 900) % 900)) * 1000, Qt::UTC);
}

QDateTime PriceData::hourStartUtc(const QDateTime &utc)
{
    const qint64 milliseconds = utc.toUTC().toMSecsSinceEpoch();
    constexpr qint64 hourMillis = 60 * 60 * 1000;
    return QDateTime::fromMSecsSinceEpoch(
                milliseconds - ((milliseconds % hourMillis + hourMillis) % hourMillis), Qt::UTC);
}

QDateTime PriceData::localHourStart(const QDateTime &utc, const QTimeZone &zone)
{
    const QDateTime local = utc.toTimeZone(zone);
    return QDateTime(local.date(), QTime(local.time().hour(), 0), zone);
}

double PriceData::centsPerKwh(double eurPerMwh, bool includeVat, double vatPercent)
{
    return (eurPerMwh / 10.0) * (includeVat ? 1.0 + vatPercent / 100.0 : 1.0);
}

bool PriceData::isNightRate(const QDateTime &utc)
{
    const QTime time = utc.toLocalTime().time();
    return time >= QTime(22, 0) || time < QTime(7, 0);
}

double PriceData::personalPriceCents(const QDateTime &utc, const AppConfig &config)
{
    if (!config.personalPriceEnabled)
        return 0.0;
    if (!config.personalPriceDayNight)
        return config.personalPriceFlat;
    return isNightRate(utc) ? config.personalPriceNight : config.personalPriceDay;
}

double PriceData::electricityTaxCents(const AppConfig &config)
{
    return config.personalPriceEnabled ? config.electricityTax : 0.0;
}

double PriceData::displayedCentsPerKwh(const PricePoint &point, const AppConfig &config)
{
    return centsPerKwh(point.eurPerMwh, config.includeVat, config.vatPercent)
            + personalPriceCents(point.startUtc, config) + electricityTaxCents(config);
}

double PriceData::spotEurPerMwhForDisplayedCents(double displayedCents,
                                                  const QDateTime &utc,
                                                  const AppConfig &config)
{
    const double vatMultiplier = config.includeVat ? 1.0 + config.vatPercent / 100.0 : 1.0;
    return (displayedCents - personalPriceCents(utc, config)
            - electricityTaxCents(config)) * 10.0 / vatMultiplier;
}

QString PriceData::bandForAverage(double average, const AppConfig &config)
{
    if (average < 0.0)
        return QStringLiteral("negative");
    if (average < config.cheapBelow)
        return QStringLiteral("cheap");
    if (average > config.expensiveAbove)
        return QStringLiteral("expensive");
    return QStringLiteral("middle");
}

PriceWindow PriceData::window(const QMap<qint64, PricePoint> &prices,
                              const QDateTime &nowUtc,
                              const AppConfig &config)
{
    PriceWindow result;
    result.quarterStartUtc = quarterStart(nowUtc);
    double total = 0.0;
    for (int i = 0; i < 4; ++i) {
        const qint64 key = result.quarterStartUtc.addSecs(i * 900).toMSecsSinceEpoch();
        if (!prices.contains(key)) {
            result.band = QStringLiteral("unknown");
            return result;
        }
        const PricePoint point = prices.value(key);
        result.points.append(point);
        total += displayedCentsPerKwh(point, config);
    }
    result.complete = true;
    result.averageCentsPerKwh = total / 4.0;
    result.band = bandForAverage(result.averageCentsPerKwh, config);
    return result;
}

bool PriceData::parseApi(const QByteArray &json,
                         QMap<qint64, PricePoint> *prices,
                         QString *error)
{
    QJsonParseError parseError;
    const QJsonDocument document = QJsonDocument::fromJson(json, &parseError);
    if (parseError.error != QJsonParseError::NoError || !document.isObject()) {
        if (error) *error = QStringLiteral("Invalid JSON: %1").arg(parseError.errorString());
        return false;
    }
    const QJsonObject response = document.object();
    if (!response.value(QStringLiteral("success")).toBool(false)) {
        if (error) *error = QStringLiteral("Elering LIVE did not report a successful response");
        return false;
    }
    const QJsonArray array = response.value(QStringLiteral("data"))
                                 .toObject().value(QStringLiteral("fi")).toArray();
    if (array.isEmpty()) {
        if (error) *error = QStringLiteral("Elering LIVE response contained no Finnish prices");
        return false;
    }

    QMap<qint64, PricePoint> parsed;
    for (const QJsonValue &entry : array) {
        const QJsonObject object = entry.toObject();
        const double timestamp = object.value(QStringLiteral("timestamp")).toDouble(qQNaN());
        const double value = object.value(QStringLiteral("price")).toDouble(qQNaN());
        const double maxEpochSeconds = static_cast<double>(std::numeric_limits<uint>::max());
        // qreal is float on the 32-bit Sailfish target.  Keep validation in
        // double precision: a 2026 Unix timestamp cannot be represented
        // exactly as a float and would otherwise be rejected as fractional.
        if (!std::isfinite(timestamp) || timestamp < 0.0 || timestamp > maxEpochSeconds
                || std::floor(timestamp) != timestamp
                || !std::isfinite(value)) {
            if (error) *error = QStringLiteral("Elering LIVE response contained an invalid Finnish price record");
            return false;
        }
        // Qt 5.1 on the Sailfish SDK predates fromSecsSinceEpoch().
        QDateTime start = QDateTime::fromTime_t(static_cast<uint>(timestamp));
        start.setTimeSpec(Qt::UTC);
        const qint64 key = start.toMSecsSinceEpoch();
        if (key % 900000 != 0) {
            if (error) *error = QStringLiteral("Elering LIVE price timestamp is not on a quarter-hour boundary");
            return false;
        }
        if (parsed.contains(key) && !qFuzzyCompare(parsed.value(key).eurPerMwh + 1.0, value + 1.0)) {
            if (error) *error = QStringLiteral("Elering LIVE response contained conflicting duplicate records");
            return false;
        }
        parsed.insert(key, PricePoint { start, value });
    }
    if (prices)
        *prices = parsed;
    return true;
}

QJsonArray PriceData::toJson(const QMap<qint64, PricePoint> &prices)
{
    QJsonArray result;
    for (const PricePoint &point : prices) {
        result.append(QJsonObject {
            { QStringLiteral("startUtc"), point.startUtc.toUTC().toString(QStringLiteral("yyyy-MM-dd'T'HH:mm:ss.zzz'Z'")) },
            { QStringLiteral("eurPerMwh"), point.eurPerMwh }
        });
    }
    return result;
}

bool PriceData::fromJson(const QJsonArray &array,
                         QMap<qint64, PricePoint> *prices,
                         QString *error)
{
    QMap<qint64, PricePoint> parsed;
    for (const QJsonValue &entry : array) {
        const QJsonObject object = entry.toObject();
        QDateTime start = QDateTime::fromString(object.value(QStringLiteral("startUtc")).toString(), Qt::ISODate);
        const double value = object.value(QStringLiteral("eurPerMwh")).toDouble(qQNaN());
        if (!start.isValid() || !qIsFinite(value)) {
            if (error) *error = QStringLiteral("Cache contains an invalid record");
            return false;
        }
        start = start.toUTC();
        const qint64 key = start.toMSecsSinceEpoch();
        if (key % 900000 != 0 || parsed.contains(key)) {
            if (error) *error = QStringLiteral("Cache contains duplicate or unaligned records");
            return false;
        }
        parsed.insert(key, PricePoint { start, value });
    }
    if (prices)
        *prices = parsed;
    return true;
}

bool PriceData::containsCompleteLocalDay(const QMap<qint64, PricePoint> &prices,
                                         const QDate &date,
                                         const QByteArray &timeZoneId)
{
    const QTimeZone zone(timeZoneId);
    if (!zone.isValid() || !date.isValid())
        return false;
    const QDateTime start(date, QTime(0, 0), zone);
    const QDateTime end(date.addDays(1), QTime(0, 0), zone);
    for (QDateTime cursor = start.toUTC(); cursor < end.toUTC(); cursor = cursor.addSecs(900)) {
        if (!prices.contains(cursor.toMSecsSinceEpoch()))
            return false;
    }
    return true;
}
