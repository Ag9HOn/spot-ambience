#include "automationrunner.h"

#include "ambienceinventory.h"

#include <QDateTime>
#include <QDebug>
#include <QDBusInterface>
#include <QDBusMessage>
#include <QDir>
#include <QEventLoop>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QLockFile>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QTimeZone>
#include <QTimer>
#include <QUrl>
#include <QUrlQuery>

#include <MDConfItem>

namespace {
const QByteArray kHelsinki("Europe/Helsinki");
const QString kEleringLiveSource = QStringLiteral("Elering LIVE");

QString isoUtc(const QDateTime &value)
{
    return value.toUTC().toString(QStringLiteral("yyyy-MM-dd'T'HH:mm:ss.zzz'Z'"));
}

QDateTime localMidnightUtc(const QDate &date)
{
    return QDateTime(date, QTime(0, 0), QTimeZone(kHelsinki)).toUTC();
}

QString nameForTarget(const AppConfig &config, const QString &url)
{
    const QString normalized = AutomationRunner::normalizeAmbienceUrl(url);
    if (AutomationRunner::normalizeAmbienceUrl(config.overrideMapping.url) == normalized
            && !config.overrideMapping.displayName.isEmpty())
        return config.overrideMapping.displayName;
    for (const QString &band : AppConfig::bands()) {
        if (AutomationRunner::normalizeAmbienceUrl(config.mappings.value(band).url) == normalized)
            return config.mappings.value(band).displayName;
    }
    return QFileInfo(normalized).baseName();
}

struct ResolvedTarget {
    QString configured;
    QString effective;
    QString warning;
    bool overrideActive = false;
};

ResolvedTarget resolveTarget(const AppConfig &config, const QString &band,
                             const QString &current, const QDateTime &nowUtc,
                             const QVariantList &ambienceInventory)
{
    ResolvedTarget result;
    const QTime localTime = nowUtc.toTimeZone(QTimeZone(kHelsinki)).time();
    result.overrideActive = config.overrideEnabled
            && AutomationRunner::isOverrideActive(config, localTime);
    if (!result.overrideActive) {
        result.configured = config.mappings.value(band).url;
        result.effective = AutomationRunner::resolveEffectiveTarget(
                    band, config, current, &result.warning, ambienceInventory);
        return result;
    }

    result.configured = config.overrideMapping.url;
    if (AutomationRunner::mappingAvailable(result.configured, ambienceInventory)) {
        result.effective = result.configured;
        result.warning = QStringLiteral("Timed ambience override is active");
        return result;
    }
    const AmbienceMapping fallback = config.mappings.value(QStringLiteral("unknown"));
    if (AutomationRunner::mappingAvailable(fallback.url, ambienceInventory)) {
        result.effective = fallback.url;
        result.warning = QStringLiteral("Timed override ambience is unavailable; using %1")
                .arg(fallback.displayName);
    } else {
        result.effective = current;
        result.warning = current.isEmpty()
                ? QStringLiteral("Timed override and fallback ambiences are unavailable")
                : QStringLiteral("Timed override and fallback ambiences are unavailable; keeping the active ambience");
    }
    return result;
}
}

AutomationRunner::AutomationRunner(QObject *parent)
    : QObject(parent)
{
}

QString AutomationRunner::normalizeAmbienceUrl(const QString &url)
{
    const QString trimmed = url.trimmed();
    const QUrl parsed(trimmed);
    if (parsed.isLocalFile())
        return QFileInfo(parsed.toLocalFile()).absoluteFilePath();
    if (trimmed.startsWith(QLatin1Char('/')))
        return QFileInfo(trimmed).absoluteFilePath();
    return trimmed;
}

bool AutomationRunner::mappingAvailable(const QString &url, const QVariantList &inventory)
{
    const QString normalized = normalizeAmbienceUrl(url);
    if (normalized.isEmpty())
        return false;
    if (!normalized.startsWith(QLatin1Char('/')))
        return true;

    // When the caller can inspect the file directly, that is stronger than a
    // possibly stale cached entry. This also lets the GUI recover immediately
    // if a previously unavailable ambience is reinstalled.
    if (QFileInfo::exists(normalized))
        return true;

    // Sailjail may intentionally hide /usr/share/ambience from the headless
    // process even though ambienced can still use the URL.  The GUI has
    // already received this inventory through Sailfish.Ambience, so honour a
    // matching entry before trying a direct file check.
    for (const QVariant &entry : inventory) {
        const QVariantMap item = entry.toMap();
        if (normalizeAmbienceUrl(item.value(QStringLiteral("url")).toString()) == normalized)
            return item.value(QStringLiteral("available"), true).toBool();
    }
    return QFileInfo::exists(normalized);
}

