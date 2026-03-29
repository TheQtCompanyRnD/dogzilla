#include <QCoreApplication>
#include <QQmlApplicationEngine>
#include <QtRos2Core/qros2context.h>
#include "controller.h"

int main(int argc, char **argv)
{
    QRos2Context::init(argc, argv);
    QCoreApplication app(argc, argv);
    Controller::setPortAndBaudRate("/dev/ttyAMA0", 115200);
    QQmlApplicationEngine engine;
    qDebug() << "import paths" << engine.importPathList();
    QObject::connect(
            &engine,
            &QQmlApplicationEngine::objectCreationFailed,
            &app,
            []() { QCoreApplication::exit(-1); },
            Qt::QueuedConnection);
    engine.load("qml/main.qml");
    if (engine.rootObjects().isEmpty())
        return -1;
    return QCoreApplication::exec();
}
