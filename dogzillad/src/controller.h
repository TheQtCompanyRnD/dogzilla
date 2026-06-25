// Copyright (C) 2026 The Qt Company Ltd.
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR BSD-3-Clause
#ifndef CONTROLLER_H
#define CONTROLLER_H

#include <QtQmlIntegration/qqmlintegration.h>
#include <QSerialPort>
#include <QTimerEvent>
#include <QMetaMethod>
#include <QQueue>
#include <cstdint>

class Controller : public QObject
{
    Q_OBJECT
    QML_ELEMENT
    Q_PROPERTY(QString serialPort READ serialPort WRITE setSerialPort CONSTANT FINAL)
    Q_PROPERTY(int baudRate READ baudRate WRITE setBaudRate CONSTANT FINAL)

    Q_PROPERTY(int batteryPercent READ batteryPercent NOTIFY batteryPercentChanged FINAL)
    Q_PROPERTY(bool motorsEngaged READ motorsEngaged WRITE setMotorsEngaged NOTIFY motorsEngagedChanged FINAL)

    Q_PROPERTY(qreal walkSpeed READ walkSpeed WRITE setWalkSpeed NOTIFY walkSpeedChanged FINAL)
    Q_PROPERTY(qreal steerAngle READ steerAngle WRITE setSteerAngle NOTIFY steerAngleChanged FINAL)
    Q_PROPERTY(qreal sideStepSpeed READ sideStepSpeed WRITE setSideStepSpeed NOTIFY sideStepSpeedChanged FINAL)

    Q_PROPERTY(qreal translationX READ translationX WRITE setTranslationX NOTIFY translationXChanged FINAL)
    Q_PROPERTY(qreal translationY READ translationY WRITE setTranslationY NOTIFY translationYChanged FINAL)
    Q_PROPERTY(qreal translationZ READ translationZ WRITE setTranslationZ NOTIFY translationZChanged FINAL)

    Q_PROPERTY(qreal roll READ roll WRITE setRoll NOTIFY rollChanged FINAL)
    Q_PROPERTY(qreal pitch READ pitch WRITE setPitch NOTIFY pitchChanged FINAL)
    Q_PROPERTY(qreal yaw READ yaw WRITE setYaw NOTIFY yawChanged FINAL)

    // IMU feedback (degrees), read-only and distinct from the roll/pitch/yaw
    // setpoints above. Polling is gated on whether anything is bound to these
    // (see connectNotify), since each axis is a separate serial read.
    //
    // These are relative to a tare reference: the firmware reports roll/pitch
    // with a large fixed bias (~+33/-37 deg when level), so we subtract an
    // offset captured on the first sample and re-capturable via tareAttitude().
    // There is no measuredYaw: the firmware's yaw is a free-running gyro integral
    // that drifts ~14 deg/s, so heading should come from odometry instead.
    Q_PROPERTY(qreal measuredRoll READ measuredRoll NOTIFY measuredRollChanged FINAL)
    Q_PROPERTY(qreal measuredPitch READ measuredPitch NOTIFY measuredPitchChanged FINAL)

    Q_PROPERTY(QList<double> jointAngles READ jointAngles WRITE setJointAngles NOTIFY jointAnglesChanged FINAL)

public:
    Controller(QObject * parent = nullptr);
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
        TranslationX,
        TranslationY,
        TranslationZ,
        AttitudeRoll,
        AttitudePitch,
        AttitudeYaw,
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

    QString serialPort() const { return m_serialPort; }
    int baudRate() const { return m_baudRate; }

    int batteryPercent() const { return m_batteryPercent; }
    bool motorsEngaged() const { return m_motorsEngaged; }
    qreal walkSpeed() const { return m_walkSpeed; }
    qreal steerAngle() const { return m_steerAngle; }
    qreal sideStepSpeed() const { return m_sideStepSpeed; }
    qreal translationX() const { return m_translationX; }
    qreal translationY() const { return m_translationY; }
    qreal translationZ() const { return m_translationZ; }
    qreal roll() const { return m_roll; }
    qreal pitch() const { return m_pitch; }
    qreal yaw() const { return m_yaw; }
    qreal measuredRoll() const { return m_measuredRoll; }
    qreal measuredPitch() const { return m_measuredPitch; }
    QList<double> jointAngles() const { return {m_motorAngles.begin(), m_motorAngles.end()}; }

public slots:
    void setSerialPort(const QString &path);
    void setBaudRate(int baud);

