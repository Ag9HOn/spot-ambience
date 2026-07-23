#include "timedscheduler.h"

#include "pricestore.h"

#include <QDBusArgument>
#include <QDBusConnection>
#include <QDBusError>
#include <QDBusInterface>
#include <QDBusMessage>
#include <QDBusMetaType>
#include <QDBusReply>
#include <QDir>
#include <QFile>
#include <QJsonDocument>
#include <QSaveFile>
#include <QTimeZone>
#include <QVariant>

// These are the public wire structures used by com.nokia.time. They are
// intentionally local to the application so no timed development package is
// required at runtime.
struct TimedAttribute {
    QMap<QString, QString> values;
};

struct TimedCredential {
    QString token;
    bool accrue = false;
};

struct TimedAction {
    quint32 flags = 0;
    TimedAttribute attributes;
    QVector<TimedCredential> credentials;
};

struct TimedButton {
    TimedAttribute attributes;
    quint32 snooze = 0;
};

struct TimedRecurrence {
    quint64 minutes = 0;
    quint32 hour = 0;
    quint32 monthDay = 0;
    quint32 weekDay = 0;
    quint32 months = 0;
    quint32 flags = 0;
};

struct TimedEvent {
    qint32 ticker = 0;
    quint32 year = 0;
    quint32 month = 0;
    quint32 day = 0;
    quint32 hour = 0;
    quint32 minute = 0;
    QString timezone;
    TimedAttribute attributes;
    quint32 flags = 0;
    QVector<TimedButton> buttons;
    QVector<TimedAction> actions;
    QVector<TimedRecurrence> recurrences;
    qint32 maximalTimeout = 0;
    qint32 timeoutLength = 1;
    QVector<TimedCredential> credentials;
};

QDBusArgument &operator<<(QDBusArgument &out, const TimedAttribute &value)
{
    out.beginStructure();
    out << value.values;
    out.endStructure();
    return out;
}

QDBusArgument &operator<<(QDBusArgument &out, const TimedCredential &value)
{
    out.beginStructure();
    out << value.token << value.accrue;
    out.endStructure();
    return out;
}

QDBusArgument &operator<<(QDBusArgument &out, const TimedAction &value)
{
    out.beginStructure();
    out << value.flags << value.attributes << value.credentials;
    out.endStructure();
    return out;
}

QDBusArgument &operator<<(QDBusArgument &out, const TimedButton &value)
{
    out.beginStructure();
    out << value.attributes << value.snooze;
    out.endStructure();
    return out;
}

QDBusArgument &operator<<(QDBusArgument &out, const TimedRecurrence &value)
{
    out.beginStructure();
    out << value.minutes << value.hour << value.monthDay << value.weekDay
        << value.months << value.flags;
    out.endStructure();
    return out;
}

QDBusArgument &operator<<(QDBusArgument &out, const TimedEvent &value)
{
    out.beginStructure();
    out << value.ticker << value.year << value.month << value.day << value.hour
        << value.minute << value.timezone << value.attributes << value.flags
        << value.buttons << value.actions << value.recurrences
        << value.maximalTimeout << value.timeoutLength << value.credentials;
    out.endStructure();
    return out;
}

const QDBusArgument &operator>>(const QDBusArgument &in, TimedAttribute &value)
{
    in.beginStructure();
    in >> value.values;
    in.endStructure();
    return in;
}

const QDBusArgument &operator>>(const QDBusArgument &in, TimedCredential &value)
{
    in.beginStructure();
    in >> value.token >> value.accrue;
    in.endStructure();
    return in;
}

const QDBusArgument &operator>>(const QDBusArgument &in, TimedAction &value)
{
    in.beginStructure();
    in >> value.flags >> value.attributes >> value.credentials;
    in.endStructure();
    return in;
}

const QDBusArgument &operator>>(const QDBusArgument &in, TimedButton &value)
{
    in.beginStructure();
    in >> value.attributes >> value.snooze;
    in.endStructure();
    return in;
}

const QDBusArgument &operator>>(const QDBusArgument &in, TimedRecurrence &value)
{
    in.beginStructure();
    in >> value.minutes >> value.hour >> value.monthDay >> value.weekDay
       >> value.months >> value.flags;
    in.endStructure();
    return in;
}

const QDBusArgument &operator>>(const QDBusArgument &in, TimedEvent &value)
{
    in.beginStructure();
    in >> value.ticker >> value.year >> value.month >> value.day >> value.hour
       >> value.minute >> value.timezone >> value.attributes >> value.flags
       >> value.buttons >> value.actions >> value.recurrences
       >> value.maximalTimeout >> value.timeoutLength >> value.credentials;
    in.endStructure();
    return in;
}

