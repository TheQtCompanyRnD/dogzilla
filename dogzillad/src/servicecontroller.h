// Copyright (C) 2026 The Qt Company Ltd.
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR BSD-3-Clause
#ifndef SERVICECONTROLLER_H
#define SERVICECONTROLLER_H

#include <QtQmlIntegration/qqmlintegration.h>
#include <QObject>
#include <QString>

// Observes and controls one systemd --user unit over D-Bus
// (org.freedesktop.systemd1 on the session bus), so no sudo and no polling:
// ActiveState changes pushed by systemd -- including those made from a shell
// with `systemctl --user` -- update the properties live.
//
// `unit` is the unit name ("dogzilla-slam", ".service" is assumed if no suffix
// is given); set it to retarget the controller, so one class serves any unit.
// `running` is true while the unit is active or activating; writing it
// requests a start or stop (asynchronously: the property only changes once
// systemd reports the new state). restart() bounces the unit.
//
// Needs the user's session bus (XDG_RUNTIME_DIR, or DBUS_SESSION_BUS_ADDRESS).
// If it isn't reachable, `available` stays false and requests are ignored.
class ServiceController : public QObject
{
    Q_OBJECT
    QML_ELEMENT
    Q_PROPERTY(QString unit READ unit WRITE setUnit NOTIFY unitChanged FINAL)
    Q_PROPERTY(bool running READ running WRITE setRunning NOTIFY runningChanged FINAL)
    Q_PROPERTY(QString activeState READ activeState NOTIFY activeStateChanged FINAL)
    Q_PROPERTY(bool available READ available NOTIFY availableChanged FINAL)
public:
    explicit ServiceController(QObject *parent = nullptr);
    ~ServiceController() override;

    QString unit() const { return m_unit; }
    void setUnit(const QString &unit);

    bool running() const { return m_running; }
    QString activeState() const { return m_activeState; }
    bool available() const { return m_available; }

public slots:
    void setRunning(bool running);
    void restart();

signals:
    void unitChanged(const QString &unit);
    void runningChanged(bool running);
    void activeStateChanged(const QString &state);
    void availableChanged(bool available);

private slots:
    void onPropertiesChanged(const QString &interface, const QVariantMap &changed,
                             const QStringList &invalidated);

private:
    void resolve();
    void refreshState();
    void setActiveState(const QString &state);
    void setAvailable(bool available);
    void callManager(const QString &method);

    QString m_unit;
    QString m_unitPath;           // D-Bus object path of the unit; empty until resolved
    QString m_activeState;
    bool m_running = false;
    bool m_available = false;
};

#endif // SERVICECONTROLLER_H
