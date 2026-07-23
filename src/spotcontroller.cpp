#include "spotcontroller.h"

#include "automationrunner.h"
#include "timedscheduler.h"

#include <QDateTime>
#include <QCoreApplication>
#include <QDebug>
#include <QDir>
#include <QFileInfo>
#include <QLocale>
#include <QSet>
#include <QTimeZone>
#include <QTimer>
#include <QUrl>
#include <QProcess>
#include <limits>
#include <QtMath>

namespace {
const QTimeZone kHelsinki(QByteArray("Europe/Helsinki"));

struct Aggregate {
    qint64 x = 0;
    qint64 endX = 0;
    double sum = 0.0;
    double min = std::numeric_limits<double>::max();
    double max = std::numeric_limits<double>::lowest();
    double spotSum = 0.0;
    double vatSum = 0.0;
    double transferSum = 0.0;
    double electricityTaxSum = 0.0;
    int count = 0;
};

struct PriceParts {
    double spot = 0.0;
    double vat = 0.0;
    double transfer = 0.0;
    double electricityTax = 0.0;

    double total() const { return spot + vat + transfer + electricityTax; }
};

PriceParts priceParts(const PricePoint &point, const AppConfig &config)
{
    PriceParts parts;
    parts.spot = point.eurPerMwh / 10.0;
    parts.vat = config.includeVat ? parts.spot * config.vatPercent / 100.0 : 0.0;
    parts.transfer = PriceData::personalPriceCents(point.startUtc, config);
    parts.electricityTax = PriceData::electricityTaxCents(config);
    return parts;
}

QVariantMap chartPoint(qint64 start, qint64 end, const PriceParts &parts,
                       double minimum, double maximum)
{
    return {
        { QStringLiteral("x"), start },
        { QStringLiteral("endX"), end },
        { QStringLiteral("avg"), parts.total() },
        { QStringLiteral("min"), minimum },
        { QStringLiteral("max"), maximum },
        { QStringLiteral("spot"), parts.spot },
        { QStringLiteral("vat"), parts.vat },
        { QStringLiteral("transfer"), parts.transfer },
        { QStringLiteral("electricityTax"), parts.electricityTax }
    };
}

QString priceText(double price)
{
    return QLocale().toString(price, 'f', 2);
}

QString coverPriceText(double price)
{
    if (price > 99.99)
        return QStringLiteral(">99.9");
    if (price < -99.99)
        return QStringLiteral("<-99.9");
    return QString::number(price, 'f', 2);
}

QString timeRange(const QDateTime &startUtc, int seconds)
{
    const QDateTime localStart = startUtc.toLocalTime();
    const QDateTime localEnd = startUtc.addSecs(seconds).toLocalTime();
    return localStart.time().toString(QStringLiteral("HH:mm"))
            + QStringLiteral(" – ")
            + localEnd.time().toString(QStringLiteral("HH:mm"));
}

QString utcOffsetText(const QDateTime &localTime)
{
    const int offset = localTime.offsetFromUtc();
    const QChar sign = offset >= 0 ? QLatin1Char('+') : QLatin1Char('-');
    const int absoluteOffset = qAbs(offset);
    return QStringLiteral("UTC%1%2:%3").arg(sign)
            .arg(absoluteOffset / 3600, 2, 10, QLatin1Char('0'))
            .arg((absoluteOffset % 3600) / 60, 2, 10, QLatin1Char('0'));
}

}

SpotController::SpotController(QObject *parent, TimedScheduler *scheduler)
    : QObject(parent)
    , m_config(AppConfig::defaultsForInstalledFiles())
    , m_scheduler(scheduler)
{
    reload();
}

