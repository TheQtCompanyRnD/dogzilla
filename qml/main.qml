// Copyright (C) 2026 The Qt Company Ltd.
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR BSD-3-Clause
import QtQml
import QtUniversalInput
import Dogzilla

QtObject {
	Component.onCompleted: console.log("hello objects", Controller, univin)

	property UniversalInput univin: UniversalInput {
		/*!
			Note: to get xbox mode, hold down the mode button on the controller to
			switch to the mode where the green LED is lit. The default mode with the
			red LED is not as useful: the right joystick doesn't work, etc.
		*/
		onJoyAxisEvent:
			(device, axis, value) => {
				console.log("axis", axis, value)
				switch (axis) {
					case 0: // JoyAxis.LeftX
						Controller.sideStepSpeed = value * -50
						break;
					case 1: // JoyAxis.LeftY
						Controller.translationX = value * 100
						break;
					case 2: // JoyAxis.RightX
						Controller.walkSpeed = value * -50
						break;
					case 4: // should be RightY
						Controller.steerAngle = (value - 0.5) * -90
						break;
				}
			}
		onJoyButtonEvent:
			(device, button, isPressed) => {
				console.log("button", button, isPressed)
				if (!isPressed)
					return
				switch (button) {
					case 6: // JoyButton.Start
						Controller.motorsEngaged = !Controller.motorsEngaged
						break
					case 4: // JoyButton.Back
						Controller.stop()
						break
				}
			}
	}
}
