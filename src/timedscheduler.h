#pragma once

#include "appconfig.h"

#include <QDateTime>
#include <QJsonObject>
#include <QObject>

class PriceStore;

class TimedScheduler : public QObject
{
    Q_OBJECT
public:
    explicit TimedScheduler(PriceStore *store, QObject *parent = nullptr);

    static QString timedEventSignature();
    static quint32 timedActionFlags();
    static QDateTime nextQuarterUtcAfter(const QDateTime &nowUtc);
    bool sync(bool enabled, QString *error = nullptr);
    bool scheduleNext(QString *error = nullptr);
    bool cancel(QString *error = nullptr);
    QDateTime nextDueUtc() const;

private:
    bool addEvent(const QDateTime &dueLocal, uint *cookie, QString *error);
    bool cancelCookie(uint cookie, QString *error);
    QJsonObject loadSchedule() const;
    bool saveSchedule(const QJsonObject &schedule, QString *error) const;

    PriceStore *m_store;
};