bool SpotController::enabled() const { return m_config.enabled; }
void SpotController::setEnabled(bool value)
{
    if (m_config.enabled == value) return;
    m_config.enabled = value;
    saveConfigAndRequest();
}
bool SpotController::testMode() const { return m_config.testMode; }
void SpotController::setTestMode(bool value)
{
    if (m_config.testMode == value) return;
    m_config.testMode = value;

    QString error;
    if (!m_store.saveConfig(m_config, &error)) {
        m_localError = error;
    } else if (!value && !m_store.clearCache(&error)) {
        m_localError = error;
    } else {
        m_localError.clear();
        if (!value) {
            m_prices.clear();
            m_state = QJsonObject();
        }
        requestRefresh();
    }
    emit configChanged();
    emit stateChanged();
}
bool SpotController::includeVat() const { return m_config.includeVat; }
void SpotController::setIncludeVat(bool value)
{
    if (m_config.includeVat == value) return;
    m_config.includeVat = value;
    saveConfigAndRequest();
}
double SpotController::vatPercent() const { return m_config.vatPercent; }
void SpotController::setVatPercent(double value)
{
    if (qFuzzyCompare(m_config.vatPercent, value) || value < 0.0 || value > 100.0) return;
    m_config.vatPercent = value;
    saveConfigAndRequest();
}
double SpotController::cheapBelow() const { return m_config.cheapBelow; }
void SpotController::setCheapBelow(double value)
{
    if (qFuzzyCompare(m_config.cheapBelow, value) || value <= 0.0 || value >= m_config.expensiveAbove) return;
    m_config.cheapBelow = value;
    saveConfigAndRequest();
}
double SpotController::expensiveAbove() const { return m_config.expensiveAbove; }
void SpotController::setExpensiveAbove(double value)
{
    if (qFuzzyCompare(m_config.expensiveAbove, value) || value <= m_config.cheapBelow) return;
    m_config.expensiveAbove = value;
    saveConfigAndRequest();
}
QString SpotController::publicationTime() const { return m_config.publicationTime.toString(QStringLiteral("HH:mm")); }
void SpotController::setPublicationTime(const QString &value)
{
    const QTime time = QTime::fromString(value.trimmed(), QStringLiteral("HH:mm"));
    if (!time.isValid() || time == m_config.publicationTime) return;
    m_config.publicationTime = time;
    saveConfigAndRequest();
}

QString SpotController::averageText() const
{
    const QJsonValue value = m_state.value(QStringLiteral("averageCentsPerKwh"));
    return value.isDouble() ? priceText(value.toDouble()) : QStringLiteral("—");
}

QString SpotController::currentPriceText() const
{
    const qint64 key = PriceData::quarterStart(QDateTime::currentDateTimeUtc()).toMSecsSinceEpoch();
    if (!m_prices.contains(key))
        return QStringLiteral("—");
    return priceText(PriceData::displayedCentsPerKwh(m_prices.value(key), m_config));
}

QString SpotController::coverAverageText() const
{
    const QJsonValue value = m_state.value(QStringLiteral("averageCentsPerKwh"));
    return value.isDouble() ? ::coverPriceText(value.toDouble()) : QStringLiteral("—");
}

QString SpotController::coverPriceText() const
{
    const qint64 key = PriceData::quarterStart(QDateTime::currentDateTimeUtc()).toMSecsSinceEpoch();
    if (!m_prices.contains(key))
        return QStringLiteral("—");
    return ::coverPriceText(PriceData::displayedCentsPerKwh(m_prices.value(key), m_config));
}

QString SpotController::bandLabel(const QString &band) const
{
    if (band == QStringLiteral("negative")) return tr("Negative");
    if (band == QStringLiteral("cheap")) return tr("Cheap");
    if (band == QStringLiteral("middle")) return tr("Middle range");
    if (band == QStringLiteral("expensive")) return tr("Expensive");
    return tr("Unknown");
}

QString SpotController::bandKey() const
{
    const QString band = m_state.value(QStringLiteral("band")).toString();
    return AppConfig::bands().contains(band) ? band : QStringLiteral("unknown");
}

QString SpotController::bandText() const { return bandLabel(m_state.value(QStringLiteral("band")).toString()); }
QString SpotController::effectiveAmbienceName() const
{
    const QString name = m_state.value(QStringLiteral("effectiveTargetName")).toString();
    return name.isEmpty() ? tr("Keep current ambience") : name;
}

QString SpotController::formatUtc(const QString &iso) const
{
    const QDateTime value = QDateTime::fromString(iso, Qt::ISODate);
    if (!value.isValid()) return tr("Never");
    return QLocale().toString(value.toLocalTime(), QLocale::ShortFormat);
}