    void setMotorsEngaged(bool v);
    void stop();
    void setWalkSpeed(qreal v);
    void setSteerAngle(qreal v);
    void setSideStepSpeed(qreal v);
    void setMotorSpeed(qreal v);
    void setTranslationX(qreal v);
    void setTranslationY(qreal v);
    void setTranslationZ(qreal v);
    void setRoll(qreal newRoll);
    void setPitch(qreal newPitch);
    void setYaw(qreal newYaw);
    void setJointAngles(const QList<double> &angles);

    // Re-zero measuredRoll/measuredPitch to the dog's current attitude. Called
    // automatically ~2 s after standing up (see setMotorsEngaged), since the pose
    // at power-up may not be level; also exposed for an explicit re-zero on demand.
    void tareAttitude();

signals:
    void batteryPercentChanged(int pct);
    void motorsEngagedChanged(bool engaged);
    void walkSpeedChanged(qreal speed);
    void steerAngleChanged(qreal angle);
    void sideStepSpeedChanged(qreal angle);
    void translationXChanged(qreal translationX);
    void translationYChanged(qreal translationY);
    void translationZChanged(qreal translationZ);
    void rollChanged();
    void pitchChanged();
    void yawChanged();
    void measuredRollChanged();
    void measuredPitchChanged();
    void jointAnglesChanged();

protected:
    void timerEvent(QTimerEvent *ev) override;
    void connectNotify(const QMetaMethod &signal) override;
    void disconnectNotify(const QMetaMethod &signal) override;

private slots:
    void onError(QSerialPort::SerialPortError err);
    uint8_t checksum(const QByteArray &buf);
    void sendThunkCommand(Command cmd);
    void sendOneArgCommand(Command cmd, int8_t arg);
    void readAndHandle();
    void pollMotorAngles();
    void pollBattery();

private:
    void maybeOpenSerialPort();
    bool handleFrame(const QByteArray &frame);
    void handleMotorAngles(const QByteArray &packet);
    // Reads are serialized through a small queue so their replies can't
    // interleave and corrupt each other's checksums.
    void enqueueRead(Command cmd);
    void pumpReadQueue();
    void updateImuPolling();
    void scheduleImuPollingUpdate(); // defers updateImuPolling out of (dis)connectNotify

private:
    QString m_serialPort;
    qint32 m_baudRate = -1;
    QSerialPort m_port;
    qreal m_walkSpeed = 0;
    qreal m_steerAngle;
    qreal m_sideStepSpeed = 0;
    qreal m_translationX;
    qreal m_translationY;
    qreal m_translationZ;
    qreal m_roll;
    qreal m_pitch;
    qreal m_yaw;
    qreal m_measuredRoll = 0;  // tared (raw - offset); what measuredRoll exposes
    qreal m_measuredPitch = 0;
    qreal m_rawRoll = 0;       // latest raw firmware reading, before tare
    qreal m_rawPitch = 0;
    qreal m_rollOffset = 0;    // tare reference subtracted from the raw readings
    qreal m_pitchOffset = 0;
    bool m_rollTared = false;  // auto-tare to the first sample so the default zero is sane
    bool m_pitchTared = false;
    bool m_imuPollingUpdatePending = false; // a deferred updateImuPolling() is queued
    std::array<double, 12> m_motorAngles;
    int m_motorPollTimerId = -1;
    int m_imuPollTimerId = -1;
    int m_batteryPollCountdown = 0;
    int m_disengageCountdown = 0;
    int m_tareCountdown = 0; // ticks until auto-tare after standing up; 0 = idle
    uint8_t m_batteryPercent = 0;
    bool m_motorsEngaged = false; // we want to explicitly engage to start moving

    // Serialized read queue and the in-flight watchdog that recovers if a reply
    // is dropped (bad checksum, partial frame) and never clears m_readInFlight.
    QQueue<Command> m_readQueue;
    QByteArray m_rxBuffer; // accumulates serial bytes until whole frames can be parsed
    bool m_readInFlight = false;
    int m_readWatchdogTimerId = -1;
    QMetaMethod m_measuredRollSig;
    QMetaMethod m_measuredPitchSig;

    static QByteArray m_commands[int(Command::Count)];
    static RealPair m_motorLimits[3]; // lower, middle, upper motors on each leg
};

#endif  // CONTROLLER_H
