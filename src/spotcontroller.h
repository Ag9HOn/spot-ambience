#pragma once

#include "appconfig.h"
#include "pricedata.h"
#include "pricestore.h"

#include <QDateTime>
#include <QJsonObject>
#include <QList>
#include <QObject>
#include <QVariantList>

class SpotController : public QObject
{
    Q_OBJECT
    Q_PROPERTY(bool enabled READ enabled WRITE setEnabled NOTIFY configChanged)
    Q_PROPERTY(bool testMode READ testMode WRITE setTestMode NOTIFY configChanged)
    Q_PROPERTY(bool includeVat READ includeVat WRITE setIncludeVat NOTIFY configChanged)
    Q_PROPERTY(double vatPercent READ vatPercent WRITE setVatPercent NOTIFY configChanged)
    Q_PROPERTY(double cheapBelow READ cheapBelow WRITE setCheapBelow NOTIFY configChanged)
    Q_PROPERTY(double expensiveAbove READ expensiveAbove WRITE setExpensiveAbove NOTIFY configChanged)
    Q_PROPERTY(QString publicationTime READ publicationTime WRITE setPublicationTime NOTIFY configChanged)
    Q_PROPERTY(QString averageText READ averageText NOTIFY stateChanged)
    Q_PROPERTY(QString currentPriceText READ currentPriceText NOTIFY stateChanged)
    Q_PROPERTY(QString coverAverageText READ coverAverageText NOTIFY stateChanged)
    Q_PROPERTY(QString coverPriceText READ coverPriceText NOTIFY stateChanged)
    Q_PROPERTY(QString bandKey READ bandKey NOTIFY stateChanged)
    Q_PROPERTY(QString bandText READ bandText NOTIFY stateChanged)
    Q_PROPERTY(QString effectiveAmbienceName READ effectiveAmbienceName NOTIFY stateChanged)
    Q_PROPERTY(QString cacheRangeText READ cacheRangeText NOTIFY stateChanged)
    Q_PROPERTY(QString lastRefreshText READ lastRefreshText NOTIFY stateChanged)
    Q_PROPERTY(QString nextRefreshText READ nextRefreshText NOTIFY stateChanged)
    Q_PROPERTY(QString errorText READ errorText NOTIFY stateChanged)
    Q_PROPERTY(QString warningText READ warningText NOTIFY stateChanged)
    Q_PROPERTY(QVariantList quarterValues READ quarterValues NOTIFY stateChanged)
    Q_PROPERTY(QVariantList availableAmbiences READ availableAmbiences NOTIFY ambiencesChanged)
public:
    explicit SpotController(QObject *parent = nullptr, class TimedScheduler *scheduler = nullptr);

    bool enabled() const;
    void setEnabled(bool value);
    bool testMode() const;
    void setTestMode(bool value);
    bool includeVat() const;
    void setIncludeVat(bool value);
    double vatPercent() const;
    void setVatPercent(double value);
    double cheapBelow() const;
    void setCheapBelow(double value);
    double expensiveAbove() const;
    void setExpensiveAbove(double value);
    QString publicationTime() const;
    void setPublicationTime(const QString &value);

    QString averageText() const;
    QString currentPriceText() const;
    QString coverAverageText() const;
    QString coverPriceText() const;
    QString bandKey() const;
    QString bandText() const;
    QString effectiveAmbienceName() const;
    QString cacheRangeText() const;
    QString lastRefreshText() const;
    QString nextRefreshText() const;
    QString errorText() const;
    QString warningText() const;
    QVariantList quarterValues() const;
    QVariantList availableAmbiences() const;
    Q_INVOKABLE void reload();
    Q_INVOKABLE void requestRefresh();
    Q_INVOKABLE void clearCache();
    Q_INVOKABLE QVariantMap mappingInfo(const QString &band) const;
    Q_INVOKABLE void setMapping(const QString &band, const QString &url, const QString &displayName);
    Q_INVOKABLE QVariantMap settingsSnapshot() const;
    Q_INVOKABLE bool applySettings(const QVariantMap &settings);
    Q_INVOKABLE bool ambienceAvailable(const QString &url) const;
    Q_INVOKABLE void setAvailableAmbiences(const QVariantList &ambiences);
    Q_INVOKABLE void reloadAmbiences();
    Q_INVOKABLE QVariantList chartPoints(const QString &range) const;
    Q_INVOKABLE QVariantMap chartHourWindow() const;
    Q_INVOKABLE QVariantMap chartWindow(int pageOffset) const;
    Q_INVOKABLE QVariantList upcomingHourlyAverages(int maximumHours = 24) const;
    Q_INVOKABLE QVariantMap priceDay(int pageOffset) const;

signals:
    void configChanged();
    void stateChanged();
    void ambiencesChanged();

private:
    void saveConfigAndRequest();
    QVariantList hourlyRows(const QList<QDateTime> &starts) const;
    QString formatUtc(const QString &iso) const;
    QString bandLabel(const QString &band) const;

    PriceStore m_store;
    AppConfig m_config;
    QMap<qint64, PricePoint> m_prices;
    QJsonObject m_state;
    QVariantList m_ambiences;
    bool m_inventoryRefreshRequested = false;
    bool m_refreshQueued = false;
    QString m_localError;
    class TimedScheduler *m_scheduler = nullptr;
    class QProcess *m_refreshProcess = nullptr;
};
