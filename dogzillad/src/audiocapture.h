// Copyright (C) 2026 The Qt Company Ltd.
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR BSD-3-Clause
#ifndef AUDIOCAPTURE_H
#define AUDIOCAPTURE_H

#include <QtQmlIntegration/qqmlintegration.h>
#include <QObject>
#include <QByteArray>
#include <QAudioFormat>
#include <QtMultimedia/qaudio.h>   // QtAudio::State / QtAudio::Error

QT_BEGIN_NAMESPACE
class QAudioSource;
class QIODevice;
QT_END_NAMESPACE

// Captures raw microphone PCM into memory with QAudioSource -- QtMultimedia has
// no QML API for raw capture (its only QML path, CaptureSession+MediaRecorder,
// writes an encoded file). Fixed at 16 kHz / mono / int16, i.e. exactly what
// WhisperSpeechToText consumes. Drive from QML via `listening`; on the
// true->false edge it emits captured() with the buffered PCM (no temp file).
//
// `listening` reports the REAL capture state, not the request: writing true is a
// request that only latches (and notifies) once the QAudioSource actually opens,
// so a failed start (no input device, backend error) leaves it false. If the
// source later stops on its own (device lost), listening flips back to false and
// captured() fires with whatever was buffered. This keeps /dogzilla/speech/state
// honest -- it can't claim "listening" while the mic never opened.
class AudioCapture : public QObject
{
    Q_OBJECT
    QML_ELEMENT
    Q_PROPERTY(bool listening READ listening WRITE setListening NOTIFY listeningChanged FINAL)
public:
    explicit AudioCapture(QObject *parent = nullptr);
    ~AudioCapture() override;

    bool listening() const { return m_listening; }

public slots:
    void setListening(bool listening);

signals:
    // Raw signed-16-bit little-endian mono PCM at 16 kHz (WHISPER_SAMPLE_RATE).
    void captured(const QByteArray &pcm);
    void listeningChanged(bool listening);

private slots:
    void onReadyRead();
    void onStateChanged(QtAudio::State state);

private:
    bool start();   // true if capture actually opened
    void stop();

    bool m_listening = false;
    QAudioFormat m_format;
    QAudioSource *m_source = nullptr;
    QIODevice *m_io = nullptr;   // pull-mode device, owned by m_source
    QByteArray m_buffer;
};

#endif // AUDIOCAPTURE_H
