import QtQuick
import QtQuick3D
import QtQuick3D.Helpers

View3D {
	id: view3D
	width: 1600; height: 1600

	camera: sceneCamera
	environment: sceneEnvironment

	SceneEnvironment {
		id: sceneEnvironment
		antialiasingMode: SceneEnvironment.MSAA
		antialiasingQuality: SceneEnvironment.High
	}

	Node {
		id: cameraNode
		eulerRotation: Qt.vector3d(-30, 30, 0)
		PerspectiveCamera {
			id: sceneCamera
			z: 1000
		}

		DirectionalLight {
			id: directionalLight
			eulerRotation: Qt.vector3d(-30, 30, 0)
		}
	}

	MetalFlakePaint {
		id: metalFlakePaint
	}

	Model {
		id: imu_link
		source: "#Cube"
		scale: Qt.vector3d(3, 3, 3)
		materials: metalFlakePaint
	}

	Model {
		source: "#Sphere"
		scale: Qt.vector3d(4, 4, 4)
		position: Qt.vector3d(0, 0, -400)
		materials: metalFlakePaint
	}

	OrbitCameraController {
		origin: cameraNode
		camera: sceneCamera
	}
}
