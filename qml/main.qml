// Copyright (C) 2026 The Qt Company Ltd.
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR BSD-3-Clause
import QtQml
import QtUniversalInput
import Dogzilla
import QtRos2.GeometryMsgs
import QtRos2.SensorMsgs

ROS2Node {
	id: root
	nodeName: "dogzilla"
	Component.onCompleted: {
		console.log("hello objects", Controller, univin, jsp)
		console.log("topics", jsp.topic, twp.topic)
	}

	TwistPublisher {
		id: twp
		topic: `/${root.nodeName}/cmd_vel`
	}

	function publishTwist() {
		twp.publish({
						"linear": {
							"x": Controller.walkSpeed,
							"y": Controller.sideStepSpeed,
							"z": 0
						},
						"angular": {
							"x": 0,
							"y": 0,
							"z": 0
						}
					})
	}

	JointStatePublisher {
		id: jsp
		topic: `/${root.nodeName}/joint_states`
	}

	function publishJointState() {
		const msg = {
			"name": [
				"lf_hip_joint",
				"lf_upper_leg_joint",
				"lf_lower_leg_joint",
				"rf_hip_joint",
				"rf_upper_leg_joint",
				"rf_lower_leg_joint",
				"lh_hip_joint",
				"lh_upper_leg_joint",
				"lh_lower_leg_joint",
				"rh_hip_joint",
				"rh_upper_leg_joint",
				"rh_lower_leg_joint"
			],
			"position": Controller.jointAngles
			// could also include velocity, effort
		}
		jsp.publish(msg)
	}

	// ROS2Node apparently only allows childEntities as children:
	// if we don't declare a property, we get
	// Cannot assign object of type "QQmlConnections" to list property "childEntities"; expected "QRos2Entity"
	// And we need Connections only because Controller is a singleton
	property Connections conn: Connections {
		target: Controller

		// sigh: Implicitly defined onFoo properties in Connections are deprecated. Use this syntax instead: function onFoo(<arguments>) { ... }
		onWalkSpeedChanged: (speed) => {
			publishTwist()
			console.log("Walk speed changed", speed);
		}
		onSideStepSpeedChanged: publishTwist()
		onJointAnglesChanged: publishJointState()
	}

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
