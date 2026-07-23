#include <QtTest>

#include "appconfig.h"
#include "automationrunner.h"
#include "pricedata.h"
#include "timedscheduler.h"

#include <QJsonDocument>
#include <QJsonObject>
#include <QFile>
#include <QTemporaryDir>
#include <QTimeZone>
#include <QUrl>

class CoreTests : public QObject
{
    Q_OBJECT
private slots:
    void conversion();
    void transferPriceAndElectricityTax();
    void bandBoundaries();
    void fourQuarterWindow();
    void incompleteWindow();
    void apiParsing();
    void apiRejectsConflictingDuplicates();
    void apiRejectsMissingFinnishData();
    void apiRejectsTimestampOutsideQt5Range();
    void dstDayCoverage();
    void configurationRoundTrip();
    void timedOverrideWindow();
    void ambienceResolutionAndReinstallation();
    void ambienceResolutionUsesCachedPublicInventory();
    void ambienceResolutionDoesNotTreatEmptyInventoryAsRemoval();
    void timedEventWireSignature();
    void timedNextQuarterBoundary();
    void localHourGrouping();
    void repeatedDstHoursRemainDistinct();
};

void CoreTests::conversion()
{
    QCOMPARE(PriceData::centsPerKwh(50.0, false), 5.0);
    QCOMPARE(PriceData::centsPerKwh(-10.0, false), -1.0);
    QCOMPARE(PriceData::centsPerKwh(50.0, true), 6.275);
    QCOMPARE(PriceData::centsPerKwh(50.0, true, 10.0), 5.5);
}

void CoreTests::transferPriceAndElectricityTax()
{
    AppConfig config = AppConfig::defaultsForInstalledFiles();
    config.personalPriceEnabled = true;
    config.personalPriceFlat = 2.5;
    config.electricityTax = 2.91788;
    const QDateTime daytime = QDateTime(QDate(2026, 7, 23), QTime(12, 0),
                                        QTimeZone::systemTimeZone()).toUTC();
    QCOMPARE(PriceData::personalPriceCents(daytime, config), 2.5);
    QCOMPARE(PriceData::electricityTaxCents(config), 2.91788);
    QCOMPARE(PriceData::displayedCentsPerKwh(PricePoint { daytime, 10.0 }, config), 6.41788);

    config.personalPriceDayNight = true;
    config.personalPriceDay = 1.0;
    config.personalPriceNight = 3.0;
    const QDateTime dayStart = QDateTime(QDate(2026, 7, 23), QTime(7, 0),
                                         QTimeZone::systemTimeZone()).toUTC();
    const QDateTime nightStart = QDateTime(QDate(2026, 7, 23), QTime(22, 0),
                                           QTimeZone::systemTimeZone()).toUTC();
    const QDateTime finalNight = QDateTime(QDate(2026, 7, 24), QTime(6, 59),
                                           QTimeZone::systemTimeZone()).toUTC();
    QCOMPARE(PriceData::personalPriceCents(dayStart, config), 1.0);
    QCOMPARE(PriceData::personalPriceCents(nightStart, config), 3.0);
    QCOMPARE(PriceData::personalPriceCents(finalNight, config), 3.0);
    QCOMPARE(PriceData::spotEurPerMwhForDisplayedCents(7.91788, nightStart, config), 20.0);
}

void CoreTests::bandBoundaries()
{
    const AppConfig config = AppConfig::defaultsForInstalledFiles();
    QCOMPARE(PriceData::bandForAverage(-0.001, config), QStringLiteral("negative"));
    QCOMPARE(PriceData::bandForAverage(0.0, config), QStringLiteral("cheap"));
    QCOMPARE(PriceData::bandForAverage(4.999, config), QStringLiteral("cheap"));
    QCOMPARE(PriceData::bandForAverage(5.0, config), QStringLiteral("middle"));
    QCOMPARE(PriceData::bandForAverage(15.0, config), QStringLiteral("middle"));
    QCOMPARE(PriceData::bandForAverage(15.001, config), QStringLiteral("expensive"));
}

