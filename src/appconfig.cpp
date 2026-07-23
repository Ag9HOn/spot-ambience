#include "appconfig.h"

#include <QFileInfo>
#include <QJsonDocument>

namespace {
const QMap<QString, QString> kPreferredUrls = {
    { QStringLiteral("unknown"), QStringLiteral("/usr/share/ambience/kaamos-black/kaamos-black.ambience") },
    { QStringLiteral("negative"), QStringLiteral("/usr/share/ambience/snow-white/snow-white.ambience") },
    { QStringLiteral("cheap"), QStringLiteral("/usr/share/ambience/inari-blue/inari-blue.ambience") },
    { QStringLiteral("middle"), QStringLiteral("/usr/share/ambience/ruska/ruska.ambience") },
    { QStringLiteral("expensive"), QStringLiteral("/usr/share/ambience/flare/flare.ambience") }
};

const QMap<QString, QString> kPreferredNames = {
    { QStringLiteral("unknown"), QStringLiteral("Kaamos Black") },
    { QStringLiteral("negative"), QStringLiteral("Snow White") },
    { QStringLiteral("cheap"), QStringLiteral("Inari Blue") },
    { QStringLiteral("middle"), QStringLiteral("Ruska") },
    { QStringLiteral("expensive"), QStringLiteral("Flare") }
};
}

QStringList AppConfig::bands()
{
    return { QStringLiteral("unknown"), QStringLiteral("negative"), QStringLiteral("cheap"),
             QStringLiteral("middle"), QStringLiteral("expensive") };
}

QString AppConfig::preferredUrl(const QString &band)
{
    return kPreferredUrls.value(band);
}

QString AppConfig::preferredName(const QString &band)
{
    return kPreferredNames.value(band);
}

AppConfig AppConfig::defaultsForInstalledFiles()
{
    AppConfig config;
    for (const QString &band : bands()) {
        AmbienceMapping mapping;
        mapping.displayName = preferredName(band);
        const QString path = preferredUrl(band);
        if (QFileInfo::exists(path))
            mapping.url = path;
        config.mappings.insert(band, mapping);
    }
    config.overrideMapping.displayName = preferredName(QStringLiteral("cheap"));
    const QString overridePath = preferredUrl(QStringLiteral("cheap"));
    if (QFileInfo::exists(overridePath))
        config.overrideMapping.url = overridePath;
    return config;
}

AppConfig AppConfig::fromJson(const QJsonObject &object, bool *ok)
{
    AppConfig config = defaultsForInstalledFiles();
    const int storedVersion = object.value(QStringLiteral("version")).toInt(1);
    bool parsed = storedVersion >= 1 && storedVersion <= 4;
    config.enabled = object.value(QStringLiteral("enabled")).toBool(config.enabled);
    config.testMode = object.value(QStringLiteral("testMode")).toBool(config.testMode);
    config.includeVat = object.value(QStringLiteral("includeVat")).toBool(config.includeVat);
    config.vatPercent = object.value(QStringLiteral("vatPercent")).toDouble(config.vatPercent);
    config.personalPriceEnabled = object.value(QStringLiteral("personalPriceEnabled"))
            .toBool(config.personalPriceEnabled);
    config.personalPriceDayNight = object.value(QStringLiteral("personalPriceDayNight"))
            .toBool(config.personalPriceDayNight);
    config.personalPriceFlat = object.value(QStringLiteral("personalPriceFlat"))
            .toDouble(config.personalPriceFlat);
    config.personalPriceDay = object.value(QStringLiteral("personalPriceDay"))
            .toDouble(config.personalPriceDay);
    config.personalPriceNight = object.value(QStringLiteral("personalPriceNight"))
            .toDouble(config.personalPriceNight);
    config.electricityTax = object.value(QStringLiteral("electricityTax"))
            .toDouble(config.electricityTax);
    config.cheapBelow = object.value(QStringLiteral("cheapBelow")).toDouble(config.cheapBelow);
    config.expensiveAbove = object.value(QStringLiteral("expensiveAbove")).toDouble(config.expensiveAbove);

    const QTime time = QTime::fromString(object.value(QStringLiteral("publicationTime")).toString(), QStringLiteral("HH:mm"));
    if (time.isValid())
        config.publicationTime = time;
    else if (object.contains(QStringLiteral("publicationTime")))
        parsed = false;

    config.overrideEnabled = object.value(QStringLiteral("overrideEnabled")).toBool(config.overrideEnabled);
    const QTime overrideStart = QTime::fromString(
            object.value(QStringLiteral("overrideStart")).toString(), QStringLiteral("HH:mm"));
    const QTime overrideEnd = QTime::fromString(
            object.value(QStringLiteral("overrideEnd")).toString(), QStringLiteral("HH:mm"));
    if (overrideStart.isValid())
        config.overrideStart = overrideStart;
    else if (object.contains(QStringLiteral("overrideStart")))
        parsed = false;
    if (overrideEnd.isValid())
        config.overrideEnd = overrideEnd;
    else if (object.contains(QStringLiteral("overrideEnd")))
        parsed = false;

    const QJsonObject overrideMapping = object.value(QStringLiteral("overrideMapping")).toObject();
    if (!overrideMapping.isEmpty()) {
        config.overrideMapping.url = overrideMapping.value(QStringLiteral("url")).toString();
        config.overrideMapping.displayName = overrideMapping.value(QStringLiteral("displayName"))
                .toString(preferredName(QStringLiteral("cheap")));
    }

    const QJsonObject mappings = object.value(QStringLiteral("mappings")).toObject();
    for (const QString &band : bands()) {
        if (!mappings.contains(band))
            continue;
        const QJsonObject value = mappings.value(band).toObject();
        config.mappings[band].url = value.value(QStringLiteral("url")).toString();
        config.mappings[band].displayName = value.value(QStringLiteral("displayName")).toString(preferredName(band));
    }

    QString validationError;
    parsed = parsed && config.isValid(&validationError);
    if (ok)
        *ok = parsed;
    return config;
}

