#include <QCoreApplication>
#include <QQmlApplicationEngine>
#include <QStandardPaths>
#include <QUrl>
#include <QtRos2Core/qros2context.h>
#include "controller.h"

int main(int argc, char **argv)
{
    QRos2Context::init(argc, argv);
    QCoreApplication app(argc, argv);
    // set the AppDataLocation suffix
    QCoreApplication::setApplicationName(QStringLiteral("dogzillad"));

    // main.qml is an editable script rather than a compiled-in resource.
    // AppDataLocation searches ~/.local/share/dogzillad then the installed
    // /usr/share/dogzillad/main.qml. Fall back to ./qml/main.qml for running
    // straight out of the build/source tree during development.
    QString qmlPath =
            QStandardPaths::locate(QStandardPaths::AppDataLocation, QStringLiteral("main.qml"));
    if (qmlPath.isEmpty())
        qmlPath = QStringLiteral("qml/main.qml");

    QQmlApplicationEngine engine;
    qDebug() << "loading" << qmlPath << "import paths" << engine.importPathList();
    QObject::connect(
            &engine,
            &QQmlApplicationEngine::objectCreationFailed,
            &app,
            []() { QCoreApplication::exit(-1); },
            Qt::QueuedConnection);
    engine.load(QUrl::fromLocalFile(qmlPath));
    if (engine.rootObjects().isEmpty())
        return -1;
    return QCoreApplication::exec();
}
