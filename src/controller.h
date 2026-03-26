// Copyright (C) 2026 The Qt Company Ltd.
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR BSD-3-Clause
#ifndef CONTROLLER_H
#define CONTROLLER_H

#include <QSerialPort>
#include <QTimerEvent>
#include <cstdint>

class Controller : public QObject
{
	Q_OBJECT
    Q_PROPERTY(int batteryPercent READ batteryPercent NOTIFY batteryPercentChanged FINAL)
    Q_PROPERTY(bool motorsEngaged READ motorsEngaged WRITE setMotorsEngaged NOTIFY motorsEngagedChanged FINAL)
    Q_PROPERTY(qreal walkSpeed READ walkSpeed WRITE setWalkSpeed NOTIFY walkSpeedChanged FINAL)
    Q_PROPERTY(qreal steerAngle READ steerAngle WRITE setSteerAngle NOTIFY steerAngleChanged FINAL)
    Q_PROPERTY(qreal sideStepSpeed READ sideStepSpeed WRITE setSideStepSpeed NOTIFY sideStepSpeedChanged FINAL)

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
        LoadMotor,
        UnloadMotor,
        VelX,
        VelY,
        VelYaw,
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

    typedef QPair<qreal, qreal> RealPair;

    int batteryPercent() const { return m_batteryPercent; }
    bool motorsEngaged() const { return m_motorsEngaged; }
    qreal walkSpeed() const { return m_walkSpeed; }
    qreal steerAngle() const { return m_steerAngle; }
    qreal sideStepSpeed() const { return m_sideStepSpeed; }

public slots:
    void setMotorsEngaged(bool v);
    void stop();
    void setWalkSpeed(qreal v);
    void setSteerAngle(qreal v);
    void setSideStepSpeed(qreal v);

signals:
    void batteryPercentChanged(int pct);
    void motorsEngagedChanged(bool engaged);
    void walkSpeedChanged(qreal speed);
    void steerAngleChanged(qreal angle);
    void sideStepSpeedChanged(qreal angle);

protected:
    virtual void timerEvent(QTimerEvent *ev);

private slots:
	void onError(QSerialPort::SerialPortError err);
    uint8_t checksum(const QByteArray &buf);
    void sendThunkCommand(Command cmd);
    void sendOneArgCommand(Command cmd, int8_t arg);
	void readAndHandle();
    void pollMotorAngles();
    void pollBattery();

private:
    void handleMotorAngles(const QByteArray &packet);

private:
    QSerialPort m_port;
    qreal m_walkSpeed = 0;
    qreal m_steerAngle;
    qreal m_sideStepSpeed = 0;
    std::array<qreal, 12> m_motorAngles;
    int m_motorPollTimerId = -1;
    uint8_t m_batteryPercent = 0;
    bool m_motorsEngaged = false; // we want to explicitly engage to start moving

    static QByteArray m_commands[int(Command::Count)];
    static RealPair m_motorLimits[3]; // lower, middle, upper motors on each leg
};

#endif  // CONTROLLER_H
