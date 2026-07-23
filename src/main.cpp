#include <QCoreApplication>
#include <QGuiApplication>
#include <QQmlContext>
#include <QQuickView>
#include <QScopedPointer>
#include <QTimer>

#include <sailfishapp.h>

#include "automationrunner.h"
#include "automationdbus.h"
#include "pricechartitem.h"
#include "spotcontroller.h"
#include "timedscheduler.h"

namespace {
void identifyApplication(QCoreApplication *app)
{
    app->setOrganizationName(QStringLiteral("fi.petteri"));
    app->setApplicationName(QStringLiteral("SpotAmbience"));
    app->setApplicationVersion(QStringLiteral("1.0.0"));
}
}

int main(int argc, char *argv[])
{
    bool runOnce = false;
    bool dbusMode = false;
    for (int i = 1; i < argc; ++i) {
        if (QString::fromLocal8Bit(argv[i]) == QStringLiteral("--run-once"))
            runOnce = true;
        if (QString::fromLocal8Bit(argv[i]) == QStringLiteral("--dbus"))
            dbusMode = true;
    }

    if (runOnce || dbusMode) {
        QCoreApplication app(argc, argv);
        identifyApplication(&app);
        PriceStore store;
        TimedScheduler scheduler(&store);
        AutomationRunner runner;
        AutomationDbusAdaptor *adaptor = nullptr;
        if (dbusMode) {
            if (!registerAutomationDbus(&app, &adaptor, &scheduler, &runner, true))
                return 1;
            // Allow the five-second Timed operation and the fifteen-second
            // price request to complete without the safety timeout racing the
            // final atomic state write.
            QTimer::singleShot(30000, &app, &QCoreApplication::quit);
            return app.exec();
        }
        return runner.run(true);
    }

    QScopedPointer<QGuiApplication> app(SailfishApp::application(argc, argv));
    identifyApplication(app.data());
    qmlRegisterType<PriceChartItem>("SpotAmbience", 1, 0, "PriceChart");
    PriceStore store;
    TimedScheduler scheduler(&store);
    AutomationRunner runner;
    AutomationDbusAdaptor *adaptor = nullptr;
    SpotController controller(nullptr, &scheduler);
    registerAutomationDbus(app.data(), &adaptor, &scheduler, &runner, false);
    QScopedPointer<QQuickView> view(SailfishApp::createView());
    view->rootContext()->setContextProperty(QStringLiteral("spot"), &controller);
    view->rootContext()->setContextProperty(QStringLiteral("startupPage"),
        QString::fromLocal8Bit(qgetenv("SPOTAMBIENCE_START_PAGE")));
    view->setSource(SailfishApp::pathTo(QStringLiteral("qml/harbour-spotambience.qml")));
    view->show();
    return app->exec();
}
