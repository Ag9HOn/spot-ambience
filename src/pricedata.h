#pragma once

#include "appconfig.h"

#include <QDateTime>
#include <QJsonArray>
#include <QMap>
#include <QString>
#include <QTimeZone>
#include <QVector>

struct PricePoint
{
    QDateTime startUtc;
    double eurPerMwh = 0.0;
};

struct PriceWindow
{
    bool complete = false;
    QDateTime quarterStartUtc;
    QVector<PricePoint> points;
    double averageCentsPerKwh = 0.0;
    QString band;
};

class PriceData
{
public:
    static constexpr double FinnishVatPercent = 25.5;
    static QDateTime quarterStart(const QDateTime &utc);
    static QDateTime hourStartUtc(const QDateTime &utc);
    static QDateTime localHourStart(const QDateTime &utc,
                                    const QTimeZone &zone = QTimeZone::systemTimeZone());
    static double centsPerKwh(double eurPerMwh, bool includeVat,
                              double vatPercent = FinnishVatPercent);
    // Transfer prices and electricity tax are final c/kWh additions: they are
    // applied after the optional VAT calculation for the spot price.
    static bool isNightRate(const QDateTime &utc);
    static double personalPriceCents(const QDateTime &utc, const AppConfig &config);
    static double electricityTaxCents(const AppConfig &config);
    static double displayedCentsPerKwh(const PricePoint &point, const AppConfig &config);
    static double spotEurPerMwhForDisplayedCents(double displayedCents,
                                                  const QDateTime &utc,
                                                  const AppConfig &config);
    static QString bandForAverage(double average, const AppConfig &config);
    static PriceWindow window(const QMap<qint64, PricePoint> &prices,
                              const QDateTime &nowUtc,
                              const AppConfig &config);
    // Parses Elering LIVE's public /api/nps/price response. Values are raw
    // EUR/MWh and timestamps are UTC Unix seconds.
    static bool parseApi(const QByteArray &json,
                         QMap<qint64, PricePoint> *prices,
                         QString *error);
    static QJsonArray toJson(const QMap<qint64, PricePoint> &prices);
    static bool fromJson(const QJsonArray &array,
                         QMap<qint64, PricePoint> *prices,
                         QString *error = nullptr);
    static bool containsCompleteLocalDay(const QMap<qint64, PricePoint> &prices,
                                         const QDate &date,
                                         const QByteArray &timeZoneId = QByteArray("Europe/Helsinki"));
};