QString SpotController::cacheRangeText() const
{
    const QString start = m_state.value(QStringLiteral("cacheStartUtc")).toString();
    const QString end = m_state.value(QStringLiteral("cacheEndUtc")).toString();
    if (start.isEmpty() || end.isEmpty()) return tr("No cached prices");
    return tr("%1 – %2").arg(formatUtc(start), formatUtc(end));
}
QString SpotController::lastRefreshText() const { return formatUtc(m_state.value(QStringLiteral("lastSuccessUtc")).toString()); }
QString SpotController::nextRefreshText() const
{
    const QDateTime now = QDateTime::currentDateTime().toTimeZone(kHelsinki);
    QDateTime next(now.date(), m_config.publicationTime, kHelsinki);
    if (next <= now) next = QDateTime(now.date().addDays(1), m_config.publicationTime, kHelsinki);
    return QLocale().toString(next, QLocale::ShortFormat);
}
QString SpotController::errorText() const
{
    const QString stateError = m_state.value(QStringLiteral("lastError")).toString();
    return !m_localError.isEmpty() ? m_localError : stateError;
}
QString SpotController::warningText() const { return m_state.value(QStringLiteral("warning")).toString(); }

QVariantList SpotController::quarterValues() const
{
    QVariantList result;
    const QJsonArray components = m_state.value(QStringLiteral("components")).toArray();
    for (const QJsonValue &entry : components) {
        const QJsonObject object = entry.toObject();
        const QDateTime utc = QDateTime::fromString(object.value(QStringLiteral("startUtc")).toString(), Qt::ISODate);
        double spot = object.value(QStringLiteral("spotCentsPerKwh")).toDouble();
        double transfer = object.value(QStringLiteral("transferCentsPerKwh")).toDouble();
        if ((!object.contains(QStringLiteral("spotCentsPerKwh"))
                || !object.contains(QStringLiteral("transferCentsPerKwh")))
                && m_prices.contains(utc.toMSecsSinceEpoch())) {
            const PriceParts parts = priceParts(m_prices.value(utc.toMSecsSinceEpoch()), m_config);
            spot = parts.spot + parts.vat;
            transfer = parts.transfer + parts.electricityTax;
        }
        result.append(QVariantMap {
            { QStringLiteral("time"), utc.toLocalTime().time().toString(QStringLiteral("HH:mm")) },
            { QStringLiteral("spot"), priceText(spot) },
            { QStringLiteral("transfer"), priceText(transfer) },
            { QStringLiteral("price"), priceText(object.value(QStringLiteral("centsPerKwh")).toDouble()) }
        });
    }
    return result;
}

QVariantList SpotController::availableAmbiences() const { return m_ambiences; }
void SpotController::reload()
{
    bool configOk = false;
    QString configError;
    m_config = m_store.loadConfig(&configOk, &configError);
    if (!configOk && !QFileInfo::exists(m_store.configPath())) {
        m_store.saveConfig(m_config);
        configError.clear();
    }
    if (m_scheduler) {
        QString schedulerError;
        if (!m_scheduler->sync(m_config.enabled, &schedulerError) && configError.isEmpty())
            configError = schedulerError;
    }
    bool cacheOk = false;
    QString cacheError;
    m_prices = m_store.loadPrices(&cacheOk, &cacheError);
    bool stateOk = false;
    QString stateError;
    m_state = m_store.loadState(&stateOk, &stateError);
    m_localError = !configError.isEmpty() ? configError : (!cacheError.isEmpty() && QFileInfo::exists(m_store.cachePath()) ? cacheError : QString());
    reloadAmbiences();
    emit configChanged();
    emit stateChanged();
}

void SpotController::requestRefresh()
{
    if (m_refreshProcess && m_refreshProcess->state() != QProcess::NotRunning) {
        // Settings may have been changed after the worker read its config.
        // Run once more when it exits so the latest saved settings always win.
        m_refreshQueued = true;
        return;
    }
    if (!m_refreshProcess)
        m_refreshProcess = new QProcess(this);
    connect(m_refreshProcess,
            static_cast<void (QProcess::*)(int, QProcess::ExitStatus)>(&QProcess::finished),
            this, [this](int exitCode, QProcess::ExitStatus exitStatus) {
        const bool runAgain = m_refreshQueued;
        m_refreshQueued = false;
        reload();
        if (exitStatus != QProcess::NormalExit || exitCode != 0) {
            m_localError = tr("Price refresh failed");
            emit stateChanged();
        }
        if (runAgain)
            QTimer::singleShot(0, this, &SpotController::requestRefresh);
    }, Qt::UniqueConnection);
    m_refreshProcess->start(QCoreApplication::applicationFilePath(), { QStringLiteral("--run-once") });
    if (!m_refreshProcess->waitForStarted(1000)) {
        m_localError = m_refreshProcess->errorString();
        emit stateChanged();
    }
}

