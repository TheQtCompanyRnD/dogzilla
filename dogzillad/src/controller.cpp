// Copyright (C) 2026 The Qt Company Ltd.
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR BSD-3-Clause
#include "controller.h"
#include <QDebug>
#include <QLoggingCategory>

Q_STATIC_LOGGING_CATEGORY(lcCtrl, "dogzilla.controller")
Q_STATIC_LOGGING_CATEGORY(lcCrLow, "dogzilla.controller.lolevel")

QByteArray Controller::m_commands[] {
    { }, // None
    // mode, addr, read_len
    // mode 1: send (command); mode 2: read (value)
    QByteArrayLiteral("\x02\x01\x01"), // GetBatteryLevel; expect to read 1 byte
    QByteArrayLiteral("\x01\x03\x00"), // Perform
    QByteArrayLiteral("\x04\x00"), // Calibrate
    QByteArrayLiteral("\x05\x00"), // Upgrade
    QByteArrayLiteral("\x06\x01"), // MoveTest
    QByteArrayLiteral("\x07"), // GetFirmwareVersion
    QByteArrayLiteral("\x09\x00"), // GaitType
    QByteArrayLiteral("\x13\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00"), // BTName
    QByteArrayLiteral("\x01\x20\x00"), // LoadMotor:   0 means all; otherwise use 0x20 + leg ID
    QByteArrayLiteral("\x01\x20\x01"), // UnloadMotor: 1 means all; otherwise use 0x10 + leg ID
    QByteArrayLiteral("\x01\x30\x80"), // VelX
    QByteArrayLiteral("\x01\x31\x80"), // VelY
    QByteArrayLiteral("\x01\x32\x80"), // VYaw
    QByteArrayLiteral("\x01\x33\x00"), // TranslationX
    QByteArrayLiteral("\x01\x34\x00"), // TranslationY
    QByteArrayLiteral("\x01\x35\x00"), // TranslationZ
    QByteArrayLiteral("\x36\x00\x00\x00"), // Attitude
    QByteArrayLiteral("\x39\x00\x00\x00"), // PeriodicRotation
    QByteArrayLiteral("\x3c\x00"), // MarkTime
    QByteArrayLiteral("\x3d\x00"), // MoveMode
    QByteArrayLiteral("\x3e\x00"), // Action
    QByteArrayLiteral("\x80\x00\x00\x00"), // PeriodicTranslate
    QByteArrayLiteral("\x02\x50\x0c"), // get MotorAngle; expect to read 12 bytes
    QByteArrayLiteral("\x01\x5c\x01"), // MotorSpeed
    QByteArrayLiteral("\x40\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00"), // LegPos
    QByteArrayLiteral("\x61\x00"), // GetIMU
    QByteArrayLiteral("\x62\x00"), // Roll
    QByteArrayLiteral("\x63\x00"), // Pitch
    QByteArrayLiteral("\x64\x00"), // Yaw
};

/* from Python:
    """
    PARAM 用来存放机器狗的参数限制范围
        PARAM is used to store the parameter limit range of the robot dog
    """

    PARAM = {
        "TRANSLATION_LIMIT": [35, 18, [75, 115]], # X Y Z 平移范围 Scope of translation
        "ATTITUDE_LIMIT": [20, 15, 11],           # Roll Pitch Yaw 姿态范围 Scope of posture
        "LEG_LIMIT": [35, 18, [75, 115]],         # 腿长范围 Scope of the leg
        "MOTOR_LIMIT": [[-73, 57], [-66, 93], [-31, 31]], # 下 中 上 舵机范围 Lower, middle and upper steering gear range
        "PERIOD_LIMIT": [[1.5, 8]],
        "MARK_TIME_LIMIT": [10, 35],  # 原地踏步高度范围 Stationary height range
        "VX_LIMIT": 25,    # X速度范围 X velocity range
        "VY_LIMIT": 18,    # Y速度范围 Y velocity range
        "VYAW_LIMIT": 100  # 旋转速度范围 Rotation speed range
    }
*/

Controller::RealPair Controller::m_motorLimits[] {
    {-73, 57},
    {-66, 93},
    {-31, 31}
};

