#pragma once

#include <QString>
#include <QVariantList>

class AmbienceInventory
{
public:
    static QVariantList discover(QString *error = nullptr);

private:
    static QVariantList discoverFromDatabase(QString *error = nullptr);
    static QVariantList discoverFromFiles();
};