Q_DECLARE_METATYPE(TimedAttribute)
Q_DECLARE_METATYPE(TimedCredential)
Q_DECLARE_METATYPE(TimedAction)
Q_DECLARE_METATYPE(TimedButton)
Q_DECLARE_METATYPE(TimedRecurrence)
Q_DECLARE_METATYPE(TimedEvent)

const QString kTimedService = QStringLiteral("com.nokia.time");
const QString kTimedPath = QStringLiteral("/com/nokia/time");
const QString kTimedInterface = QStringLiteral("com.nokia.time");
const QString kAppService = QStringLiteral("fi.petteri.SpotAmbience");
const QString kAppPath = QStringLiteral("/fi/petteri/SpotAmbience");
const QString kAppInterface = QStringLiteral("fi.petteri.SpotAmbience");

constexpr quint32 kTriggerIfMissed = 1u << 2;
constexpr quint32 kSingleShot = 1u << 12;
constexpr quint32 kDbusMethod = 1u << 4;
constexpr quint32 kStateTriggered = 1u << 10;

bool isStaleCookieError(const QString &message)
{
    const QString lowered = message.toLower();
    return lowered.contains(QStringLiteral("not found"))
            || lowered.contains(QStringLiteral("invalid cookie"));
}

TimedScheduler::TimedScheduler(PriceStore *store, QObject *parent)
    : QObject(parent)
    , m_store(store)
{
    qDBusRegisterMetaType<TimedAttribute>();
    qDBusRegisterMetaType<TimedCredential>();
    qDBusRegisterMetaType<TimedAction>();
    qDBusRegisterMetaType<TimedButton>();
    qDBusRegisterMetaType<TimedRecurrence>();
    qDBusRegisterMetaType<TimedEvent>();
}

QString TimedScheduler::timedEventSignature()
{
    qDBusRegisterMetaType<TimedAttribute>();
    qDBusRegisterMetaType<TimedCredential>();
    qDBusRegisterMetaType<TimedAction>();
    qDBusRegisterMetaType<TimedButton>();
    qDBusRegisterMetaType<TimedRecurrence>();
    qDBusRegisterMetaType<TimedEvent>();
    return QString::fromLatin1(QDBusMetaType::typeToSignature(qMetaTypeId<TimedEvent>()));
}

quint32 TimedScheduler::timedActionFlags()
{
    return kDbusMethod | kStateTriggered;
}

QDateTime TimedScheduler::nextQuarterUtcAfter(const QDateTime &nowUtc)
{
    const QTimeZone zone(QByteArray("Europe/Helsinki"));
    const QDateTime local = nowUtc.toUTC().toTimeZone(zone);
    const QDateTime localHour(local.date(), QTime(local.time().hour(), 0), zone);
    const int secondsToNextQuarter = ((local.time().minute() / 15) + 1) * 15 * 60;
    return localHour.addSecs(secondsToNextQuarter).toUTC();
}

QJsonObject TimedScheduler::loadSchedule() const
{
    QFile file(m_store->schedulePath());
    if (!file.open(QIODevice::ReadOnly))
        return {};
    const QJsonDocument document = QJsonDocument::fromJson(file.readAll());
    return document.isObject() ? document.object() : QJsonObject();
}

bool TimedScheduler::saveSchedule(const QJsonObject &schedule, QString *error) const
{
    QDir directory;
    if (!directory.mkpath(m_store->dataDirectory())) {
        if (error) *error = QStringLiteral("Could not create scheduler data directory");
        return false;
    }
    QSaveFile file(m_store->schedulePath());
    if (!file.open(QIODevice::WriteOnly)) {
        if (error) *error = file.errorString();
        return false;
    }
    if (file.write(QJsonDocument(schedule).toJson(QJsonDocument::Compact)) < 0
            || !file.commit()) {
        if (error) *error = file.errorString();
        return false;
    }
    return true;
}