static const QList<QByteArray> jointNames = {
	"lfLowerLeg",
	"lfUpperLeg",
	"lfHip",

	"rfLowerLeg",
	"rfUpperLeg",
	"rfHip",

	"lhLowerLeg",
	"lhUpperLeg",
	"lhHip",

	"rhLowerLeg",
	"rhUpperLeg",
	"rhHip",
};

Controller::Controller(QObject * parent)
  : QObject(parent)
{
    connect(&m_port, &QSerialPort::errorOccurred, this, &Controller::onError);
    connect(&m_port, &QIODevice::readyRead, this, &Controller::readAndHandle);
    // connect(this, &QSerialPort::dataTerminalReadyChanged, this, &Controller::emitReadySend);
}

Controller::~Controller() { }

void Controller::setSerialPort(const QString &path)
{
    m_serialPort = path;
    maybeOpenSerialPort();
}

void Controller::setBaudRate(int baud)
{
    m_baudRate = baud;
    maybeOpenSerialPort();
}

void Controller::maybeOpenSerialPort()
{
    if (!m_port.isWritable() && !m_serialPort.isEmpty() && m_baudRate > 0) {
        m_port.setPortName(m_serialPort);
        m_port.setBaudRate(m_baudRate);
        const bool success = m_port.open(QIODevice::ReadWrite);
        qCDebug(lcCtrl) << m_port.portName() << m_port.baudRate() << "opened successfully?" << success;
        if (success && !m_batteryPollCountdown)
            pollBattery();
    }
}

void Controller::onError(QSerialPort::SerialPortError err)
{
    qCWarning(lcCtrl) << err;
}

uint8_t Controller::checksum(const QByteArray &buf)
{
    uint8_t sum = buf.size() + 6;
    for (const auto byte : buf)
        sum += uint8_t(byte);
    return uint8_t(255) - sum;
}

void Controller::timerEvent(QTimerEvent *ev)
{
    if (ev->timerId() == m_motorPollTimerId) {
        pollMotorAngles();
    }
}

void Controller::sendThunkCommand(Command cmd)
{
    QByteArray buf(m_commands[int(cmd)]);
    QByteArray header = QByteArrayLiteral("\x55\x00\x00");
    header[2] = buf.size() + 6;
    QByteArray footer = QByteArrayLiteral("\x00\x00\xaa");
    footer[0] = checksum(buf);
    buf.prepend(header);
    buf.append(footer);
    maybeOpenSerialPort();
    const auto len = m_port.write(buf);
    qCDebug(lcCrLow) << "wrote" << len << "bytes:" << m_commands[int(cmd)].toHex() << buf.toHex();
}

void Controller::sendOneArgCommand(Command cmd, int8_t arg)
{
    QByteArray buf(m_commands[int(cmd)]);
    buf[buf.size() - 1] = arg;
    QByteArray header = QByteArrayLiteral("\x55\x00\x00");
    header[2] = buf.size() + 6;
    QByteArray footer = QByteArrayLiteral("\x00\x00\xaa");
    footer[0] = checksum(buf);
    buf.prepend(header);
    buf.append(footer);
    maybeOpenSerialPort();
    const auto len = m_port.write(buf);
    qCDebug(lcCrLow) << "wrote" << len << "bytes:" << m_commands[int(cmd)].toHex() << buf.toHex();
}

void Controller::pollMotorAngles()
{
    if (m_disengageCountdown > 0) {
        --m_disengageCountdown;
        if (m_disengageCountdown <= 10) {
                stop();
            sendThunkCommand(Command::UnloadMotor);
            m_motorsEngaged = false;
            qCDebug(lcCtrl) << m_motorsEngaged << "->" << false;
            emit motorsEngagedChanged(false);
        }
        if (m_disengageCountdown == 0) {
            killTimer(m_motorPollTimerId);
            m_motorPollTimerId = -1;
            return;
        }
    }

    //                           "55 00 09 02 01 50 a3 00 aa"
    // should be (from python) [0x55 0x0 0x9 0x2 0x50 0xc 0x98 0x0 0xaa]
    // response in standing pos \xb3\xaa\x80\xb1\xab\x7f\xb4\xb4\x83\xb4\xb4\x7f\x00\xff\x00 ...
    // meaning motor angles: [18.25, 40.0, 0.12, 17.24, 40.62, -0.12, 18.76, 46.24, 0.85, 18.76, 46.24, -0.12]
    sendThunkCommand(Command::MotorAngle);
}