void CoreTests::fourQuarterWindow()
{
    const QDateTime start(QDate(2026, 7, 22), QTime(10, 0), Qt::UTC);
    QMap<qint64, PricePoint> prices;
    const QVector<double> values { 10.0, 20.0, 30.0, 40.0 };
    for (int i = 0; i < values.size(); ++i) {
        const QDateTime time = start.addSecs(i * 900);
        prices.insert(time.toMSecsSinceEpoch(), PricePoint { time, values.at(i) });
    }
    const PriceWindow window = PriceData::window(prices, start.addSecs(421), AppConfig::defaultsForInstalledFiles());
    QVERIFY(window.complete);
    QCOMPARE(window.points.size(), 4);
    QCOMPARE(window.averageCentsPerKwh, 2.5);
    QCOMPARE(window.band, QStringLiteral("cheap"));
}

void CoreTests::incompleteWindow()
{
    const QDateTime start(QDate(2026, 7, 22), QTime(10, 0), Qt::UTC);
    QMap<qint64, PricePoint> prices;
    prices.insert(start.toMSecsSinceEpoch(), PricePoint { start, 10.0 });
    const PriceWindow window = PriceData::window(prices, start, AppConfig::defaultsForInstalledFiles());
    QVERIFY(!window.complete);
    QCOMPARE(window.band, QStringLiteral("unknown"));
}

void CoreTests::apiParsing()
{
    const QByteArray json = R"({"success":true,"data":{"ee":[],"fi":[
        {"timestamp":1784714400,"price":10.25},
        {"timestamp":1784714400,"price":10.25},
        {"timestamp":1784715300,"price":-2.5}
    ]}})";
    QMap<qint64, PricePoint> prices;
    QString error;
    QVERIFY2(PriceData::parseApi(json, &prices, &error), qPrintable(error));
    QCOMPARE(prices.size(), 2);
    QCOMPARE(prices.first().eurPerMwh, 10.25);
}

void CoreTests::apiRejectsConflictingDuplicates()
{
    const QByteArray json = R"({"success":true,"data":{"fi":[
        {"timestamp":1784714400,"price":10.0},
        {"timestamp":1784714400,"price":11.0}
    ]}})";
    QMap<qint64, PricePoint> prices;
    QString error;
    QVERIFY(!PriceData::parseApi(json, &prices, &error));
    QVERIFY(error.contains(QStringLiteral("duplicate")));
}

void CoreTests::apiRejectsMissingFinnishData()
{
    const QByteArray json = R"({"success":true,"data":{"ee":[
        {"timestamp":1784714400,"price":10.0}
    ]}})";
    QMap<qint64, PricePoint> prices;
    QString error;
    QVERIFY(!PriceData::parseApi(json, &prices, &error));
    QVERIFY(error.contains(QStringLiteral("Finnish")));
}

void CoreTests::apiRejectsTimestampOutsideQt5Range()
{
    const QByteArray json = R"({"success":true,"data":{"fi":[
        {"timestamp":4294967400,"price":10.0}
    ]}})";
    QMap<qint64, PricePoint> prices;
    QString error;
    QVERIFY(!PriceData::parseApi(json, &prices, &error));
    QVERIFY(error.contains(QStringLiteral("invalid")));
}

void CoreTests::dstDayCoverage()
{
    const QTimeZone helsinki(QByteArray("Europe/Helsinki"));
    for (const QDate &date : { QDate(2026, 3, 29), QDate(2026, 10, 25) }) {
        const QDateTime start(date, QTime(0, 0), helsinki);
        const QDateTime end(date.addDays(1), QTime(0, 0), helsinki);
        QMap<qint64, PricePoint> prices;
        for (QDateTime cursor = start.toUTC(); cursor < end.toUTC(); cursor = cursor.addSecs(900))
            prices.insert(cursor.toMSecsSinceEpoch(), PricePoint { cursor, 1.0 });
        QVERIFY(PriceData::containsCompleteLocalDay(prices, date));
        QCOMPARE(prices.size(), date.month() == 3 ? 92 : 100);
        prices.remove(prices.firstKey());
        QVERIFY(!PriceData::containsCompleteLocalDay(prices, date));
    }
}

