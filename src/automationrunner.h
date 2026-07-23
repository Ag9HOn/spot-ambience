#pragma once

#include "appconfig.h"
#include "pricedata.h"
#include "pricestore.h"

#include <QJsonObject>
#include <QMap>
#include <QObject>

class AutomationRunner : public QObject
{
    Q_OBJECT
public:
    explicit AutomationRunner(QObject *parent = nullptr);
    int run(bool forceRefresh = false);

    static QString normalizeAmbienceUrl(const QString &url);
    // The public QML ambience model persists its authoritative inventory for
    // the short-lived worker.  A sandboxed worker cannot always stat the
    // system ambience directory itself, so a known inventory entry is also a
    // valid availability signal.
    static bool mappingAvailable(const QString &url, const QVariantList &inventory = {});
    static bool isOverrideActive(const AppConfig &config, const QTime &localTime);
    static QString resolveEffectiveTarget(const QString &band,
                                          const AppConfig &config,
                                          const QString &current,
                                          QString *warning = nullptr,
                                          const QVariantList &inventory = {});

private:
    struct FetchResult {
        bool attempted = false;
        bool success = false;
        QMap<qint64, PricePoint> prices;
        QString error;
    };

    FetchResult fetchPrices(const AppConfig &config,
                            const QMap<qint64, PricePoint> &existing,
                            const QDateTime &nowUtc,
                            bool initial);
    QString currentAmbience(QString *error = nullptr) const;
    bool setAmbience(const QString &url, QString *error = nullptr) const;
    int runTestCycle(const AppConfig &config,
                     const QJsonObject &previousState,
                     const QDateTime &nowUtc,
                     const QVariantList &ambienceInventory);
    void prune(QMap<qint64, PricePoint> *prices, const QDateTime &nowUtc) const;
    QJsonObject buildState(const AppConfig &config,
                           const PriceWindow &window,
                           const QMap<qint64, PricePoint> &prices,
                           const QDateTime &nowUtc,
                           const QString &configuredTarget,
                           const QString &effectiveTarget,
                           const QString &actualAmbience,
                           const QString &warning,
                           const QString &lastError,
                           const QString &priceSource,
                           const QJsonObject &previousState,
                           bool fetchedSuccessfully,
                           bool scheduledSatisfied) const;

    PriceStore m_store;
};
