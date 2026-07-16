// Copyright (C) 2026 The Qt Company Ltd.
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR BSD-3-Clause
#include "telemetry.h"

#include <QTimerEvent>
#include <QFile>

namespace {
// cooling_device0 is the chassis fan; thermal_zone0 is cpu-thermal (confirmed
// on the Pi 5). dogzilla-fan drives cur_state, so this reflects PTT silencing.
constexpr auto kFanState   = "/sys/class/thermal/cooling_device0/cur_state";
constexpr auto kTempMilliC = "/sys/class/thermal/thermal_zone0/temp";
}

Telemetry::Telemetry(QObject *parent) : QObject(parent)
{
    readCpuPercent();   // prime the /proc/stat baseline; first tick has a valid delta
    m_timer.start(m_intervalMs, this);
}

void Telemetry::setIntervalMs(int ms)
{
    if (ms <= 0 || ms == m_intervalMs)
        return;
    m_intervalMs = ms;
    m_timer.start(m_intervalMs, this);
    emit intervalMsChanged();
}

void Telemetry::timerEvent(QTimerEvent *event)
{
    if (event->timerId() == m_timer.timerId())
        sample();
}

void Telemetry::sample()
{
    m_fanLevel     = readFanLevel();
    m_temperatureC = readTemperatureC();
    m_cpuPercent   = readCpuPercent();
    emit sampled();
}

int Telemetry::readFanLevel()
{
    QFile f(QString::fromLatin1(kFanState));
    if (!f.open(QIODevice::ReadOnly))
        return -1;
    return f.readAll().trimmed().toInt();
}

qreal Telemetry::readTemperatureC()
{
    QFile f(QString::fromLatin1(kTempMilliC));
    if (!f.open(QIODevice::ReadOnly))
        return 0.0;
    return f.readAll().trimmed().toDouble() / 1000.0;   // millidegrees -> C
}

qreal Telemetry::readCpuPercent()
{
    QFile f(QStringLiteral("/proc/stat"));
    if (!f.open(QIODevice::ReadOnly))
        return m_cpuPercent;
    // First line: "cpu  user nice system idle iowait irq softirq steal ..."
    const QList<QByteArray> parts = f.readLine().simplified().split(' ');
    if (parts.size() < 5 || parts.first() != "cpu")
        return m_cpuPercent;

    quint64 total = 0;
    for (int i = 1; i < parts.size(); ++i)
        total += parts.at(i).toULongLong();
    const quint64 idle = parts.at(4).toULongLong();   // idle jiffies

    const quint64 dTotal = total - m_prevTotal;
    const quint64 dIdle  = idle - m_prevIdle;
    m_prevTotal = total;
    m_prevIdle = idle;
    if (dTotal == 0)
        return m_cpuPercent;
    return 100.0 * qreal(dTotal - dIdle) / qreal(dTotal);
}