bool TimedScheduler::addEvent(const QDateTime &dueLocal, uint *cookie, QString *error)
{
    TimedEvent event;
    event.year = dueLocal.date().year();
    event.month = dueLocal.date().month();
    event.day = dueLocal.date().day();
    event.hour = dueLocal.time().hour();
    event.minute = dueLocal.time().minute();
    event.timezone = QStringLiteral("Europe/Helsinki");
    event.flags = kTriggerIfMissed | kSingleShot;
    event.attributes.values.insert(QStringLiteral("APPLICATION"), QStringLiteral("SpotAmbience"));
    event.attributes.values.insert(QStringLiteral("TITLE"), QStringLiteral("Spot Ambience automation"));

    TimedAction action;
    // Timed executes an action only when its state mask includes the state
    // entered by the event. A normal one-shot reaches Triggered first.
    action.flags = timedActionFlags();
    action.attributes.values.insert(QStringLiteral("DBUS_SERVICE"), kAppService);
    action.attributes.values.insert(QStringLiteral("DBUS_PATH"), kAppPath);
    action.attributes.values.insert(QStringLiteral("DBUS_INTERFACE"), kAppInterface);
    action.attributes.values.insert(QStringLiteral("DBUS_METHOD"), QStringLiteral("runAutomation"));
    event.actions.append(action);

    QDBusInterface timed(kTimedService, kTimedPath, kTimedInterface,
                         QDBusConnection::systemBus());
    timed.setTimeout(5000);
    if (!timed.isValid()) {
        if (error) *error = QStringLiteral("The Sailfish time service is unavailable");
        return false;
    }
    const QDBusMessage reply = timed.call(QStringLiteral("add_event"), QVariant::fromValue(event));
    if (reply.type() == QDBusMessage::ErrorMessage || reply.arguments().isEmpty()) {
        if (error) *error = reply.errorMessage().isEmpty()
                ? QStringLiteral("Could not register the next scheduled run") : reply.errorMessage();
        return false;
    }
    bool ok = false;
    const uint result = reply.arguments().first().toUInt(&ok);
    if (!ok || result == 0) {
        if (error) *error = QStringLiteral("The time service rejected the scheduled run");
        return false;
    }
    if (cookie) *cookie = result;
    return true;
}

bool TimedScheduler::cancelCookie(uint cookie, QString *error)
{
    if (cookie == 0)
        return true;
    QDBusInterface timed(kTimedService, kTimedPath, kTimedInterface,
                         QDBusConnection::systemBus());
    timed.setTimeout(5000);
    if (!timed.isValid()) {
        if (error) *error = QStringLiteral("The Sailfish time service is unavailable");
        return false;
    }
    const QDBusMessage reply = timed.call(QStringLiteral("cancel"), cookie);
    if (reply.type() == QDBusMessage::ErrorMessage || reply.arguments().isEmpty()) {
        if (reply.type() == QDBusMessage::ErrorMessage && isStaleCookieError(reply.errorMessage()))
            return true;
        if (error) *error = reply.errorMessage().isEmpty()
                ? QStringLiteral("Could not cancel the scheduled run") : reply.errorMessage();
        return false;
    }
    if (!reply.arguments().first().toBool()) {
        if (error) *error = QStringLiteral("The scheduled run could not be cancelled");
        return false;
    }
    return true;
}

bool TimedScheduler::scheduleNext(QString *error)
{
    const QJsonObject old = loadSchedule();
    const uint oldCookie = old.value(QStringLiteral("cookie")).toInt();
    const QDateTime oldDue = QDateTime::fromString(
            old.value(QStringLiteral("dueUtc")).toString(), Qt::ISODate);
    const bool oldEventIsFuture = oldCookie > 0 && oldDue.isValid()
            && oldDue > QDateTime::currentDateTimeUtc();
    if (oldEventIsFuture) {
        QString cancelError;
        if (!cancelCookie(oldCookie, &cancelError)) {
            if (error) *error = cancelError;
            return false;
        }
    }

    const QDateTime due = nextQuarterUtcAfter(QDateTime::currentDateTimeUtc())
            .toTimeZone(QTimeZone(QByteArray("Europe/Helsinki")));
    uint cookie = 0;
    if (!addEvent(due, &cookie, error))
        return false;
    return saveSchedule(QJsonObject {
        { QStringLiteral("version"), 1 },
        { QStringLiteral("cookie"), int(cookie) },
        { QStringLiteral("dueUtc"), due.toUTC().toString(Qt::ISODate) }
    }, error);
}

bool TimedScheduler::cancel(QString *error)
{
    const QJsonObject schedule = loadSchedule();
    const uint cookie = schedule.value(QStringLiteral("cookie")).toInt();
    if (cookie > 0 && !cancelCookie(cookie, error))
        return false;
    return QFile::remove(m_store->schedulePath()) || !QFile::exists(m_store->schedulePath());
}

bool TimedScheduler::sync(bool enabled, QString *error)
{
    if (!enabled)
        return cancel(error);
    const QJsonObject schedule = loadSchedule();
    const QDateTime due = QDateTime::fromString(schedule.value(QStringLiteral("dueUtc")).toString(), Qt::ISODate);
    const uint cookie = schedule.value(QStringLiteral("cookie")).toInt();
    if (cookie > 0 && due.isValid() && due > QDateTime::currentDateTimeUtc())
        return true;
    return scheduleNext(error);
}

QDateTime TimedScheduler::nextDueUtc() const
{
    return QDateTime::fromString(loadSchedule().value(QStringLiteral("dueUtc")).toString(), Qt::ISODate);
}