QJsonObject AppConfig::toJson() const
{
    QJsonObject mappingObject;
    for (const QString &band : bands()) {
        const AmbienceMapping mapping = mappings.value(band);
        mappingObject.insert(band, QJsonObject {
            { QStringLiteral("url"), mapping.url },
            { QStringLiteral("displayName"), mapping.displayName }
        });
    }
    return QJsonObject {
        { QStringLiteral("version"), version },
        { QStringLiteral("enabled"), enabled },
        { QStringLiteral("testMode"), testMode },
        { QStringLiteral("includeVat"), includeVat },
        { QStringLiteral("vatPercent"), vatPercent },
        { QStringLiteral("personalPriceEnabled"), personalPriceEnabled },
        { QStringLiteral("personalPriceDayNight"), personalPriceDayNight },
        { QStringLiteral("personalPriceFlat"), personalPriceFlat },
        { QStringLiteral("personalPriceDay"), personalPriceDay },
        { QStringLiteral("personalPriceNight"), personalPriceNight },
        { QStringLiteral("electricityTax"), electricityTax },
        { QStringLiteral("cheapBelow"), cheapBelow },
        { QStringLiteral("expensiveAbove"), expensiveAbove },
        { QStringLiteral("publicationTime"), publicationTime.toString(QStringLiteral("HH:mm")) },
        { QStringLiteral("overrideEnabled"), overrideEnabled },
        { QStringLiteral("overrideStart"), overrideStart.toString(QStringLiteral("HH:mm")) },
        { QStringLiteral("overrideEnd"), overrideEnd.toString(QStringLiteral("HH:mm")) },
        { QStringLiteral("overrideMapping"), QJsonObject {
              { QStringLiteral("url"), overrideMapping.url },
              { QStringLiteral("displayName"), overrideMapping.displayName }
          } },
        { QStringLiteral("mappings"), mappingObject }
    };
}

bool AppConfig::isValid(QString *error) const
{
    if (!(cheapBelow > 0.0)) {
        if (error) *error = QStringLiteral("Cheap threshold must be greater than zero");
        return false;
    }
    if (!(expensiveAbove > cheapBelow)) {
        if (error) *error = QStringLiteral("Expensive threshold must be greater than cheap threshold");
        return false;
    }
    if (!(vatPercent >= 0.0 && vatPercent <= 100.0)) {
        if (error) *error = QStringLiteral("VAT rate must be between zero and 100 percent");
        return false;
    }
    const double selectedPersonalPrice = personalPriceDayNight
            ? qMax(personalPriceDay, personalPriceNight) : personalPriceFlat;
    if (!(personalPriceFlat >= 0.0) || !(personalPriceDay >= 0.0)
            || !(personalPriceNight >= 0.0) || !(selectedPersonalPrice <= 100.0)
            || !(electricityTax >= 0.0) || !(electricityTax <= 100.0)) {
        if (error) *error = QStringLiteral("Transfer price must be between zero and 100 c/kWh");
        return false;
    }
    if (!publicationTime.isValid()) {
        if (error) *error = QStringLiteral("Publication time is invalid");
        return false;
    }
    if (!overrideStart.isValid() || !overrideEnd.isValid()) {
        if (error) *error = QStringLiteral("Ambience override time is invalid");
        return false;
    }
    return true;
}