void SpotController::clearCache()
{
    QString error;
    if (!m_store.clearCache(&error))
        m_localError = error;
    else {
        m_prices.clear();
        m_state = QJsonObject();
        requestRefresh();
    }
    emit stateChanged();
}

void SpotController::reloadAmbiences()
{
    bool ok = false;
    const QVariantList cached = m_store.loadAmbiences(&ok);
    if (ok) {
        if (cached != m_ambiences) {
            m_ambiences = cached;
            qInfo() << "Loaded" << m_ambiences.size() << "cached ambiences";
            emit ambiencesChanged();
        }
        m_inventoryRefreshRequested = false;
    } else if (!m_inventoryRefreshRequested) {
        m_inventoryRefreshRequested = true;
        requestRefresh();
    }
}

QVariantMap SpotController::mappingInfo(const QString &band) const
{
    const bool override = band == QStringLiteral("override");
    const AmbienceMapping mapping = override ? m_config.overrideMapping : m_config.mappings.value(band);
    const auto inventoryContains = [this](const QString &url) {
        for (const QVariant &entry : m_ambiences) {
            if (AutomationRunner::normalizeAmbienceUrl(entry.toMap().value(QStringLiteral("url")).toString())
                    == AutomationRunner::normalizeAmbienceUrl(url)) {
                return entry.toMap().value(QStringLiteral("available"), true).toBool();
            }
        }
        return false;
    };
    const bool available = inventoryContains(mapping.url)
            || (m_ambiences.isEmpty() && AutomationRunner::mappingAvailable(mapping.url));
    const AmbienceMapping unknown = m_config.mappings.value(QStringLiteral("unknown"));
    const bool unknownAvailable = inventoryContains(unknown.url)
            || (m_ambiences.isEmpty() && AutomationRunner::mappingAvailable(unknown.url));
    QString effectiveName;
    if (available)
        effectiveName = mapping.displayName;
    else if (band != QStringLiteral("unknown") && unknownAvailable)
        effectiveName = unknown.displayName;
    else
        effectiveName = tr("Keep current ambience");
    return {
        { QStringLiteral("band"), band },
        { QStringLiteral("bandName"), override ? tr("Timed override") : bandLabel(band) },
        { QStringLiteral("url"), mapping.url },
        { QStringLiteral("displayName"), mapping.displayName.isEmpty()
              ? AppConfig::preferredName(override ? QStringLiteral("cheap") : band) : mapping.displayName },
        { QStringLiteral("preferredName"), AppConfig::preferredName(
              override ? QStringLiteral("cheap") : band) },
        { QStringLiteral("available"), available },
        { QStringLiteral("configured"), !mapping.url.isEmpty() },
        { QStringLiteral("usingFallback"), !available },
        { QStringLiteral("effectiveName"), effectiveName }
    };
}

void SpotController::setMapping(const QString &band, const QString &url, const QString &displayName)
{
    if (!AppConfig::bands().contains(band) && band != QStringLiteral("override")) return;
    AmbienceMapping &mapping = band == QStringLiteral("override")
            ? m_config.overrideMapping : m_config.mappings[band];
    mapping.url = url.trimmed();
    mapping.displayName = displayName;
    saveConfigAndRequest();
}

bool SpotController::ambienceAvailable(const QString &url) const
{
    const QString normalized = AutomationRunner::normalizeAmbienceUrl(url);
    for (const QVariant &entry : m_ambiences) {
        if (AutomationRunner::normalizeAmbienceUrl(entry.toMap().value(QStringLiteral("url")).toString())
                == normalized)
            return entry.toMap().value(QStringLiteral("available"), true).toBool();
    }
    return AutomationRunner::mappingAvailable(url);
}

void SpotController::setAvailableAmbiences(const QVariantList &ambiences)
{
    QVariantList inventory;
    QSet<QString> seen;
    for (const QVariant &entry : ambiences) {
        QVariantMap item = entry.toMap();
        const QString url = item.value(QStringLiteral("url")).toString().trimmed();
        const QString normalized = AutomationRunner::normalizeAmbienceUrl(url);
        if (normalized.isEmpty() || seen.contains(normalized))
            continue;
        seen.insert(normalized);
        item.insert(QStringLiteral("url"), url);
        item.insert(QStringLiteral("available"), true);
        inventory.append(item);
    }
    if (inventory != m_ambiences) {
        m_ambiences = inventory;
        QString error;
        // The live QML model may briefly be empty while Sailfish refreshes
        // it. Reflect that empty result in the picker, but retain the last
        // non-empty public inventory for the sandboxed worker so it does not
        // turn an observation gap into a false "unavailable" warning.
        if (!m_ambiences.isEmpty() && !m_store.saveAmbiences(m_ambiences, &error))
            m_localError = error;
        emit ambiencesChanged();
        if (!error.isEmpty())
            emit stateChanged();
    }
}

