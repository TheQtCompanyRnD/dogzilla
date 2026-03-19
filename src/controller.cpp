// Copyright (C) 2026 The Qt Company Ltd.
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR BSD-3-Clause
#include "controller.h"
#include <QDebug>

Controller::Controller(const QString &serialPort, qint32 baudRate, QObject * parent)
  : QObject(parent), m_port(serialPort, this)
{
	m_port.setBaudRate(baudRate);
	connect(&m_port, &QSerialPort::errorOccurred, this, &Controller::onError);
	connect(&m_port, &QIODevice::readyRead, this, &Controller::readAndHandle);
	// connect(this, &QSerialPort::dataTerminalReadyChanged, this, &Controller::emitReadySend);
	const bool success = m_port.open(QIODevice::ReadWrite);
	qDebug() << m_port.portName() << m_port.baudRate() << "opened successfully?" << success;
	poll();
}

Controller::~Controller() {}

void Controller::onError(QSerialPort::SerialPortError err)
{
	qDebug() << err;
}

void Controller::poll()
{
	QByteArray cmd = QByteArrayLiteral("\x55\x00\x09\x02\x50\x0C\x98\x00\xAA");
	qDebug() << "wrote" << m_port.write(cmd);
}

void Controller::readAndHandle()
{
	QByteArray buf = m_port.readAll();
	qDebug() << buf.toHex();
	// e.g. "5500171250a1af829eae83a4c282a5c38200ff001400aa"
	// 55 00 17 are header and length; 14 00 aa are checksum and footer
	// meaning: command 0x12 and its data 50 a1 af 82 9e ae 83 a4 c2 82 a5 c3 82 00 ff 00
}