bool AutomationRunner::isOverrideActive(const AppConfig &config, const QTime &localTime)
{
    if (!config.overrideEnabled || !localTime.isValid())
        return false;
    if (config.overrideStart == config.overrideEnd)
        return true;
    if (config.overrideStart < config.overrideEnd)
        return localTime >= config.overrideStart && localTime < config.overrideEnd;
    return localTime >= config.overrideStart || localTime < config.overrideEnd;
}

AutomationRunner::FetchResult AutomationRunner::fetchPrices(const AppConfig &,
                                                             const QMap<qint64, PricePoint> &existing,
                                                             const QDateTime &nowUtc,
                                                             bool initial)
{
    FetchResult result;
    result.attempted = true;
    const QDate today = nowUtc.toTimeZone(QTimeZone(kHelsinki)).date();
    const QDate startDate = initial || existing.isEmpty() ? today.addDays(-31) : today.addDays(-2);
    const QDate endDate = today.addDays(8);

    QUrl url(QStringLiteral("https://dashboard.elering.ee/api/nps/price"));
    QUrlQuery query;
    query.addQueryItem(QStringLiteral("start"), isoUtc(localMidnightUtc(startDate)));
    query.addQueryItem(QStringLiteral("end"), isoUtc(localMidnightUtc(endDate)));
    url.setQuery(query);

    QNetworkAccessManager manager;
    QNetworkRequest request(url);
    request.setRawHeader("User-Agent", "SpotAmbience/1.0 SailfishOS");
    request.setRawHeader("Accept", "application/json");
    QNetworkReply *reply = manager.get(request);
    QTimer timeout;
    timeout.setSingleShot(true);
    timeout.start(15000);
    QEventLoop loop;
    connect(reply, &QNetworkReply::finished, &loop, &QEventLoop::quit);
    connect(&timeout, &QTimer::timeout, reply, &QNetworkReply::abort);
    loop.exec();

    if (reply->error() != QNetworkReply::NoError) {
        result.error = QStringLiteral("Elering LIVE price request failed: %1").arg(reply->errorString());
        reply->deleteLater();
        return result;
    }
    const int status = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
    const QByteArray body = reply->readAll();
    reply->deleteLater();
    if (status < 200 || status >= 300) {
        result.error = QStringLiteral("Elering LIVE price request returned HTTP %1").arg(status);
        return result;
    }
    result.success = PriceData::parseApi(body, &result.prices, &result.error);
    return result;
}

QString AutomationRunner::currentAmbience(QString *error) const
{
    MDConfItem active(QStringLiteral("/desktop/jolla/theme/active_ambience"));
    QString value = active.value().toString().trimmed();
    if (value.isEmpty()) {
        if (error) *error = QStringLiteral("Could not read active ambience");
        return QString();
    }
    return normalizeAmbienceUrl(value);
}

bool AutomationRunner::setAmbience(const QString &url, QString *error) const
{
    QDBusInterface ambience(QStringLiteral("com.jolla.ambienced"),
                            QStringLiteral("/com/jolla/ambienced"),
                            QStringLiteral("com.jolla.ambienced"),
                            QDBusConnection::sessionBus());
    if (!ambience.isValid()) {
        if (error) *error = QStringLiteral("Ambience service is unavailable");
        return false;
    }
    ambience.setTimeout(5000);
    // ambienced exposes the active ambience through dconf as a local path, but
    // setAmbience() requires a URL (a bare path fails with "Protocol is
    // unknown"). Keep non-local identifiers opaque and only URL-encode local
    // files at the D-Bus boundary.
    QString callUrl = url.trimmed();
    const QString normalized = normalizeAmbienceUrl(callUrl);
    if (normalized.startsWith(QLatin1Char('/')))
        callUrl = QUrl::fromLocalFile(normalized).toString(QUrl::FullyEncoded);

    const QDBusMessage reply = ambience.call(QStringLiteral("setAmbience"), callUrl);
    if (reply.type() == QDBusMessage::ErrorMessage) {
        if (error) *error = QStringLiteral("Ambience switch failed: %1").arg(reply.errorMessage());
        return false;
    }
    return true;
}