void CoreTests::configurationRoundTrip()
{
    AppConfig input = AppConfig::defaultsForInstalledFiles();
    input.enabled = false;
    input.testMode = true;
    input.includeVat = true;
    input.vatPercent = 24.0;
    input.personalPriceEnabled = true;
    input.personalPriceDayNight = true;
    input.personalPriceFlat = 1.0;
    input.personalPriceDay = 2.0;
    input.personalPriceNight = 3.0;
    input.electricityTax = 2.91788;
    input.cheapBelow = 4.5;
    input.expensiveAbove = 17.5;
    input.publicationTime = QTime(14, 30);
    input.overrideEnabled = true;
    input.overrideStart = QTime(23, 0);
    input.overrideEnd = QTime(7, 30);
    input.overrideMapping = { QStringLiteral("file:///tmp/night.ambience"), QStringLiteral("Night") };
    input.mappings[QStringLiteral("cheap")] = { QStringLiteral("file:///tmp/blue.ambience"), QStringLiteral("Blue") };
    bool ok = false;
    const AppConfig output = AppConfig::fromJson(input.toJson(), &ok);
    QVERIFY(ok);
    QCOMPARE(output.enabled, false);
    QCOMPARE(output.testMode, true);
    QCOMPARE(output.includeVat, true);
    QCOMPARE(output.vatPercent, 24.0);
    QCOMPARE(output.personalPriceEnabled, true);
    QCOMPARE(output.personalPriceDayNight, true);
    QCOMPARE(output.personalPriceFlat, 1.0);
    QCOMPARE(output.personalPriceDay, 2.0);
    QCOMPARE(output.personalPriceNight, 3.0);
    QCOMPARE(output.electricityTax, 2.91788);
    QCOMPARE(output.cheapBelow, 4.5);
    QCOMPARE(output.expensiveAbove, 17.5);
    QCOMPARE(output.publicationTime, QTime(14, 30));
    QCOMPARE(output.overrideEnabled, true);
    QCOMPARE(output.overrideStart, QTime(23, 0));
    QCOMPARE(output.overrideEnd, QTime(7, 30));
    QCOMPARE(output.overrideMapping.displayName, QStringLiteral("Night"));
    QCOMPARE(output.mappings.value(QStringLiteral("cheap")).displayName, QStringLiteral("Blue"));
}

void CoreTests::timedOverrideWindow()
{
    AppConfig config = AppConfig::defaultsForInstalledFiles();
    config.overrideEnabled = true;
    config.overrideStart = QTime(0, 0);
    config.overrideEnd = QTime(8, 0);
    QVERIFY(AutomationRunner::isOverrideActive(config, QTime(0, 0)));
    QVERIFY(AutomationRunner::isOverrideActive(config, QTime(7, 59)));
    QVERIFY(!AutomationRunner::isOverrideActive(config, QTime(8, 0)));
    config.overrideStart = QTime(22, 0);
    config.overrideEnd = QTime(6, 0);
    QVERIFY(AutomationRunner::isOverrideActive(config, QTime(23, 0)));
    QVERIFY(AutomationRunner::isOverrideActive(config, QTime(5, 59)));
    QVERIFY(!AutomationRunner::isOverrideActive(config, QTime(12, 0)));
    config.overrideEnabled = false;
    QVERIFY(!AutomationRunner::isOverrideActive(config, QTime(23, 0)));
}

