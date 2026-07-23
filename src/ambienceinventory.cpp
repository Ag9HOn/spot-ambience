#include "ambienceinventory.h"

#include "appconfig.h"

#include <QDir>
#include <QDirIterator>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QMap>
#include <QUrl>

namespace {
QString normalizedPath(const QString &value)
{
    const QString trimmed = value.trimmed();
    const QUrl url(trimmed);
    if (url.isLocalFile())
        return QFileInfo(url.toLocalFile()).absoluteFilePath();
    if (trimmed.startsWith(QLatin1Char('/')))
        return QFileInfo(trimmed).absoluteFilePath();
    return trimmed;
}

QString preferredDisplayName(const QString &path)
{
    const QString normalized = normalizedPath(path);
    for (const QString &band : AppConfig::bands()) {
        if (normalizedPath(AppConfig::preferredUrl(band)) == normalized)
            return AppConfig::preferredName(band);
    }
    QString name = QFileInfo(path).completeBaseName();
    name.replace(QLatin1Char('-'), QLatin1Char(' '));
    if (!name.isEmpty())
        name[0] = name.at(0).toUpper();
    return name;
}

QString wallpaperUrl(const QString &wallpaper, const QString &ambiencePath)
{
    if (wallpaper.isEmpty())
        return QString();
    const QUrl url(wallpaper);
    if (url.isRelative())
        return QUrl::fromLocalFile(QFileInfo(ambiencePath).dir().absoluteFilePath(wallpaper)).toString();
    if (wallpaper.startsWith(QLatin1Char('/')))
        return QUrl::fromLocalFile(wallpaper).toString();
    return wallpaper;
}

QVariantList sortedValues(const QMap<QString, QVariantMap> &byUrl)
{
    QVariantList result;
    for (auto it = byUrl.constBegin(); it != byUrl.constEnd(); ++it)
        result.append(it.value());
    return result;
}
}

QVariantList AmbienceInventory::discover(QString *error)
{
    QString databaseError;
    const QVariantList databaseEntries = discoverFromDatabase(&databaseError);
    if (!databaseEntries.isEmpty()) {
        if (error)
            error->clear();
        return databaseEntries;
    }

    const QVariantList fileEntries = discoverFromFiles();
    if (!fileEntries.isEmpty()) {
        if (error)
            error->clear();
        return fileEntries;
    }

    if (error)
        *error = databaseError.isEmpty()
                ? QStringLiteral("No installed ambiences were found") : databaseError;
    return {};
}

QVariantList AmbienceInventory::discoverFromDatabase(QString *error)
{
    // The protected ambience database is deliberately not read here. Harbour
    // builds must remain sandboxed and use only the public ambience inventory
    // provider; the file scan is retained as a compatibility fallback for
    // devices where that provider is unavailable.
    if (error)
        *error = QStringLiteral("Public ambience inventory is unavailable");
    return {};
}

QVariantList AmbienceInventory::discoverFromFiles()
{
    QMap<QString, QVariantMap> byUrl;
    const QStringList roots {
        QStringLiteral("/usr/share/ambience"),
        QDir::homePath() + QStringLiteral("/.local/share/ambienced"),
        QDir::homePath() + QStringLiteral("/.local/share/system/privileged/Ambienced")
    };
    for (const QString &root : roots) {
        QDirIterator iterator(root, { QStringLiteral("*.ambience") }, QDir::Files,
                              QDirIterator::Subdirectories);
        while (iterator.hasNext()) {
            const QString path = iterator.next();
            QFile file(path);
            if (!file.open(QIODevice::ReadOnly))
                continue;
            const QJsonDocument document = QJsonDocument::fromJson(file.readAll());
            if (!document.isObject())
                continue;
            const QJsonObject object = document.object();
            QString displayName = object.value(QStringLiteral("displayName")).toString().trimmed();
            const QString preferred = preferredDisplayName(path);
            if (displayName.isEmpty() || displayName.startsWith(QStringLiteral("ambience-")))
                displayName = preferred;
            const QVariantMap item {
                { QStringLiteral("url"), path },
                { QStringLiteral("displayName"), displayName },
                { QStringLiteral("wallpaperUrl"), wallpaperUrl(
                      object.value(QStringLiteral("wallpaper")).toString(), path) },
                { QStringLiteral("favorite"), object.value(QStringLiteral("favorite")).toBool() },
                { QStringLiteral("readOnly"), !QFileInfo(path).isWritable() },
                { QStringLiteral("highlightColor"), object.value(QStringLiteral("highlightColor")).toString() },
                { QStringLiteral("primaryColor"), object.value(QStringLiteral("primaryColor")).toString() },
                { QStringLiteral("available"), true }
            };
            byUrl.insert(normalizedPath(path), item);
        }
    }
    return sortedValues(byUrl);
}