QString AutomationRunner::resolveEffectiveTarget(const QString &band,
                                                  const AppConfig &config,
                                                  const QString &current,
                                                  QString *warning,
                                                  const QVariantList &inventory)
{
    if (warning)
        warning->clear();
    const QString configured = config.mappings.value(band).url;

    // Sailfish.Ambience occasionally reports an empty model while it is
    // refreshing. An empty inventory means that availability is unknown, not
    // that every saved mapping was removed. If the worker cannot even inspect
    // the mapping's parent directory, keep using the configured opaque URL
    // and let ambienced decide whether it can activate it. A visible parent
    // directory with a missing file is an actual removal and still follows
    // the fallback policy below.
    const QString normalizedConfigured = normalizeAmbienceUrl(configured);
    const QFileInfo configuredFile(normalizedConfigured);
    const bool unobservableLocalPath = !normalizedConfigured.isEmpty()
            && normalizedConfigured.startsWith(QLatin1Char('/'))
            && !configuredFile.dir().exists();
    if (inventory.isEmpty() && !configured.isEmpty()
            && (!normalizedConfigured.startsWith(QLatin1Char('/')) || unobservableLocalPath))
        return configured;

    if (mappingAvailable(configured, inventory))
        return configured;

    const QString unknown = config.mappings.value(QStringLiteral("unknown")).url;
    if (mappingAvailable(unknown, inventory)) {
        if (warning)
            *warning = QStringLiteral("%1 ambience is unavailable; using %2")
                    .arg(AppConfig::preferredName(band), config.mappings.value(QStringLiteral("unknown")).displayName);
        return unknown;
    }

    if (warning)
        *warning = current.isEmpty()
                ? QStringLiteral("Configured and fallback ambiences are unavailable")
                : QStringLiteral("Configured and fallback ambiences are unavailable; keeping the active ambience");
    return current;
}

void AutomationRunner::prune(QMap<qint64, PricePoint> *prices, const QDateTime &nowUtc) const
{
    const QDate today = nowUtc.toTimeZone(QTimeZone(kHelsinki)).date();
    const qint64 cutoff = localMidnightUtc(today.addDays(-31)).toMSecsSinceEpoch();
    auto it = prices->begin();
    while (it != prices->end() && it.key() < cutoff)
        it = prices->erase(it);
}

