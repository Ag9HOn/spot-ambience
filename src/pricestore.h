#pragma once

#include "appconfig.h"
#include "pricedata.h"

#include <QJsonObject>
#include <QMap>
#include <QString>
#include <QVariantList>

class PriceStore
{
public:
    PriceStore();

    QString dataDirectory() const;
    QString configPath() const;
    QString cachePath() const;
    QString statePath() const;
    QString ambienceInventoryPath() const;
    QString schedulePath() const;
    QString lockPath() const;

    AppConfig loadConfig(bool *ok = nullptr, QString *error = nullptr) const;
    bool saveConfig(const AppConfig &config, QString *error = nullptr) const;
    QMap<qint64, PricePoint> loadPrices(bool *ok = nullptr, QString *error = nullptr) const;
    bool savePrices(const QMap<qint64, PricePoint> &prices, QString *error = nullptr) const;
    QJsonObject loadState(bool *ok = nullptr, QString *error = nullptr) const;
    bool saveState(const QJsonObject &state, QString *error = nullptr) const;
    QVariantList loadAmbiences(bool *ok = nullptr, QString *error = nullptr) const;
    bool saveAmbiences(const QVariantList &ambiences, QString *error = nullptr) const;
    bool clearCache(QString *error = nullptr) const;

private:
    bool ensureDirectory(QString *error = nullptr) const;
    bool writeJson(const QString &path, const QJsonObject &object, QString *error) const;
    QJsonObject readJson(const QString &path, bool *ok, QString *error) const;
};