void Controller::pollBattery()
{
    // should be (from python) [0x55 0x0 0x9  0x2 0x1 0x1 0xf2 0x0 0xaa]
    sendThunkCommand(Command::GetBatteryLevel);
}

// from Python:
// def conver2float(data, limit):
//     if not isinstance(limit, list):
//     return (data - 128.0) / 255.0 * limit
//     else:
//            limitmin = limit[0]
//     limitmax = limit[1]
//                    return data / 255.0 * (limitmax - limitmin) + limitmin
// read_motor: ...
// index = round(conver2float(self.rx_data[i], PARAM["MOTOR_LIMIT"][i % 3]), 2)
//         if out_int:
//                      if index > 0:
//                            angle.append(int(index+0.5))
//                            elif index < 0:
//                            angle.append(int(index-0.5))
//                            else:
//                                   angle.append(int(index))
//                                   else:
//                                          angle.append(index)

double byteToReal(uint8_t b, const Controller::RealPair &limits)
{
    const auto limitMin = limits.first;
    const auto limitMax = limits.second;
    return b / 255.0 * (limitMax - limitMin) + limitMin;
}

void Controller::handleMotorAngles(const QByteArray &packet)
{
    if (packet.size() < 18)
        return;

    bool changed = false;
    // example: 5500171250 b2 aa 7f b1 aa 80 b4 b6 7f b2 b4 7e 00ff000400aa
    // meaning: lower, middle, upper motor position on each leg (but we have more than 12?)
    // 17.7451, 40, -0.121569, 17.2353, 40, 0.121569, 18.7647, 47.4824, -0.121569, 17.7451, 46.2353, -0.364706
    for (int i = 0; i < 12; ++i) {
        const int perLegIdx = i % 3;
        int rawAngle = packet.at(i + 5);
        double v = byteToReal(rawAngle, m_motorLimits[perLegIdx]);
        // to put the lower leg at a 90 degree angle to the thigh, use +16 (from the range -73..57)
        // at that position, we report joint_state 90, not 0
        if (perLegIdx == 0)
            v -= 106.0;
        qCDebug(lcCrLow) << "   " << i << jointNames.at(i) << m_motorAngles[i] << "->" << Qt::hex << rawAngle << ":" << v;
        if (!changed && m_motorAngles[i] != v)
            changed = true;
        m_motorAngles[i] = v * M_PI / 180;
    }
    if (changed)
        emit jointAnglesChanged();

    // Polling the battery right after a reply is received
    // prevents problems with interleaving replies and getting bad checksums.
    // TODO Use a command queue instead.
    if (m_batteryPollCountdown > 0) {
        --m_batteryPollCountdown;
    } else {
        pollBattery();
        m_batteryPollCountdown = 100; // 100 * 100 ms = 10 sec
    }
}

void Controller::readAndHandle()
{
    QByteArray buf = m_port.readAll();
    // e.g. for battery level: 550009 12 01 18 cb 00aa"
    // 55 00 09 are header and length; cb 00 aa are checksum and footer
    // meaning: response 0x12 from addr 0x01: its data is 0x18, i.e. 24% battery
    uint8_t len = buf.at(2);
    uint8_t expectedChecksum = checksum(buf.sliced(3, buf.size() - 6));
    if (expectedChecksum != uint8_t(buf.at(buf.size() - 3))) {
        qWarning() << "ignoring message with bad checksum: expected" << Qt::hex << expectedChecksum << buf.toHex();
        return;
    }
    qCDebug(lcCrLow) << buf.toHex() << "len" << len << "exchk" << Qt::hex << expectedChecksum;
    if (buf.at(3) == 0x12) {
        const uint8_t addr = buf.at(4);
        switch(addr) {
        case 0x01:
            m_batteryPercent = buf.at(5);
            qCDebug(lcCtrl) << "batt" << m_batteryPercent << "pct";
            emit batteryPercentChanged(m_batteryPercent);
            break;
        case 0x50:
            handleMotorAngles(buf);
            break;
        }
    }
}

