// Copyright (C) 2026 The Qt Company Ltd.
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR BSD-3-Clause
import QtQml
import QtUniversalInput
import Dogzilla
import QtRos2.GeometryMsgs
import QtRos2.SensorMsgs

Ros2Node {
	id: root
	nodeName: "dogzilla"

	TwistPublisher {
		id: twp
		topic: `/${root.nodeName}/cmd_vel`
	}

	function publishTwist() {
		twp.publish({
						"linear": {
							"x": controller.walkSpeed,
							"y": controller.sideStepSpeed,
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
				"lf_lower_leg_joint",
				"lf_upper_leg_joint",
				"lf_hip_joint",

				"rf_lower_leg_joint",
				"rf_upper_leg_joint",
				"rf_hip_joint",

				"lh_lower_leg_joint",
				"lh_upper_leg_joint",
				"lh_hip_joint",

				"rh_lower_leg_joint",
				"rh_upper_leg_joint",
				"rh_hip_joint",
			],
			"position": controller.jointAngles
			// could also include velocity, effort
		}
		jsp.publish(msg)
	}

	BatteryStatePublisher {
		id: bsp
		topic: `/${root.nodeName}/battery_state`
	}

	// Ros2Node apparently only allows childEntities as children:
	// if we don't declare a property, we get
	// Cannot assign object of type "QQmlConnections" to list property "childEntities"; expected "QRos2Entity"
	property Controller controller: Controller {
		serialPort: "/dev/ttyAMA0"
		baudRate: 115200

		onBatteryPercentChanged: (pct) => {
			const msg = {
				"percentage": pct / 100,
				"present": true,
				// TODO voltage comes from /sys/devices/platform/soc/soc:rpi_rtc/rtc/rtc0/battery_voltage
				// TODO power_supply_status?
			}
			bsp.publish(msg)
		}

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
						controller.sideStepSpeed = value * -50
						break;
					case 1: // JoyAxis.LeftY
						controller.translationX = value * 100
						break;
					case 2: // JoyAxis.RightX
						controller.walkSpeed = value * -50
						break;
					case 4: // should be RightY
						controller.steerAngle = (value - 0.5) * -90
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
						controller.motorsEngaged = !controller.motorsEngaged
						break
					case 4: // JoyButton.Back
						controller.stop()
						break
				}
			}
	}
}
