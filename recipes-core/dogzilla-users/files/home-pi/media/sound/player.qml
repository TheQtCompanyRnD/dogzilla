import QtQml
import QtMultimedia

MediaPlayer {
    audioOutput: AudioOutput {}
    source: "file:Who Let The Dogs Out.mp3"
    Component.onCompleted: {
        play();
    }
}
