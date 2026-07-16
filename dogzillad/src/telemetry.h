// Copyright (C) 2026 The Qt Company Ltd.
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR BSD-3-Clause
#ifndef TELEMETRY_H
#define TELEMETRY_H

#include <QtQmlIntegration/qqmlintegration.h>
#include <QObject>
#include <QBasicTimer>

// Periodically samples system telemetry -- fan level, CPU temperature and CPU
// load -- from sysfs/procfs, for publishing on ROS topics (so the digital twin
// can chart e.g. PTT silencing the fan and the temperature/CPU response).
// Emits sampled() every intervalMs; read fanLevel/temperatureC/cpuPercent in
// the handler and publish them (the bridge's single-field publishers are
// publish()-only, so this stays imperative rather than a declarative binding).
class Telemetry : public QObject
{
    Q_OBJECT
    QML_ELEMENT
    Q_PROPERTY(int intervalMs READ intervalMs WRITE setIntervalMs NOTIFY intervalMsChanged FINAL)
    // These share the sampled() notifier: they all refresh together each tick.
    Q_PROPERTY(int fanLevel READ fanLevel NOTIFY sampled FINAL)
    Q_PROPERTY(qreal temperatureC READ temperatureC NOTIFY sampled FINAL)
    Q_PROPERTY(qreal cpuPercent READ cpuPercent NOTIFY sampled FINAL)
public:
    explicit Telemetry(QObject *parent = nullptr);

    int intervalMs() const { return m_intervalMs; }
    void setIntervalMs(int ms);
    int fanLevel() const { return m_fanLevel; }
    qreal temperatureC() const { return m_temperatureC; }
    qreal cpuPercent() const { return m_cpuPercent; }

signals:
    void sampled();
    void intervalMsChanged();

protected:
    void timerEvent(QTimerEvent *event) override;

private:
    void sample();
    static int readFanLevel();
    static qreal readTemperatureC();
    qreal readCpuPercent();   // stateful: percentage over the interval

    QBasicTimer m_timer;
    int m_intervalMs = 1000;
    int m_fanLevel = 0;
    qreal m_temperatureC = 0.0;
    qreal m_cpuPercent = 0.0;
    quint64 m_prevTotal = 0;
    quint64 m_prevIdle = 0;
};

#endif // TELEMETRY_H
