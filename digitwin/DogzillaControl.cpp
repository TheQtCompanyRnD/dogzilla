
// Manual extension point. This file is created once if missing.
#include "DogzillaControl.h"
#include <QtRos2Core/qros2context.h>
#include <QCoreApplication>
#include <QLoggingCategory>

Q_STATIC_LOGGING_CATEGORY(lcCtrl, "dogzilla.control")

// static init when the plugin is loaded
namespace {
    static bool s_initialized = []() {
        const auto args = qApp->arguments();
        // recover the app arguments to pass to QRos2Context::init(argc, argv)
        std::vector<char*> argv(args.size() + 1, nullptr);
        std::vector<QByteArray> storage(args.size());
        for (int i = 0; i < args.size(); ++i) {
            storage[i] = args[i].toUtf8();
            argv[i] = storage[i].data();
        }
        QRos2Context::init(args.size(), argv.data());
        qCDebug(lcCtrl) << "QRos2Context initialized with" << args;
        return true;
    }();
}

DogzillaControl::DogzillaControl(QObject *parent)
    : DogzillaControlBase(parent)
{
}

static QString snakeToCamel(const QString &snake)
{
    // TODO optimize
    QStringList parts = snake.split('_', Qt::SkipEmptyParts);
    for (int i = 1; i<parts.size(); ++i)
        parts[i].replace(0, 1, parts[i][0].toUpper());

    return parts.join("");
}

void DogzillaControl::updateJointState(QStringList names, QList<qreal> values)
{
    // qCDebug(lcCtrl) << names << values;
    Q_ASSERT(names.size() == values.size());
    const auto meta = metaObject();
    for (int i = 0; i < names.size(); ++i) {
        const QString propertyName = snakeToCamel(names.at(i)) + "Angle";
        const int propIdx = meta->indexOfProperty(propertyName.toLocal8Bit().data());
        if (propIdx < 0) {
            qWarning() << propertyName << "not found";
            continue;
        }
        const qreal deg = values.at(i) * 180 / M_PI;
        qCDebug(lcCtrl).noquote() << names.at(i) << propertyName << values.at(i) << "rad" << deg << "deg";
        meta->property(propIdx).write(this, deg);
    }
}
