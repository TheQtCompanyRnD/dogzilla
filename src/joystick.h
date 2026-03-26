// Copyright (C) 2026 The Qt Company Ltd.
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR BSD-3-Clause
#ifndef JOYSTICK_H
#define JOYSTICK_H

#include <QtUniversalInput/QUniversalInput>

class Controller;

class JoystickHandler : public QObject
{
	Q_OBJECT

public:
	JoystickHandler(Controller *controller, QObject * parent = nullptr);
	~JoystickHandler();

private:
	void onConnectionChanged(int joyId, bool connected);
	void onButtonEvent(int device, JoyButton button, bool pressed);
	void onAxisEvent(int device, JoyAxis axis, float value);

private:
	QUniversalInput *m_dev = QUniversalInput::instance();
	Controller *m_controller = nullptr;
};

#endif  // JOYSTICK_H
