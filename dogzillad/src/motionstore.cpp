#include "motionstore.h"

#include <QDebug>
#include <QDir>
#include <QFile>
#include <QStandardPaths>

MotionStore::MotionStore(QObject *parent) : QObject(parent) {}

QString MotionStore::filePath() const
{
    const QString dir = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    QDir().mkpath(dir);
    return dir + QStringLiteral("/motions.json");
}

void MotionStore::save(const QString &json) const
{
    QFile f(filePath());
    if (!f.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        qWarning() << "MotionStore: cannot write" << filePath();
        return;
    }
    f.write(json.toUtf8());
}

QString MotionStore::load() const
{
    QFile f(filePath());
    if (!f.open(QIODevice::ReadOnly))
        return {};
    return QString::fromUtf8(f.readAll());
}
