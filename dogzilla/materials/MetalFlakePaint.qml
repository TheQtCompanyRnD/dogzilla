import QtQuick3D

PrincipledMaterial {
	id: metalFlakePaint
	baseColorMap: Texture {
		source: "paint-tile.png"
		tilingModeHorizontal: Texture.Repeat
		tilingModeVertical: Texture.Repeat
		scaleU: 20
		scaleV: 20
	}

	metalness: 0.5
	roughness: 0.5
	roughnessMap: Texture {
		source: "paint-tile-bumpmap.png"
		scaleU: 20
		scaleV: 20
	}
}
