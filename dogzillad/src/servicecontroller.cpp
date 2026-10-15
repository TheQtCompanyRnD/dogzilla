// Copyright (C) 2026 The Qt Company Ltd.
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR BSD-3-Clause
#include "servicecontroller.h"

#include <QDBusConnection>
#include <QDBusMessage>
#include <QDBusObjectPath>
#include <QDBusPendingCallWatcher>
#include <QDBusPendingReply>
#include <QDBusVariant>
#include <QLoggingCategory>

Q_LOGGING_CATEGORY(lcService, "dogzilla.service")

using namespace Qt::StringLiterals;

static const auto kService = u"org.freedesktop.systemd1"_s;
static const auto kManagerPath = u"/org/freedesktop/systemd1"_s;
static const auto kManagerIface = u"org.freedesktop.systemd1.Manager"_s;
static const auto kUnitIface = u"org.freedesktop.systemd1.Unit"_s;
static const auto kPropsIface = u"org.freedesktop.DBus.Properties"_s;

static QString fullUnitName(const QString &unit)
{
    return unit.contains(u'.') ? unit : unit + ".service"_L1;
}

ServiceController::ServiceController(QObject *parent) : QObject(parent) {}

ServiceController::~ServiceController()
{
    if (!m_unitPath.isEmpty()) {
        QDBusConnection::sessionBus().disconnect(kService, m_unitPath, kPropsIface,
                u"PropertiesChanged"_s, this,
                SLOT(onPropertiesChanged(QString,QVariantMap,QStringList)));
    }
}

void ServiceController::setUnit(const QString &unit)
{
    if (m_unit == unit)
        return;
    if (!m_unitPath.isEmpty()) {
        QDBusConnection::sessionBus().disconnect(kService, m_unitPath, kPropsIface,
                u"PropertiesChanged"_s, this,
                SLOT(onPropertiesChanged(QString,QVariantMap,QStringList)));
        m_unitPath.clear();
    }
    m_unit = unit;
    setActiveState({});
    emit unitChanged(m_unit);
    if (!m_unit.isEmpty())
        resolve();
}

void ServiceController::resolve()
{
    auto bus = QDBusConnection::sessionBus();
    if (!bus.isConnected()) {
        qCWarning(lcService) << "no session bus; cannot control" << m_unit
                             << bus.lastError().message();
        setAvailable(false);
        return;
    }

    // systemd only emits unit PropertiesChanged to clients that have subscribed.
    // (Idempotent per connection; it is dropped when we disconnect.)
    bus.asyncCall(QDBusMessage::createMethodCall(kService, kManagerPath, kManagerIface,
                                                 u"Subscribe"_s));

    // LoadUnit, not GetUnit: works for units that are inactive and not loaded.
    auto msg = QDBusMessage::createMethodCall(kService, kManagerPath, kManagerIface,
                                              u"LoadUnit"_s);
    const QString name = fullUnitName(m_unit);
    msg << name;
    auto *watcher = new QDBusPendingCallWatcher(bus.asyncCall(msg), this);
    connect(watcher, &QDBusPendingCallWatcher::finished, this,
            [this, name](QDBusPendingCallWatcher *w) {
        w->deleteLater();
        QDBusPendingReply<QDBusObjectPath> reply = *w;
        if (name != fullUnitName(m_unit))
            return; // retargeted while the call was in flight
        if (reply.isError()) {
            qCWarning(lcService) << "LoadUnit" << name << "failed:" << reply.error().message();
            setAvailable(false);
            return;
        }
        m_unitPath = reply.value().path();
        QDBusConnection::sessionBus().connect(kService, m_unitPath, kPropsIface,
                u"PropertiesChanged"_s, this,
                SLOT(onPropertiesChanged(QString,QVariantMap,QStringList)));
        setAvailable(true);
        refreshState();
    });
}

void ServiceController::refreshState()
{
    auto msg = QDBusMessage::createMethodCall(kService, m_unitPath, kPropsIface, u"Get"_s);
    msg << kUnitIface << u"ActiveState"_s;
    auto *watcher = new QDBusPendingCallWatcher(
            QDBusConnection::sessionBus().asyncCall(msg), this);
    connect(watcher, &QDBusPendingCallWatcher::finished, this,
            [this](QDBusPendingCallWatcher *w) {
        w->deleteLater();
        QDBusPendingReply<QDBusVariant> reply = *w;
        if (reply.isError())
            qCWarning(lcService) << "reading ActiveState failed:" << reply.error().message();
        else
            setActiveState(reply.value().variant().toString());
    });
}

void ServiceController::onPropertiesChanged(const QString &interface,
                                            const QVariantMap &changed,
                                            const QStringList &invalidated)
{
    if (interface != kUnitIface)
        return;
    if (const auto it = changed.constFind(u"ActiveState"_s); it != changed.cend())
        setActiveState(it->toString());
    else if (invalidated.contains(u"ActiveState"_s))
        refreshState();
}

void ServiceController::setActiveState(const QString &state)
{
    if (m_activeState != state) {
        m_activeState = state;
        emit activeStateChanged(m_activeState);
    }
    const bool running = state == "active"_L1 || state == "activating"_L1
            || state == "reloading"_L1;
    if (m_running != running) {
        m_running = running;
        emit runningChanged(m_running);
    }
}

void ServiceController::setAvailable(bool available)
{
    if (m_available == available)
        return;
    m_available = available;
    emit availableChanged(m_available);
}

void ServiceController::setRunning(bool running)
{
    if (running == m_running)
        return;
    callManager(running ? u"StartUnit"_s : u"StopUnit"_s);
}

void ServiceController::restart()
{
    callManager(u"RestartUnit"_s);
}

void ServiceController::callManager(const QString &method)
{
    if (!m_available) {
        qCWarning(lcService) << method << m_unit << "ignored: systemd not reachable";
        return;
    }
    auto msg = QDBusMessage::createMethodCall(kService, kManagerPath, kManagerIface, method);
    msg << fullUnitName(m_unit) << u"replace"_s;
    qCDebug(lcService) << method << m_unit;
    auto *watcher = new QDBusPendingCallWatcher(
            QDBusConnection::sessionBus().asyncCall(msg), this);
    connect(watcher, &QDBusPendingCallWatcher::finished, this,
            [method](QDBusPendingCallWatcher *w) {
        w->deleteLater();
        QDBusPendingReply<QDBusObjectPath> reply = *w;
        if (reply.isError())
            qCWarning(lcService) << method << "failed:" << reply.error().message();
    });
}
