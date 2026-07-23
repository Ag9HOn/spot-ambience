#include "pricestore.h"

#include <QCoreApplication>
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QJsonDocument>
#include <QJsonArray>
#include <QJsonParseError>
#include <QSaveFile>
#include <QStandardPaths>

PriceStore::PriceStore()
{
}

QString PriceStore::dataDirectory() const
{
    return QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
}

QString PriceStore::configPath() const { return dataDirectory() + QStringLiteral("/config.json"); }
QString PriceStore::cachePath() const { return dataDirectory() + QStringLiteral("/prices.json"); }
QString PriceStore::statePath() const { return dataDirectory() + QStringLiteral("/state.json"); }
QString PriceStore::ambienceInventoryPath() const { return dataDirectory() + QStringLiteral("/ambiences.json"); }
QString PriceStore::schedulePath() const { return dataDirectory() + QStringLiteral("/schedule.json"); }
QString PriceStore::lockPath() const { return dataDirectory() + QStringLiteral("/runner.lock"); }

bool PriceStore::ensureDirectory(QString *error) const
{
    QDir directory;
    if (directory.mkpath(dataDirectory()))
        return true;
    if (error) *error = QStringLiteral("Could not create application data directory");
    return false;
}

QJsonObject PriceStore::readJson(const QString &path, bool *ok, QString *error) const
{
    QFile file(path);
    if (!file.exists()) {
        if (ok) *ok = false;
        return QJsonObject();
    }
    if (!file.open(QIODevice::ReadOnly)) {
        if (ok) *ok = false;
        if (error) *error = file.errorString();
        return QJsonObject();
    }
    QJsonParseError parseError;
    const QJsonDocument document = QJsonDocument::fromJson(file.readAll(), &parseError);
    const bool parsed = parseError.error == QJsonParseError::NoError && document.isObject();
    if (!parsed && error)
        *error = QStringLiteral("Invalid JSON in %1: %2").arg(path, parseError.errorString());
    if (ok) *ok = parsed;
    return parsed ? document.object() : QJsonObject();
}

bool PriceStore::writeJson(const QString &path, const QJsonObject &object, QString *error) const
{
    if (!ensureDirectory(error))
        return false;
    QSaveFile file(path);
    if (!file.open(QIODevice::WriteOnly)) {
        if (error) *error = file.errorString();
        return false;
    }
    if (file.write(QJsonDocument(object).toJson(QJsonDocument::Compact)) < 0 || !file.commit()) {
        if (error) *error = file.errorString();
        return false;
    }
    return true;
}

AppConfig PriceStore::loadConfig(bool *ok, QString *error) const
{
    bool documentOk = false;
    const QJsonObject object = readJson(configPath(), &documentOk, error);
    if (!documentOk) {
        if (ok) *ok = false;
        return AppConfig::defaultsForInstalledFiles();
    }
    bool configOk = false;
    const AppConfig config = AppConfig::fromJson(object, &configOk);
    if (!configOk && error && error->isEmpty())
        *error = QStringLiteral("Configuration is invalid");
    if (ok) *ok = configOk;
    return config;
}

bool PriceStore::saveConfig(const AppConfig &config, QString *error) const
{
    QString validation;
    if (!config.isValid(&validation)) {
        if (error) *error = validation;
        return false;
    }
    return writeJson(configPath(), config.toJson(), error);
}

QMap<qint64, PricePoint> PriceStore::loadPrices(bool *ok, QString *error) const
{
    bool documentOk = false;
    const QJsonObject object = readJson(cachePath(), &documentOk, error);
    if (!documentOk) {
        if (ok) *ok = false;
        return {};
    }
    QMap<qint64, PricePoint> result;
    const bool parsed = PriceData::fromJson(object.value(QStringLiteral("prices")).toArray(), &result, error);
    if (ok) *ok = parsed;
    return parsed ? result : QMap<qint64, PricePoint>();
}

bool PriceStore::savePrices(const QMap<qint64, PricePoint> &prices, QString *error) const
{
    return writeJson(cachePath(), QJsonObject {
        { QStringLiteral("version"), 1 },
        { QStringLiteral("prices"), PriceData::toJson(prices) }
    }, error);
}

QJsonObject PriceStore::loadState(bool *ok, QString *error) const
{
    return readJson(statePath(), ok, error);
}

bool PriceStore::saveState(const QJsonObject &state, QString *error) const
{
    return writeJson(statePath(), state, error);
}

QVariantList PriceStore::loadAmbiences(bool *ok, QString *error) const
{
    bool documentOk = false;
    const QJsonObject object = readJson(ambienceInventoryPath(), &documentOk, error);
    if (!documentOk) {
        if (ok) *ok = false;
        return {};
    }
    const QJsonValue value = object.value(QStringLiteral("ambiences"));
    const bool valid = value.isArray();
    if (!valid && error)
        *error = QStringLiteral("Ambience inventory is invalid");
    if (ok) *ok = valid;
    return valid ? value.toArray().toVariantList() : QVariantList();
}

bool PriceStore::saveAmbiences(const QVariantList &ambiences, QString *error) const
{
    return writeJson(ambienceInventoryPath(), QJsonObject {
        { QStringLiteral("version"), 1 },
        { QStringLiteral("generatedUtc"), QDateTime::currentDateTimeUtc().toString(Qt::ISODate) },
        { QStringLiteral("ambiences"), QJsonArray::fromVariantList(ambiences) }
    }, error);
}

bool PriceStore::clearCache(QString *error) const
{
    bool success = true;
    for (const QString &path : { cachePath(), statePath() }) {
        if (QFile::exists(path) && !QFile::remove(path)) {
            success = false;
            if (error) *error = QStringLiteral("Could not remove %1").arg(path);
        }
    }
    return success;
}
