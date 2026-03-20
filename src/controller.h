// Copyright (C) 2026 The Qt Company Ltd.
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR BSD-3-Clause
#ifndef CONTROLLER_H
#define CONTROLLER_H

#include <QSerialPort>
#include <cstdint>

class Controller : public QObject
{
	Q_OBJECT
    Q_PROPERTY(int batteryPercent READ batteryPercent NOTIFY batteryPercentChanged FINAL)

public:
	Controller(const QString &serialPort, qint32 baudRate, QObject * parent = nullptr);
	~Controller();

	enum class Command {
		None,
		GetBatteryLevel,
		Perform,
   		Calibrate,
     	Upgrade,
      	MoveTest,
       	GetFirmwareVersion,
        GaitType,
        BTName,
        UnloadMotor,
        LoadMotor,
        VX,
        VY,
        VYaw,
        Translation,
        Attitude,
        PeriodicRotation,
        MarkTime,
        MoveMode,
        Action,
        PeriodicTranslate,
        MotorAngle,
        MotorSpeed,
        LegPos,
        GetIMU,
        Roll,
        Pitch,
        Yaw,
        Count // count of commands in array
    };

    int batteryPercent() const { return m_batteryPercent; }

signals:
    void batteryPercentChanged(int pct);

private slots:
	void onError(QSerialPort::SerialPortError err);
    uint8_t checksum(const QByteArray &buf);
    void sendThunkCommand(Command cmd);
	void readAndHandle();
    void pollBattery();

private:
	QSerialPort m_port;
    uint8_t m_batteryPercent = 0;

    static QByteArray m_commands[int(Command::Count)];
};

#endif  // CONTROLLER_H