QVariantMap SpotController::settingsSnapshot() const
{
    QVariantMap mappings;
    for (const QString &band : AppConfig::bands()) {
        const AmbienceMapping mapping = m_config.mappings.value(band);
        mappings.insert(band, QVariantMap {
            { QStringLiteral("url"), mapping.url },
            { QStringLiteral("displayName"), mapping.displayName }
        });
    }
    return {
        { QStringLiteral("enabled"), m_config.enabled },
        { QStringLiteral("testMode"), m_config.testMode },
        { QStringLiteral("includeVat"), m_config.includeVat },
        { QStringLiteral("vatPercent"), m_config.vatPercent },
        { QStringLiteral("personalPriceEnabled"), m_config.personalPriceEnabled },
        { QStringLiteral("personalPriceDayNight"), m_config.personalPriceDayNight },
        { QStringLiteral("personalPriceFlat"), m_config.personalPriceFlat },
        { QStringLiteral("personalPriceDay"), m_config.personalPriceDay },
        { QStringLiteral("personalPriceNight"), m_config.personalPriceNight },
        { QStringLiteral("electricityTax"), m_config.electricityTax },
        { QStringLiteral("cheapBelow"), m_config.cheapBelow },
        { QStringLiteral("expensiveAbove"), m_config.expensiveAbove },
        { QStringLiteral("publicationTime"), publicationTime() },
        { QStringLiteral("overrideEnabled"), m_config.overrideEnabled },
        { QStringLiteral("overrideStart"), m_config.overrideStart.toString(QStringLiteral("HH:mm")) },
        { QStringLiteral("overrideEnd"), m_config.overrideEnd.toString(QStringLiteral("HH:mm")) },
        { QStringLiteral("overrideMapping"), QVariantMap {
              { QStringLiteral("url"), m_config.overrideMapping.url },
              { QStringLiteral("displayName"), m_config.overrideMapping.displayName }
          } },
        { QStringLiteral("mappings"), mappings }
    };
}

bool SpotController::applySettings(const QVariantMap &settings)
{
    AppConfig updated = m_config;
    updated.enabled = settings.value(QStringLiteral("enabled"), updated.enabled).toBool();
    updated.testMode = settings.value(QStringLiteral("testMode"), updated.testMode).toBool();
    updated.includeVat = settings.value(QStringLiteral("includeVat"), updated.includeVat).toBool();
    updated.vatPercent = settings.value(QStringLiteral("vatPercent"), updated.vatPercent).toDouble();
    updated.personalPriceEnabled = settings.value(QStringLiteral("personalPriceEnabled"),
                                                   updated.personalPriceEnabled).toBool();
    updated.personalPriceDayNight = settings.value(QStringLiteral("personalPriceDayNight"),
                                                    updated.personalPriceDayNight).toBool();
    updated.personalPriceFlat = settings.value(QStringLiteral("personalPriceFlat"),
                                                updated.personalPriceFlat).toDouble();
    updated.personalPriceDay = settings.value(QStringLiteral("personalPriceDay"),
                                               updated.personalPriceDay).toDouble();
    updated.personalPriceNight = settings.value(QStringLiteral("personalPriceNight"),
                                                 updated.personalPriceNight).toDouble();
    updated.electricityTax = settings.value(QStringLiteral("electricityTax"),
                                             updated.electricityTax).toDouble();
    updated.cheapBelow = settings.value(QStringLiteral("cheapBelow"), updated.cheapBelow).toDouble();
    updated.expensiveAbove = settings.value(QStringLiteral("expensiveAbove"), updated.expensiveAbove).toDouble();
    updated.overrideEnabled = settings.value(QStringLiteral("overrideEnabled"), updated.overrideEnabled).toBool();
    const QTime publication = QTime::fromString(settings.value(QStringLiteral("publicationTime")).toString(),
                                                 QStringLiteral("HH:mm"));
    const QTime overrideStart = QTime::fromString(settings.value(QStringLiteral("overrideStart")).toString(),
                                                   QStringLiteral("HH:mm"));
    const QTime overrideEnd = QTime::fromString(settings.value(QStringLiteral("overrideEnd")).toString(),
                                                 QStringLiteral("HH:mm"));
    if (publication.isValid()) updated.publicationTime = publication;
    if (overrideStart.isValid()) updated.overrideStart = overrideStart;
    if (overrideEnd.isValid()) updated.overrideEnd = overrideEnd;

    const QVariantMap mappings = settings.value(QStringLiteral("mappings")).toMap();
    for (const QString &band : AppConfig::bands()) {
        const QVariantMap mapping = mappings.value(band).toMap();
        if (!mapping.isEmpty()) {
            updated.mappings[band].url = mapping.value(QStringLiteral("url")).toString();
            updated.mappings[band].displayName = mapping.value(QStringLiteral("displayName")).toString();
        }
    }
    const QVariantMap overrideMapping = settings.value(QStringLiteral("overrideMapping")).toMap();
    if (!overrideMapping.isEmpty()) {
        updated.overrideMapping.url = overrideMapping.value(QStringLiteral("url")).toString();
        updated.overrideMapping.displayName = overrideMapping.value(QStringLiteral("displayName")).toString();
    }

    QString validationError;
    if (!updated.isValid(&validationError)) {
        m_localError = validationError;
        emit stateChanged();
        return false;
    }
    const bool disablingTestMode = m_config.testMode && !updated.testMode;
    QString error;
    if (!m_store.saveConfig(updated, &error)) {
        m_localError = error;
        emit stateChanged();
        return false;
    }
    m_config = updated;
    QString schedulerError;
    if (m_scheduler) {
        if (!m_scheduler->sync(m_config.enabled, &schedulerError) && !schedulerError.isEmpty())
            m_localError = schedulerError;
    }
    if (disablingTestMode) {
        if (!m_store.clearCache(&error)) {
            m_localError = error;
            emit stateChanged();
            return false;
        }
        m_prices.clear();
        m_state = QJsonObject();
    }
    m_localError = schedulerError;
    requestRefresh();
    emit configChanged();
    emit stateChanged();
    return true;
}

