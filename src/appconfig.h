#pragma once

#include <QJsonObject>
#include <QMap>
#include <QString>
#include <QTime>

struct AmbienceMapping
{
    QString url;
    QString displayName;
};

struct AppConfig
{
    int version = 4;
    bool enabled = false;
    bool testMode = false;
    bool includeVat = false;
    double vatPercent = 25.5;
    bool personalPriceEnabled = false;
    bool personalPriceDayNight = false;
    double personalPriceFlat = 0.0;
    double personalPriceDay = 0.0;
    double personalPriceNight = 0.0;
    double electricityTax = 2.91788;
    double cheapBelow = 5.0;
    double expensiveAbove = 15.0;
    QTime publicationTime = QTime(14, 15);
    bool overrideEnabled = false;
    QTime overrideStart = QTime(0, 0);
    QTime overrideEnd = QTime(8, 0);
    AmbienceMapping overrideMapping;
    QMap<QString, AmbienceMapping> mappings;

    static QStringList bands();
    static QString preferredUrl(const QString &band);
    static QString preferredName(const QString &band);
    static AppConfig defaultsForInstalledFiles();
    static AppConfig fromJson(const QJsonObject &object, bool *ok = nullptr);
    QJsonObject toJson() const;
    bool isValid(QString *error = nullptr) const;
};
