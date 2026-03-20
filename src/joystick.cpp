// Copyright (C) 2026 The Qt Company Ltd.
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR BSD-3-Clause
#include "joystick.h"
#include <QDebug>

JoystickHandler::JoystickHandler(Controller *controller, QObject * parent)
  : QObject(parent)
  , m_controller(controller)
{
	connect(m_dev, &QUniversalInput::joyConnectionChanged,
			this, &JoystickHandler::onConnectionChanged);
	connect(m_dev, &QUniversalInput::joyButtonEvent,
			this, &JoystickHandler::onButtonEvent);
	connect(m_dev, &QUniversalInput::joyAxisEvent,
			this, &JoystickHandler::onAxisEvent);
}

JoystickHandler::~JoystickHandler() {}

void JoystickHandler::onConnectionChanged(int joyId, bool connected)
{
	qDebug() << "joy" << joyId << "connected:" << connected;
}

void JoystickHandler::onButtonEvent(int device, JoyButton button, bool pressed)
{
	qDebug() << "Device: " << device << "button: " << int(button) << ( pressed ? "pressed" : "released");
	// Nintendo-pad: up 0 down 12 left 13 right 14
	// upper shoulders: 9 left 10 right
	// select 4 start 6
	// x 2 y 3 a 0 b 1
}

void JoystickHandler::onAxisEvent(int device, JoyAxis axis, float value)
{
	qDebug() << "Device: " << device << "axis: " << int(axis) << "value: " << value;
	// left: axis 0 horizontal, left negative right positive
	//       axis 1 vertical, up negative down positive
	// right joystick not working!
	// lower shoulder left: axis 4
	// lower shoulder right: axis 5
}
