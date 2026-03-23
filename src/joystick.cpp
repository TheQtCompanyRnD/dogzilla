// Copyright (C) 2026 The Qt Company Ltd.
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR BSD-3-Clause
#include "joystick.h"
#include "controller.h"
#include <QDebug>

/*!
    Note: to get xbox mode, hold down the mode button on the controller to
    switch to the mode where the green LED is lit. The default mode with the
    red LED is not as useful: the right joystick doesn't work, etc.
*/

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
    // select 4 start 6 mode 5
    // x 2 y 3 a 0 b 1

    if (pressed) {
        switch (button) {
        case JoyButton::Back:   //labeled Select on the actual controller
            m_controller->stop();
            break;
        case JoyButton::Start:
            m_controller->setMotorsEngaged(!m_controller->motorsEngaged());
            break;
        }
    }
}

void JoystickHandler::onAxisEvent(int device, JoyAxis axis, float value)
{
	qDebug() << "Device: " << device << "axis: " << int(axis) << "value: " << value;
	// left: axis 0 horizontal, left negative right positive
	//       axis 1 vertical, up negative down positive
    // right: axis 2 horizontal, left 0 middle 0.5 right 0
    //       axis 4 vertical, up negative down positive
    // lower shoulder left: axis 3
	// lower shoulder right: axis 5
	switch (axis) {
    case JoyAxis::LeftX:
        qDebug() << "sidestep";
        m_controller->setSideStepSpeed(value * -50.0f);
        break;
    case JoyAxis::RightX:
        qDebug() << "walk";
        m_controller->setWalkSpeed(value * -50.0f);
        break;
    case JoyAxis::TriggerLeft: // bug workaround: should be RightY
        qDebug() << "turn" << value;
        m_controller->setSteerAngle((value - 0.5) * 90.0f);
        break;
	}
}
