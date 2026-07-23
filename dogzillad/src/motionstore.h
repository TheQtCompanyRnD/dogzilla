#ifndef MOTIONSTORE_H
#define MOTIONSTORE_H

#include <QtQmlIntegration/qqmlintegration.h>
#include <QObject>

// Persists the named-motion library JSON (the { name: JointTrajectory } blob the
// twin publishes on /motion/library) to AppDataLocation/motions.json, so the robot
// keeps its taught poses across restarts even when the twin isn't connected. QML
// saves the received string on change and loads it once at startup.
class MotionStore : public QObject
{
    Q_OBJECT
    QML_ELEMENT

public:
    explicit MotionStore(QObject *parent = nullptr);

    Q_INVOKABLE void save(const QString &json) const;
    Q_INVOKABLE QString load() const;

private:
    QString filePath() const;
};

#endif // MOTIONSTORE_H
