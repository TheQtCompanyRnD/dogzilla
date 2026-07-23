#include "motionlibrary.h"

#include <QDebug>
#include <QDir>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QStandardPaths>

MotionLibrary::MotionLibrary(QObject *parent) : QObject(parent) {}

QString MotionLibrary::filePath() const
{
    const QString dir = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    QDir().mkpath(dir);
    return dir + QStringLiteral("/motions.json");
}

QJsonObject MotionLibrary::readAll() const
{
    QFile f(filePath());
    if (!f.open(QIODevice::ReadOnly))
        return {};
    return QJsonDocument::fromJson(f.readAll()).object();
}

void MotionLibrary::writeAll(const QJsonObject &all)
{
    QFile f(filePath());
    if (!f.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        qWarning() << "MotionLibrary: cannot write" << filePath();
        return;
    }
    f.write(QJsonDocument(all).toJson(QJsonDocument::Indented));
    emit changed();
}

QStringList MotionLibrary::names() const
{
    QStringList n = readAll().keys();
    n.sort();
    return n;
}

void MotionLibrary::save(const QString &name, const QString &motionJson)
{
    if (name.isEmpty())
        return;
    QJsonParseError err;
    const QJsonDocument d = QJsonDocument::fromJson(motionJson.toUtf8(), &err);
    if (err.error != QJsonParseError::NoError) {
        qWarning() << "MotionLibrary::save parse error:" << err.errorString();
        return;
    }
    QJsonObject all = readAll();
    all.insert(name, d.isArray() ? QJsonValue(d.array()) : QJsonValue(d.object()));
    writeAll(all);
}

QString MotionLibrary::load(const QString &name) const
{
    const QJsonObject all = readAll();
    const auto it = all.find(name);
    if (it == all.end())
        return {};
    const QJsonValue v = it.value();
    const QJsonDocument d = v.isArray() ? QJsonDocument(v.toArray())
                                        : QJsonDocument(v.toObject());
    return QString::fromUtf8(d.toJson(QJsonDocument::Compact));
}

void MotionLibrary::remove(const QString &name)
{
    QJsonObject all = readAll();
    if (!all.contains(name))
        return;
    all.remove(name);
    writeAll(all);
}
