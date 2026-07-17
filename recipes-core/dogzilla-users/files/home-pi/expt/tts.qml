import QtQuick
import QtTextToSpeech

Item {
    TextToSpeech {
        id: tts
        Component.onCompleted: {
            console.log("engine:", engine, "engines:", availableEngines());
            console.log("voices:", availableVoices().map(v => v.name));
            say("Hello, I am Dogzilla. Nice to meet you!");
        }
        onStateChanged: console.log("state", state)
    }
}
