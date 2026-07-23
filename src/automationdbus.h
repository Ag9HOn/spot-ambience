#pragma once

#include <QDBusAbstractAdaptor>
#include <QVariantMap>

class AutomationRunner;
class TimedScheduler;

class AutomationDbusAdaptor : public QDBusAbstractAdaptor
{
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface", "fi.petteri.SpotAmbience")
public:
    AutomationDbusAdaptor(QObject *parent, TimedScheduler *scheduler,
                           AutomationRunner *runner, bool quitAfterRun);

public slots:
    void runAutomation(const QVariantMap &attributes);

private:
    TimedScheduler *m_scheduler;
    AutomationRunner *m_runner;
    bool m_quitAfterRun;
};

bool registerAutomationDbus(QObject *object, AutomationDbusAdaptor **adaptor,
                             TimedScheduler *scheduler, AutomationRunner *runner,
                             bool quitAfterRun);