void SpotController::saveConfigAndRequest()
{
    QString error;
    if (!m_store.saveConfig(m_config, &error))
        m_localError = error;
    else {
        m_localError.clear();
        if (m_scheduler) {
            QString schedulerError;
            if (!m_scheduler->sync(m_config.enabled, &schedulerError) && !schedulerError.isEmpty())
                m_localError = schedulerError;
        }
        requestRefresh();
    }
    emit configChanged();
    emit stateChanged();
}

QVariantList SpotController::chartPoints(const QString &range) const
{
    if (m_prices.isEmpty()) return {};
    const int dayCount = range == QStringLiteral("week") ? 7 : 31;
    const QDate today = QDateTime::currentDateTimeUtc().toTimeZone(kHelsinki).date();
    const QDate firstDate = today.addDays(1 - dayCount);
    const QDateTime cutoff(firstDate, QTime(0, 0), kHelsinki);
    const QDateTime end(today.addDays(1), QTime(0, 0), kHelsinki);

    QMap<qint64, Aggregate> aggregates;
    for (const PricePoint &point : m_prices) {
        if (point.startUtc < cutoff.toUTC() || point.startUtc >= end.toUTC())
            continue;
        const QDate date = point.startUtc.toTimeZone(kHelsinki).date();
        const QDateTime dayStart(date, QTime(0, 0), kHelsinki);
        const qint64 key = dayStart.toMSecsSinceEpoch();
        Aggregate &aggregate = aggregates[key];
        aggregate.x = key;
        aggregate.endX = QDateTime(date.addDays(1), QTime(0, 0), kHelsinki).toMSecsSinceEpoch();
        const PriceParts parts = priceParts(point, m_config);
        const double value = parts.total();
        aggregate.sum += value;
        aggregate.min = qMin(aggregate.min, value);
        aggregate.max = qMax(aggregate.max, value);
        aggregate.spotSum += parts.spot;
        aggregate.vatSum += parts.vat;
        aggregate.transferSum += parts.transfer;
        aggregate.electricityTaxSum += parts.electricityTax;
        ++aggregate.count;
    }

    QVariantList result;
    for (const Aggregate &aggregate : aggregates) {
        if (aggregate.count == 0) continue;
        const PriceParts parts {
            aggregate.spotSum / aggregate.count,
            aggregate.vatSum / aggregate.count,
            aggregate.transferSum / aggregate.count,
            aggregate.electricityTaxSum / aggregate.count
        };
        result.append(chartPoint(aggregate.x, aggregate.endX, parts,
                                 aggregate.min, aggregate.max));
    }
    return result;
}