int AutomationRunner::runTestCycle(const AppConfig &config,
                                   const QJsonObject &previousState,
                                   const QDateTime &nowUtc,
                                   const QVariantList &ambienceInventory)
{
    const QStringList bands = AppConfig::bands();
    const QDateTime quarterStart = PriceData::quarterStart(nowUtc);
    const qint64 quarterIndex = quarterStart.toMSecsSinceEpoch() / (15 * 60 * 1000);
    const int phase = int((quarterIndex % bands.size() + bands.size()) % bands.size());
    const QString band = bands.at(phase);

    const QMap<QString, double> representativePrices = {
        { QStringLiteral("negative"), -1.0 },
        { QStringLiteral("cheap"), config.cheapBelow / 2.0 },
        { QStringLiteral("middle"), (config.cheapBelow + config.expensiveAbove) / 2.0 },
        { QStringLiteral("expensive"), config.expensiveAbove + qMax(5.0, config.expensiveAbove * 0.25) }
    };
    const auto eurPerMwh = [&](double displayedCents, const QDateTime &start) {
        return PriceData::spotEurPerMwhForDisplayedCents(displayedCents, start, config);
    };

    // The cache is intentionally synthetic while this mode is enabled. Every
    // fifth quarter is absent to represent the Unknown/Kaamos phase; the other
    // quarters exercise the four numeric bands in order.
    QMap<qint64, PricePoint> prices;
    const QDateTime cacheStart = quarterStart.addDays(-31);
    const QDateTime cacheEnd = quarterStart.addDays(7);
    for (QDateTime cursor = cacheStart; cursor <= cacheEnd; cursor = cursor.addSecs(900)) {
        const qint64 index = cursor.toMSecsSinceEpoch() / (15 * 60 * 1000);
        const int cursorPhase = int((index % bands.size() + bands.size()) % bands.size());
        const QString cursorBand = bands.at(cursorPhase);
        if (cursorBand == QStringLiteral("unknown"))
            continue;
        const double cents = representativePrices.value(cursorBand);
        prices.insert(cursor.toMSecsSinceEpoch(), PricePoint { cursor, eurPerMwh(cents, cursor) });
    }
    QString persistenceError;
    if (!m_store.savePrices(prices, &persistenceError))
        qWarning() << "Could not save test prices:" << persistenceError;

    PriceWindow window;
    window.quarterStartUtc = quarterStart;
    window.band = band;
    if (band != QStringLiteral("unknown")) {
        window.complete = true;
        window.averageCentsPerKwh = representativePrices.value(band);
        for (int component = 0; component < 4; ++component) {
            const QDateTime start = quarterStart.addSecs(component * 900);
            window.points.append(PricePoint { start, eurPerMwh(window.averageCentsPerKwh, start) });
        }
    }

    QString lastError = persistenceError;
    QString currentError;
    const QString current = currentAmbience(&currentError);
    if (!currentError.isEmpty() && lastError.isEmpty())
        lastError = currentError;
    const ResolvedTarget resolved = resolveTarget(config, band, current, nowUtc, ambienceInventory);
    const QString configuredTarget = resolved.configured;
    const QString target = resolved.effective;
    QString actual = current;
    if (config.enabled && !target.isEmpty()
            && normalizeAmbienceUrl(target) != normalizeAmbienceUrl(current)) {
        QString switchError;
        if (setAmbience(target, &switchError))
            actual = target;
        else
            lastError = switchError;
    }

    QString warning = QStringLiteral("TEST MODE: synthetic prices; ambience changes every 15 minutes (%1 phase)")
            .arg(AppConfig::preferredName(band));
    if (!resolved.warning.isEmpty())
        warning += QStringLiteral(". ") + resolved.warning;
    QJsonObject state = buildState(config, window, prices, nowUtc, configuredTarget, target,
                                   actual, warning, lastError, QStringLiteral("Synthetic test cycle"),
                                   previousState, false, false);
    state.insert(QStringLiteral("testMode"), true);
    state.insert(QStringLiteral("lastAttemptUtc"), isoUtc(nowUtc));
    state.insert(QStringLiteral("lastSuccessUtc"), isoUtc(nowUtc));
    if (!m_store.saveState(state, &persistenceError)) {
        qWarning() << "Could not save test state:" << persistenceError;
        return 1;
    }
    return 0;
}