// from python: load all [0x55 0x0 0x9 0x1 0x20 0x0 0xd5 0x0 0xaa]
//            unload all [0x55 0x0 0x9 0x1 0x20 0x1 0xd4 0x0 0xaa]
void Controller::setMotorsEngaged(bool v)
{
    if (m_motorsEngaged == v)
        return;

    if (v) {
        if (m_motorPollTimerId < 0)
            m_motorPollTimerId = startTimer(100);
        setMotorSpeed(50);
        setTranslationZ(100); // stand up; TODO this doesn't go high enough
        sendThunkCommand(Command::LoadMotor);
        m_motorsEngaged = v;
        qCDebug(lcCtrl) << m_motorsEngaged << "->" << v;
        emit motorsEngagedChanged(v);
    } else {
        setTranslationZ(0); // crouch; TODO this doesn't go low enough
        m_disengageCountdown = 25; // ticks
    }
}

void Controller::stop()
{
    qCDebug(lcCtrl) << "STOP ALL";
    sendOneArgCommand(Command::VelX, 0x80);
    sendOneArgCommand(Command::VelY, 0x80);
    // TODO mark_time(0), turn(0)
}

// from python: speed 10 [0x55 0x0 0x9 0x1 0x30 0xb3 0x12 0x0 0xaa]
//                  stop [0x55 0x0 0x9 0x1 0x30 0x80 0x45 0x0 0xaa]
void Controller::setWalkSpeed(qreal v)
{
    if (qFuzzyCompare(m_walkSpeed, v))
        return;

    setMotorsEngaged(true);
    m_walkSpeed = v;
    uint8_t arg = 0x80 + lroundf(m_walkSpeed);
    qCDebug(lcCtrl) << "move_x" << m_walkSpeed << lroundf(m_walkSpeed) << arg;
    sendOneArgCommand(Command::VelX, arg);
    emit walkSpeedChanged(m_walkSpeed);
}

void Controller::setSteerAngle(qreal v)
{
    if (qFuzzyCompare(m_steerAngle, v))
        return;

    setMotorsEngaged(true);
    m_steerAngle = v;
    // TODO is v in degrees? convert it properly to what the controller firmware expects
    uint8_t arg = 0x80 + lroundf(v);
    qCDebug(lcCtrl) << "steer" << m_steerAngle << lroundf(v) << arg;
    sendOneArgCommand(Command::VelYaw, arg);
    emit steerAngleChanged(v);
}

void Controller::setSideStepSpeed(qreal v)
{
    if (qFuzzyCompare(m_steerAngle, v))
        return;

    setMotorsEngaged(true);
    m_sideStepSpeed = v;
    uint8_t arg = 0x80 + lroundf(v);
    qCDebug(lcCtrl) << "move_y" << m_sideStepSpeed << lroundf(v) << arg;
    sendOneArgCommand(Command::VelY, arg);
    emit sideStepSpeedChanged(v);
}

void Controller::setMotorSpeed(qreal v)
{
    if (qFuzzyCompare(m_steerAngle, v))
        return;

    uint8_t arg = 0x80 + lroundf(v);
    qCDebug(lcCtrl) << "speed" << lroundf(v) << arg;
    sendOneArgCommand(Command::MotorSpeed, arg);
}

void Controller::setTranslationX(qreal v)
{
    if (qFuzzyCompare(m_translationX, v))
        return;
    m_translationX = v;
    uint8_t arg = 0x80 + lroundf(v);
    qCDebug(lcCtrl) << "trans_x" << m_translationX << lroundf(v) << arg;
    sendOneArgCommand(Command::TranslationX, arg);
    emit translationXChanged(v);
}

void Controller::setTranslationY(qreal v)
{
    if (qFuzzyCompare(m_translationY, v))
        return;
    m_translationY = v;
    uint8_t arg = 0x80 + lroundf(v);
    qCDebug(lcCtrl) << "trans_y" << m_translationY << lroundf(v) << arg;
    sendOneArgCommand(Command::TranslationY, arg);
    emit translationYChanged(v);
}

// TODO define the allowed range;
// since the minimum is not low enough, calculate angles and set them directly
void Controller::setTranslationZ(qreal v)
{
    if (qFuzzyCompare(m_translationZ, v))
        return;
    m_translationZ = v;
    uint8_t arg = lroundf(v);
    qCDebug(lcCtrl) << "trans_z" << m_translationZ << lroundf(v) << arg;
    sendOneArgCommand(Command::TranslationZ, arg);
    emit translationZChanged(v);
}

void Controller::setJointAngles(const QList<double> &angles)
{
    if (jointAngles() == angles)
        return;
    // TODO send commands to change them
}
