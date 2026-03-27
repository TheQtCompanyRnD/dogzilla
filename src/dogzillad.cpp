#include <QCoreApplication>
#include <QQmlApplicationEngine>
#include "controller.h"

// Controller ctl("/dev/ttyAMA0", 115200, this);
// JoystickHandler joy(&ctl); // TODO handle joystick in QML

int main(int argc, char **argv)
{
    QCoreApplication app(argc, argv);
    Controller::setPortAndBaudRate("/dev/ttyAMA0", 115200);
    QQmlApplicationEngine engine;
    engine.load("qml/main.qml");
    if (engine.rootObjects().isEmpty())
        return -1;
    return QCoreApplication::exec();
}