void CoreTests::ambienceResolutionAndReinstallation()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const QString unknownPath = directory.path() + QStringLiteral("/unknown.ambience");
    const QString cheapPath = directory.path() + QStringLiteral("/cheap.ambience");
    for (const QString &path : { unknownPath, cheapPath }) {
        QFile file(path);
        QVERIFY(file.open(QIODevice::WriteOnly));
    }

    AppConfig config = AppConfig::defaultsForInstalledFiles();
    config.mappings[QStringLiteral("unknown")] = { QUrl::fromLocalFile(unknownPath).toString(), QStringLiteral("Fallback") };
    config.mappings[QStringLiteral("cheap")] = { QUrl::fromLocalFile(cheapPath).toString(), QStringLiteral("Cheap") };
    QString warning;
    QCOMPARE(AutomationRunner::resolveEffectiveTarget(QStringLiteral("cheap"), config,
                                                       QStringLiteral("/active.ambience"), &warning),
             config.mappings.value(QStringLiteral("cheap")).url);
    QVERIFY(warning.isEmpty());

    QVERIFY(QFile::remove(cheapPath));
    QCOMPARE(AutomationRunner::resolveEffectiveTarget(QStringLiteral("cheap"), config,
                                                       QStringLiteral("/active.ambience"), &warning),
             config.mappings.value(QStringLiteral("unknown")).url);
    QVERIFY(!warning.isEmpty());
    QCOMPARE(config.mappings.value(QStringLiteral("cheap")).url,
             QUrl::fromLocalFile(cheapPath).toString());

    QVERIFY(QFile::remove(unknownPath));
    QCOMPARE(AutomationRunner::resolveEffectiveTarget(QStringLiteral("cheap"), config,
                                                       QStringLiteral("/active.ambience"), &warning),
             QStringLiteral("/active.ambience"));
    QCOMPARE(AutomationRunner::resolveEffectiveTarget(QStringLiteral("cheap"), config,
                                                       QString(), &warning), QString());

    QFile restored(cheapPath);
    QVERIFY(restored.open(QIODevice::WriteOnly));
    restored.close();
    QCOMPARE(AutomationRunner::resolveEffectiveTarget(QStringLiteral("cheap"), config,
                                                       QStringLiteral("/active.ambience"), &warning),
             config.mappings.value(QStringLiteral("cheap")).url);

    const QString opaqueUrl = QStringLiteral("ambience-library://downloaded/example");
    config.mappings[QStringLiteral("cheap")].url = opaqueUrl;
    QVERIFY(AutomationRunner::mappingAvailable(opaqueUrl));
    QCOMPARE(AutomationRunner::resolveEffectiveTarget(QStringLiteral("cheap"), config,
                                                       QStringLiteral("/active.ambience"), &warning),
             opaqueUrl);
}

void CoreTests::ambienceResolutionUsesCachedPublicInventory()
{
    // A short-lived Sailjail worker may not be allowed to inspect
    // /usr/share/ambience, while the GUI has already enumerated it through
    // Sailfish.Ambience. The saved public inventory must therefore be enough
    // to resolve a mapped local URL.
    AppConfig config = AppConfig::defaultsForInstalledFiles();
    const QString configured = QStringLiteral("/sandbox-hidden/cheap.ambience");
    const QString fallback = QStringLiteral("/sandbox-hidden/unknown.ambience");
    config.mappings[QStringLiteral("cheap")] = { configured, QStringLiteral("Cheap") };
    config.mappings[QStringLiteral("unknown")] = { fallback, QStringLiteral("Fallback") };
    const QVariantList inventory {
        QVariantMap {{ QStringLiteral("url"), configured }, { QStringLiteral("available"), true }},
        QVariantMap {{ QStringLiteral("url"), fallback }, { QStringLiteral("available"), true }}
    };
    QString warning;
    QCOMPARE(AutomationRunner::resolveEffectiveTarget(QStringLiteral("cheap"), config,
                                                       QStringLiteral("/active.ambience"), &warning,
                                                       inventory), configured);
    QVERIFY(warning.isEmpty());

    QVariantList unavailableInventory = inventory;
    QVariantMap unavailable = unavailableInventory.first().toMap();
    unavailable.insert(QStringLiteral("available"), false);
    unavailableInventory[0] = unavailable;
    QCOMPARE(AutomationRunner::resolveEffectiveTarget(QStringLiteral("cheap"), config,
                                                       QStringLiteral("/active.ambience"), &warning,
                                                       unavailableInventory), fallback);
    QVERIFY(!warning.isEmpty());
}