QVariantMap SpotController::chartHourWindow() const
{
    const QDateTime now = QDateTime::currentDateTimeUtc();
    const QDateTime currentHour(now.date(), QTime(now.time().hour(), 0), Qt::UTC);
    const QDateTime start = currentHour.addSecs(-2 * 60 * 60);
    const QDateTime end = currentHour.addSecs(3 * 60 * 60);
    QVariantList points;
    for (const PricePoint &point : m_prices) {
        if (point.startUtc < start || point.startUtc >= end)
            continue;
        const PriceParts parts = priceParts(point, m_config);
        points.append(chartPoint(point.startUtc.toMSecsSinceEpoch(),
                                 point.startUtc.addSecs(15 * 60).toMSecsSinceEpoch(),
                                 parts, parts.total(), parts.total()));
    }
    return {
        { QStringLiteral("points"), points },
        { QStringLiteral("startMs"), start.toMSecsSinceEpoch() },
        { QStringLiteral("endMs"), end.toMSecsSinceEpoch() }
    };
}

QVariantMap SpotController::chartWindow(int pageOffset) const
{
    const QDate today = QDateTime::currentDateTimeUtc().toTimeZone(kHelsinki).date();
    const QDate date = today.addDays(pageOffset);
    const QDateTime start(date, QTime(0, 0), kHelsinki);
    const QDateTime end(date.addDays(1), QTime(0, 0), kHelsinki);
    QMap<qint64, Aggregate> aggregates;
    for (const PricePoint &point : m_prices) {
        if (point.startUtc < start.toUTC() || point.startUtc >= end.toUTC())
            continue;
        const QDateTime hour = PriceData::hourStartUtc(point.startUtc);
        const qint64 key = hour.toMSecsSinceEpoch();
        Aggregate &aggregate = aggregates[key];
        aggregate.x = key;
        aggregate.endX = qMin(hour.addSecs(60 * 60).toMSecsSinceEpoch(), end.toMSecsSinceEpoch());
        const PriceParts parts = priceParts(point, m_config);
        const double value = parts.total();
        aggregate.sum += value;
        aggregate.min = qMin(aggregate.min, value);
        aggregate.max = qMax(aggregate.max, value);
        aggregate.spotSum += parts.spot;
        aggregate.vatSum += parts.vat;
        aggregate.transferSum += parts.transfer;
        aggregate.electricityTaxSum += parts.electricityTax;
        ++aggregate.count;
    }
    QVariantList points;
    for (const Aggregate &aggregate : aggregates) {
        if (aggregate.count == 0)
            continue;
        const PriceParts parts {
            aggregate.spotSum / aggregate.count,
            aggregate.vatSum / aggregate.count,
            aggregate.transferSum / aggregate.count,
            aggregate.electricityTaxSum / aggregate.count
        };
        points.append(chartPoint(aggregate.x, aggregate.endX, parts,
                                 aggregate.min, aggregate.max));
    }
    return {
        { QStringLiteral("points"), points },
        { QStringLiteral("startMs"), start.toMSecsSinceEpoch() },
        { QStringLiteral("endMs"), end.toMSecsSinceEpoch() },
        { QStringLiteral("rangeText"), QLocale().toString(date, QLocale::LongFormat) },
        { QStringLiteral("hasPrevious"), !m_prices.isEmpty() && m_prices.first().startUtc < start.toUTC() },
        { QStringLiteral("hasNext"), !m_prices.isEmpty() && m_prices.last().startUtc >= end.toUTC() }
    };
}

