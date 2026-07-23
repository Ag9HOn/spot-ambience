#include "automationdbus.h"

#include "automationrunner.h"
#include "pricestore.h"
#include "timedscheduler.h"

#include <QCoreApplication>
#include <QDebug>
#include <QDBusConnection>
#include <QDBusError>
#include <QTimer>

namespace {
const QString kService = QStringLiteral("fi.petteri.SpotAmbience");
const QString kPath = QStringLiteral("/fi/petteri/SpotAmbience");
}

AutomationDbusAdaptor::AutomationDbusAdaptor(QObject *parent, TimedScheduler *scheduler,
                                             AutomationRunner *runner, bool quitAfterRun)
    : QDBusAbstractAdaptor(parent)
    , m_scheduler(scheduler)
    , m_runner(runner)
    , m_quitAfterRun(quitAfterRun)
{
}

void AutomationDbusAdaptor::runAutomation(const QVariantMap &)
{
    QString scheduleError;
    // Schedule the successor before network work. A disabled configuration
    // prevents recreation while a due event is being delivered.
    PriceStore store;
    bool configOk = false;
    const AppConfig config = store.loadConfig(&configOk);
    if (configOk && config.enabled)
        m_scheduler->scheduleNext(&scheduleError);
    else
        m_scheduler->cancel(&scheduleError);
    const int result = m_runner->run(false);
    if (!scheduleError.isEmpty())
        qWarning() << scheduleError;
    if (m_quitAfterRun)
        QTimer::singleShot(0, qApp, [result]() { qApp->exit(result); });
}

bool registerAutomationDbus(QObject *object, AutomationDbusAdaptor **adaptor,
                             TimedScheduler *scheduler, AutomationRunner *runner,
                             bool quitAfterRun)
{
    QDBusConnection bus = QDBusConnection::sessionBus();
    if (!bus.registerService(kService)) {
        qWarning() << "Could not register automation D-Bus service:" << bus.lastError().message();
        return false;
    }
    *adaptor = new AutomationDbusAdaptor(object, scheduler, runner, quitAfterRun);
    if (!bus.registerObject(kPath, object, QDBusConnection::ExportAdaptors)) {
        delete *adaptor;
        *adaptor = nullptr;
        return false;
    }
    return true;
}
