import QtQuick3D

PrincipledMaterial {
	baseColorMap: Texture {
		source: "paint-tile.png"
		tilingModeHorizontal: Texture.Repeat
		tilingModeVertical: Texture.Repeat
	}

	metalness: 0.5
	roughness: 0.5
	roughnessMap: Texture {
		source: "paint-tile-bumpmap.png"
	}
}