QJsonObject AutomationRunner::buildState(const AppConfig &config,
                                          const PriceWindow &window,
                                          const QMap<qint64, PricePoint> &prices,
                                          const QDateTime &nowUtc,
                                          const QString &configuredTarget,
                                          const QString &effectiveTargetUrl,
                                          const QString &actualAmbience,
                                          const QString &warning,
                                          const QString &lastError,
                                          const QString &priceSource,
                                          const QJsonObject &previousState,
                                          bool fetchedSuccessfully,
                                          bool scheduledSatisfied) const
{
    QJsonArray components;
    for (const PricePoint &point : window.points) {
        const double spot = point.eurPerMwh / 10.0;
        const double vat = config.includeVat ? spot * config.vatPercent / 100.0 : 0.0;
        const double transfer = PriceData::personalPriceCents(point.startUtc, config);
        const double electricityTax = PriceData::electricityTaxCents(config);
        components.append(QJsonObject {
            { QStringLiteral("startUtc"), isoUtc(point.startUtc) },
            { QStringLiteral("centsPerKwh"), PriceData::displayedCentsPerKwh(point, config) },
            { QStringLiteral("spotCentsPerKwh"), spot + vat },
            { QStringLiteral("transferCentsPerKwh"), transfer + electricityTax }
        });
    }
    QJsonObject state {
        { QStringLiteral("version"), 1 },
        { QStringLiteral("updatedUtc"), isoUtc(nowUtc) },
        { QStringLiteral("enabled"), config.enabled },
        { QStringLiteral("complete"), window.complete },
        { QStringLiteral("quarterStartUtc"), isoUtc(window.quarterStartUtc) },
        { QStringLiteral("components"), components },
        { QStringLiteral("averageCentsPerKwh"), window.complete ? window.averageCentsPerKwh : QJsonValue() },
        { QStringLiteral("band"), window.complete ? window.band : QStringLiteral("unknown") },
        { QStringLiteral("configuredTarget"), configuredTarget },
        { QStringLiteral("effectiveTarget"), effectiveTargetUrl },
        { QStringLiteral("effectiveTargetName"), nameForTarget(config, effectiveTargetUrl) },
        { QStringLiteral("actualAmbience"), actualAmbience },
        { QStringLiteral("priceSource"), priceSource },
        { QStringLiteral("warning"), warning },
        { QStringLiteral("lastError"), lastError }
    };
    if (!prices.isEmpty()) {
        state.insert(QStringLiteral("cacheStartUtc"), isoUtc(prices.first().startUtc));
        state.insert(QStringLiteral("cacheEndUtc"), isoUtc(prices.last().startUtc.addSecs(900)));
        if (prices.last().startUtc >= nowUtc)
            state.insert(QStringLiteral("lastFutureIntervalUtc"), isoUtc(prices.last().startUtc));
    }
    const qint64 quarterMillis = 15 * 60 * 1000;
    const qint64 nextCheckMillis = ((nowUtc.toMSecsSinceEpoch() / quarterMillis) + 1) * quarterMillis;
    state.insert(QStringLiteral("nextCheckUtc"),
                 isoUtc(QDateTime::fromMSecsSinceEpoch(nextCheckMillis, Qt::UTC)));
    state.insert(QStringLiteral("lastAttemptUtc"), previousState.value(QStringLiteral("lastAttemptUtc")));
    state.insert(QStringLiteral("lastSuccessUtc"), previousState.value(QStringLiteral("lastSuccessUtc")));
    state.insert(QStringLiteral("lastScheduledDate"), previousState.value(QStringLiteral("lastScheduledDate")));
    if (fetchedSuccessfully) {
        state.insert(QStringLiteral("lastAttemptUtc"), isoUtc(nowUtc));
        state.insert(QStringLiteral("lastSuccessUtc"), isoUtc(nowUtc));
    } else if (!lastError.isEmpty()) {
        state.insert(QStringLiteral("lastAttemptUtc"), isoUtc(nowUtc));
    }
    if (scheduledSatisfied)
        state.insert(QStringLiteral("lastScheduledDate"), nowUtc.toTimeZone(QTimeZone(kHelsinki)).date().toString(Qt::ISODate));
    return state;
}

