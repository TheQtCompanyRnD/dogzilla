// Copyright (C) 2026 The Qt Company Ltd.
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR BSD-3-Clause
#ifndef CONTROLLER_H
#define CONTROLLER_H

#include <QSerialPort>

class Controller : public QObject
{
	Q_OBJECT

public:
	Controller(const QString &serialPort, qint32 baudRate, QObject * parent = nullptr);
	~Controller();

private slots:
	void onError(QSerialPort::SerialPortError err);
	void poll();
	void readAndHandle();

private:
	QSerialPort m_port;
};

#endif  // CONTROLLER_H
