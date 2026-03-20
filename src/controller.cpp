// Copyright (C) 2026 The Qt Company Ltd.
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR BSD-3-Clause
#include "controller.h"
#include <QDebug>

QByteArray Controller::m_commands[] {
    {}, // None
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
    QByteArrayLiteral("\x01\x30\x80"), // VX
    QByteArrayLiteral("\x31\x80"), // VY
    QByteArrayLiteral("\x32\x80"), // VYaw
    QByteArrayLiteral("\x33\x00\x00\x00"), // Translation
    QByteArrayLiteral("\x36\x00\x00\x00"), // Attitude
    QByteArrayLiteral("\x39\x00\x00\x00"), // PeriodicRotation
    QByteArrayLiteral("\x3c\x00"), // MarkTime
    QByteArrayLiteral("\x3d\x00"), // MoveMode
    QByteArrayLiteral("\x3e\x00"), // Action
    QByteArrayLiteral("\x80\x00\x00\x00"), // PeriodicTranslate
    QByteArrayLiteral("\x50\x80\x80\x80\x80\x80\x80\x80\x80\x80\x80\x80\x80"), // MotorAngle
    QByteArrayLiteral("\x5c\x01"), // MotorSpeed
    QByteArrayLiteral("\x40\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00"), // LegPos
    QByteArrayLiteral("\x61\x00"), // GetIMU
    QByteArrayLiteral("\x62\x00"), // Roll
    QByteArrayLiteral("\x63\x00"), // Pitch
    QByteArrayLiteral("\x64\x00"), // Yaw
};

Controller::Controller(const QString &serialPort, qint32 baudRate, QObject * parent)
  : QObject(parent), m_port(serialPort, this)
{
    m_port.setBaudRate(baudRate);
    connect(&m_port, &QSerialPort::errorOccurred, this, &Controller::onError);
    connect(&m_port, &QIODevice::readyRead, this, &Controller::readAndHandle);
    // connect(this, &QSerialPort::dataTerminalReadyChanged, this, &Controller::emitReadySend);
    const bool success = m_port.open(QIODevice::ReadWrite);
    qDebug() << m_port.portName() << m_port.baudRate() << "opened successfully?" << success;
    pollBattery(); // TODO periodically when otherwise idle
    // setMotorsEngaged(false); // TODO after being idle for some minutes
}

Controller::~Controller() {}

void Controller::onError(QSerialPort::SerialPortError err)
{
    qDebug() << err;
}

uint8_t Controller::checksum(const QByteArray &buf)
{
    uint8_t sum = buf.size() + 6;
    for (const auto byte : buf)
        sum += uint8_t(byte);
    return uint8_t(255) - sum;
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
    qDebug() << "wrote" << m_port.write(buf) << "bytes:" << m_commands[int(cmd)].toHex() << buf.toHex();
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
    qDebug() << "wrote" << m_port.write(buf) << "bytes:" << m_commands[int(cmd)].toHex() << buf.toHex();
}

void Controller::pollBattery()
{
    // should be (from python) [0x55 0x0 0x9  0x2 0x1 0x1 0xf2 0x0 0xaa]
    sendThunkCommand(Command::GetBatteryLevel);
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
    qDebug() << buf.toHex() << "len" << len << "exchk" << Qt::hex << expectedChecksum;
    if (buf.at(3) == 0x12) {
	    const uint8_t addr = buf.at(4);
		switch(addr) {
			case 0x01:
                m_batteryPercent = buf.at(5);
                qDebug() << "batt" << m_batteryPercent << "pct";
                emit batteryPercentChanged(m_batteryPercent);
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

    // TODO crouch down before disengaging
qDebug() << m_motorsEngaged << "->" << v;
    sendThunkCommand(v ? Command::LoadMotor : Command::UnloadMotor);
    m_motorsEngaged = v;
    emit motorsEngagedChanged(v);
}

// from python: speed 10 [0x55 0x0 0x9 0x1 0x30 0xb3 0x12 0x0 0xaa]
//                  stop [0x55 0x0 0x9 0x1 0x30 0x80 0x45 0x0 0xaa]
void Controller::setWalkingSpeed(qreal speed)
{
    if (qFuzzyCompare(m_walkingSpeed, speed))
        return;

    setMotorsEngaged(true);
    m_walkingSpeed = speed;
    uint8_t arg = 0x80 + lroundf(m_walkingSpeed);
    qDebug() << "move_x" << m_walkingSpeed << lroundf(m_walkingSpeed) << arg;
    sendOneArgCommand(Command::VX, arg);
    emit walkingSpeedChanged(m_walkingSpeed);
}