QVariantList SpotController::hourlyRows(const QList<QDateTime> &starts) const
{
    QVariantList result;
    const qint64 currentQuarter = PriceData::quarterStart(QDateTime::currentDateTimeUtc())
            .toMSecsSinceEpoch();
    for (const QDateTime &start : starts) {
        double sum = 0.0;
        double spotSum = 0.0;
        double transferSum = 0.0;
        QVariantList quarters;
        bool current = false;
        for (int quarter = 0; quarter < 4; ++quarter) {
            const qint64 key = start.addSecs(quarter * 900).toMSecsSinceEpoch();
            if (!m_prices.contains(key))
                continue;
            const PricePoint point = m_prices.value(key);
            const PriceParts parts = priceParts(point, m_config);
            const double spot = parts.spot + parts.vat;
            const double transfer = parts.transfer + parts.electricityTax;
            const double price = parts.total();
            const bool isCurrent = key == currentQuarter;
            sum += price;
            spotSum += spot;
            transferSum += transfer;
            current = current || isCurrent;
            quarters.append(QVariantMap {
                { QStringLiteral("time"), timeRange(point.startUtc, 900) },
                { QStringLiteral("spot"), priceText(spot) },
                { QStringLiteral("transfer"), priceText(transfer) },
                { QStringLiteral("price"), priceText(price) },
                { QStringLiteral("current"), isCurrent }
            });
        }
        if (quarters.isEmpty())
            continue;
        const QDateTime localStart = start.toLocalTime();
        const QString localKey = localStart.date().toString(Qt::ISODate)
                + QStringLiteral("T") + localStart.time().toString(QStringLiteral("HH:mm"));
        result.append(QVariantMap {
            { QStringLiteral("time"), timeRange(start, 3600) },
            { QStringLiteral("dateKey"), localStart.date().toString(Qt::ISODate) },
            { QStringLiteral("dateText"), QLocale().toString(localStart.date(), QLocale::LongFormat) },
            { QStringLiteral("localKey"), localKey },
            { QStringLiteral("spot"), priceText(spotSum / quarters.size()) },
            { QStringLiteral("transfer"), priceText(transferSum / quarters.size()) },
            { QStringLiteral("price"), priceText(sum / quarters.size()) },
            { QStringLiteral("current"), current },
            { QStringLiteral("quarters"), quarters },
            { QStringLiteral("ambiguous"), false },
            { QStringLiteral("offset"), utcOffsetText(localStart) }
        });
    }
    QMap<QString, int> localCounts;
    for (const QVariant &entry : result)
        ++localCounts[entry.toMap().value(QStringLiteral("localKey")).toString()];
    for (int i = 0; i < result.size(); ++i) {
        QVariantMap entry = result.at(i).toMap();
        entry.insert(QStringLiteral("ambiguous"),
                     localCounts.value(entry.value(QStringLiteral("localKey")).toString()) > 1);
        entry.remove(QStringLiteral("localKey"));
        result[i] = entry;
    }
    return result;
}

QVariantList SpotController::upcomingHourlyAverages(int maximumHours) const
{
    QList<QDateTime> starts;
    const QDateTime first = PriceData::hourStartUtc(QDateTime::currentDateTimeUtc());
    for (int hour = 0; hour < qBound(0, maximumHours, 168); ++hour) {
        const QDateTime start = first.addSecs(hour * 3600);
        if (!m_prices.isEmpty() && start > m_prices.last().startUtc)
            break;
        starts.append(start);
    }
    return hourlyRows(starts);
}

QVariantMap SpotController::priceDay(int pageOffset) const
{
    QMap<QDate, QVector<PricePoint>> byDate;
    for (const PricePoint &point : m_prices)
        byDate[point.startUtc.toLocalTime().date()].append(point);
    if (byDate.isEmpty())
        return { { QStringLiteral("rows"), QVariantList() } };

    const QList<QDate> dates = byDate.keys();
    const QDate today = QDate::currentDate();
    int baseIndex = dates.indexOf(today);
    if (baseIndex < 0) {
        baseIndex = 0;
        for (int i = 0; i < dates.size(); ++i) {
            if (dates.at(i) <= today)
                baseIndex = i;
        }
    }
    const int index = qBound(0, baseIndex + pageOffset, dates.size() - 1);
    const QDate date = dates.at(index);
    const QVector<PricePoint> dayPoints = byDate.value(date);
    QMap<qint64, QDateTime> byHour;
    for (const PricePoint &point : dayPoints) {
        const QDateTime hour = PriceData::hourStartUtc(point.startUtc);
        byHour.insert(hour.toMSecsSinceEpoch(), hour);
    }
    QList<QDateTime> hourStarts;
    for (auto hourIt = byHour.constBegin(); hourIt != byHour.constEnd(); ++hourIt)
        hourStarts.append(hourIt.value());
    const QVariantList rows = hourlyRows(hourStarts);
    return {
        { QStringLiteral("isToday"), date == today },
        { QStringLiteral("rows"), rows },
        { QStringLiteral("hasPrevious"), index > 0 },
        { QStringLiteral("hasNext"), index + 1 < dates.size() },
        { QStringLiteral("resolvedOffset"), index - baseIndex }
    };
}
