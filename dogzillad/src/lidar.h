// Copyright (C) 2026 The Qt Company Ltd.
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR BSD-3-Clause
#ifndef LIDAR_H
#define LIDAR_H

#include <QtQmlIntegration/qqmlintegration.h>
#include <QJsonObject>
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
    Q_PROPERTY(QString hardwareModel READ hardwareModel NOTIFY hardwareModelChanged FINAL)
    Q_PROPERTY(QString serialNumber READ serialNumber NOTIFY serialNumberChanged FINAL)

public:
    Lidar(QObject * parent = nullptr);
    ~Lidar();

    QString serialPort() const { return m_serialPort; }
    int baudRate() const { return m_baudRate; }

    bool isRunning() const { return m_running; }
    void setRunning(bool r);

    QString hardwareModel() const { return m_hardwareModel; }
    QString serialNumber() const { return m_serialNumber; }

public slots:
    void setSerialPort(const QString &path);

signals:
    void serialPortChanged();
    void runningChanged();
    void hardwareModelChanged();
    void serialNumberChanged();
    void sectorScanned(QJsonObject data);

private slots:
    void onError(QSerialPort::SerialPortError err);
    void readAndHandle();

private:
    bool maybeOpenSerialPort();
    void emitScanData(int startAngle, int endAngle, int datumAngleDelta,
                      int speed, void *distanceAndIntensity, int sampleCount);

private:
    QString m_hardwareModel;    // device may send this data after power-on and setRunning(true)
    QString m_serialNumber;     // device may send this data after power-on and setRunning(true)
    qreal m_rangeMin = 0.05;    // meters
    qreal m_rangeMax = 20;      // meters
    QString m_serialPort;
    QSerialPort m_port;
    qint32 m_baudRate = 230400; // no reason to change it
    quint16 m_lastTimeStamp = 0;
    bool m_running = false;     // could be in halfway state at startup: spinning but not sending data
};

#endif  // LIDAR_H