void CoreTests::ambienceResolutionDoesNotTreatEmptyInventoryAsRemoval()
{
    // A refreshing Sailfish.Ambience model can temporarily have no entries.
    // That absence is not evidence that configured opaque URLs were removed.
    AppConfig config = AppConfig::defaultsForInstalledFiles();
    const QString configured = QStringLiteral("/sandbox-hidden/cheap.ambience");
    config.mappings[QStringLiteral("cheap")] = { configured, QStringLiteral("Cheap") };

    QString warning;
    QCOMPARE(AutomationRunner::resolveEffectiveTarget(QStringLiteral("cheap"), config,
                                                       QStringLiteral("/active.ambience"), &warning,
                                                       {}), configured);
    QVERIFY(warning.isEmpty());
}

void CoreTests::timedEventWireSignature()
{
    QCOMPARE(TimedScheduler::timedEventSignature(), QStringLiteral(
        "(iuuuuus(a{ss})ua((a{ss})u)a(u(a{ss})a(sb))a(tuuuuu)iia(sb))"));
    QCOMPARE(TimedScheduler::timedActionFlags(), quint32((1u << 4) | (1u << 10)));
}

void CoreTests::timedNextQuarterBoundary()
{
    const QDateTime fortyFive(QDate(2026, 7, 23), QTime(9, 45), Qt::UTC);
    QCOMPARE(TimedScheduler::nextQuarterUtcAfter(fortyFive),
             QDateTime(QDate(2026, 7, 23), QTime(10, 0), Qt::UTC));

    const QDateTime finalQuarter(QDate(2026, 7, 23), QTime(20, 45), Qt::UTC);
    QCOMPARE(TimedScheduler::nextQuarterUtcAfter(finalQuarter),
             QDateTime(QDate(2026, 7, 23), QTime(21, 0), Qt::UTC));
}

void CoreTests::localHourGrouping()
{
    const QTimeZone helsinki(QByteArray("Europe/Helsinki"));
    const QDateTime instant(QDate(2026, 7, 23), QTime(7, 42), Qt::UTC);
    const QDateTime hour = PriceData::localHourStart(instant, helsinki);
    QCOMPARE(hour.time(), QTime(10, 0));
    QCOMPARE(hour.toUTC().addSecs(3600).toTimeZone(helsinki).time(), QTime(11, 0));
    for (int quarter = 0; quarter < 4; ++quarter)
        QCOMPARE(hour.addSecs(quarter * 900).time(), QTime(10, quarter * 15));
}

void CoreTests::repeatedDstHoursRemainDistinct()
{
    // Helsinki returns from UTC+3 to UTC+2 on this date. Both instants render
    // as 03:15 locally, but they must remain separate hourly rows/bars.
    const QTimeZone helsinki(QByteArray("Europe/Helsinki"));
    const QDateTime daylightHour(QDate(2026, 10, 25), QTime(0, 15), Qt::UTC);
    const QDateTime standardHour(QDate(2026, 10, 25), QTime(1, 15), Qt::UTC);
    QCOMPARE(daylightHour.toTimeZone(helsinki).time(), QTime(3, 15));
    QCOMPARE(standardHour.toTimeZone(helsinki).time(), QTime(3, 15));
    const QDateTime daylightStart = PriceData::hourStartUtc(daylightHour);
    const QDateTime standardStart = PriceData::hourStartUtc(standardHour);
    QVERIFY(daylightStart != standardStart);
    QCOMPARE(daylightStart, QDateTime(QDate(2026, 10, 25), QTime(0, 0), Qt::UTC));
    QCOMPARE(standardStart, QDateTime(QDate(2026, 10, 25), QTime(1, 0), Qt::UTC));
}

QTEST_MAIN(CoreTests)
#include "test_core.moc"
