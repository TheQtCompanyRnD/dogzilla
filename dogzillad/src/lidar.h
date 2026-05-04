// Copyright (C) 2026 The Qt Company Ltd.
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR BSD-3-Clause
#ifndef LIDAR_H
#define LIDAR_H

#include <QtQmlIntegration/qqmlintegration.h>
#include <QSerialPort>
#include <QTimerEvent>
#include <cstdint>

// serial interface to Oradar MS200 LiDAR scanner
class Lidar : public QObject
{
    Q_OBJECT
    QML_ELEMENT
    Q_PROPERTY(QString serialPort READ serialPort WRITE setSerialPort NOTIFY serialPortChanged FINAL)
    Q_PROPERTY(bool running READ isRunning WRITE setRunning NOTIFY runningChanged FINAL)

public:
    Lidar(QObject * parent = nullptr);
    ~Lidar();

    QString serialPort() const { return m_serialPort; }
    int baudRate() const { return m_baudRate; }

    bool isRunning() const { return m_running; }
    void setRunning(bool r);

public slots:
    void setSerialPort(const QString &path);

signals:
    void serialPortChanged();
    void runningChanged();

protected:

private slots:
    void onError(QSerialPort::SerialPortError err);
    void readAndHandle();

private:
    bool maybeOpenSerialPort();

private:
    QString m_serialNumber;     // device sends this packet after power-on and setRunning(true)
    QString m_serialPort;
    QSerialPort m_port;
    qint32 m_baudRate = 230400; // no reason to change it
    bool m_running = false;     // could be in halfway state at startup: spinning but not sending data
};

#endif  // LIDAR_H
