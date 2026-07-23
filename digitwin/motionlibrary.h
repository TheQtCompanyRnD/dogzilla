#ifndef MOTIONLIBRARY_H
#define MOTIONLIBRARY_H

#include <QObject>
#include <QQmlEngine>
#include <QStringList>
#include <QJsonObject>

// Persists named teach-pendant motions as a single JSON file under the app's
// writable data location (QStandardPaths::AppDataLocation/motions.json). Each
// motion is a JSON array of waypoints ({ pose: {joint: deg, ...}, duration });
// QML passes and receives them as JSON strings (JSON.stringify / JSON.parse).
class MotionLibrary : public QObject
{
    Q_OBJECT
    QML_ELEMENT
    Q_PROPERTY(QStringList names READ names NOTIFY changed)

public:
    explicit MotionLibrary(QObject *parent = nullptr);

    QStringList names() const;

    Q_INVOKABLE void save(const QString &name, const QString &motionJson);
    Q_INVOKABLE QString load(const QString &name) const;
    Q_INVOKABLE void remove(const QString &name);

signals:
    void changed();

private:
    QString filePath() const;
    QJsonObject readAll() const;
    void writeAll(const QJsonObject &all);
};

#endif // MOTIONLIBRARY_H