int AutomationRunner::run(bool forceRefresh)
{
    if (!QDir().mkpath(m_store.dataDirectory()))
        return 1;
    const bool forced = forceRefresh;

    QLockFile lock(m_store.lockPath());
    lock.setStaleLockTime(120000);
    if (!lock.tryLock(0))
        return 0;
    bool inventoryOk = false;
    QVariantList ambienceInventory = m_store.loadAmbiences(&inventoryOk);
    // Only use filesystem discovery to bootstrap a missing cache.  It is not
    // authoritative (it cannot enumerate all downloaded/user ambiences) and
    // must never replace the public Sailfish.Ambience inventory saved by the
    // GUI.
    if (!inventoryOk) {
        QString inventoryError;
        const QVariantList discovered = AmbienceInventory::discover(&inventoryError);
        if (!discovered.isEmpty()) {
            ambienceInventory = discovered;
            if (!m_store.saveAmbiences(ambienceInventory, &inventoryError))
                qWarning() << "Could not save bootstrap ambience inventory:" << inventoryError;
        } else {
            qWarning() << "Could not bootstrap ambience inventory:" << inventoryError;
        }
    }

    bool configOk = false;
    QString configError;
    AppConfig config = m_store.loadConfig(&configOk, &configError);
    bool configNeedsSave = !configOk;
    for (const QString &band : AppConfig::bands()) {
        AmbienceMapping &mapping = config.mappings[band];
        const QString preferred = AppConfig::preferredUrl(band);
        if (mapping.url.isEmpty() && mappingAvailable(preferred, ambienceInventory)) {
            mapping.url = preferred;
            mapping.displayName = AppConfig::preferredName(band);
            configNeedsSave = true;
        }
    }
    if (config.overrideMapping.url.isEmpty()) {
        const QString preferred = AppConfig::preferredUrl(QStringLiteral("cheap"));
        if (mappingAvailable(preferred, ambienceInventory)) {
            config.overrideMapping.url = preferred;
            config.overrideMapping.displayName = AppConfig::preferredName(QStringLiteral("cheap"));
            configNeedsSave = true;
        }
    }
    if (configNeedsSave)
        m_store.saveConfig(config);

    bool cacheOk = false;
    QString cacheError;
    QMap<qint64, PricePoint> prices = m_store.loadPrices(&cacheOk, &cacheError);
    bool stateOk = false;
    QJsonObject previousState = m_store.loadState(&stateOk);
    if (!stateOk)
        previousState = QJsonObject();

    const QDateTime nowUtc = QDateTime::currentDateTimeUtc();
    const bool environmentForcesTestMode = qgetenv("SPOTAMBIENCE_TEST_CYCLE") == QByteArray("1");
    if (environmentForcesTestMode && !config.testMode) {
        config.testMode = true;
        m_store.saveConfig(config);
    }
    if (config.testMode || environmentForcesTestMode)
        return runTestCycle(config, previousState, nowUtc, ambienceInventory);

    const QDateTime localNow = nowUtc.toTimeZone(QTimeZone(kHelsinki));
    const QString today = localNow.date().toString(Qt::ISODate);
    PriceWindow initialWindow = PriceData::window(prices, nowUtc, config);
    const bool dailyDue = localNow.time() >= config.publicationTime
            && previousState.value(QStringLiteral("lastScheduledDate")).toString() != today;
    const bool sourceChanged = previousState.value(QStringLiteral("priceSource")).toString()
            != kEleringLiveSource;
    const bool shouldFetch = forced || !initialWindow.complete || prices.isEmpty() || dailyDue
            || sourceChanged;

    QString lastError;
    bool fetchedSuccessfully = false;
    bool scheduledSatisfied = false;
    if (shouldFetch) {
        const FetchResult fetch = fetchPrices(config, prices, nowUtc, prices.isEmpty() || sourceChanged);
        if (fetch.success) {
            if (sourceChanged) {
                prices = fetch.prices;
            } else {
                for (auto it = fetch.prices.constBegin(); it != fetch.prices.constEnd(); ++it)
                    prices.insert(it.key(), it.value());
            }
            fetchedSuccessfully = true;
            scheduledSatisfied = dailyDue && PriceData::containsCompleteLocalDay(prices, localNow.date().addDays(1));
            if (dailyDue && !scheduledSatisfied)
                lastError = QStringLiteral("Tomorrow's price data is not complete yet");
        } else {
            lastError = fetch.error;
        }
    }

    prune(&prices, nowUtc);
    if (!prices.isEmpty()) {
        QString persistenceError;
        if (!m_store.savePrices(prices, &persistenceError) && lastError.isEmpty())
            lastError = QStringLiteral("Could not save price cache: %1").arg(persistenceError);
    }
    const PriceWindow window = PriceData::window(prices, nowUtc, config);

    QString currentError;
    const QString current = currentAmbience(&currentError);
    if (!currentError.isEmpty() && lastError.isEmpty())
        lastError = currentError;
    const QString band = window.complete ? window.band : QStringLiteral("unknown");
    const ResolvedTarget resolved = resolveTarget(config, band, current, nowUtc, ambienceInventory);
    const QString configuredTarget = resolved.configured;
    const QString target = resolved.effective;
    const QString warning = resolved.warning;
    QString actual = current;
    if (config.enabled && !target.isEmpty() && normalizeAmbienceUrl(target) != normalizeAmbienceUrl(current)) {
        QString switchError;
        if (setAmbience(target, &switchError))
            actual = target;
        else if (lastError.isEmpty())
            lastError = switchError;
    }

    const QString priceSource = fetchedSuccessfully ? kEleringLiveSource
            : previousState.value(QStringLiteral("priceSource")).toString();
    const QJsonObject state = buildState(config, window, prices, nowUtc, configuredTarget, target,
                                         actual, warning, lastError, priceSource, previousState,
                                         fetchedSuccessfully, scheduledSatisfied);
    QString stateError;
    if (!m_store.saveState(state, &stateError)) {
        qWarning() << "Could not save price state:" << stateError;
        return 1;
    }
    return 0;
}
