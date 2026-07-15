// Copyright (C) 2026 The Qt Company Ltd.
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR BSD-3-Clause
#ifndef WHISPERSPEECHTOTEXT_H
#define WHISPERSPEECHTOTEXT_H

#include <QtQmlIntegration/qqmlintegration.h>
#include <QObject>
#include <QByteArray>
#include <QString>
#include <atomic>

struct whisper_context;

// In-process offline speech-to-text via whisper.cpp (libwhisper). Feed it the
// 16 kHz mono int16 PCM emitted by AudioCapture; whisper_full() runs on a
// worker thread (it pegs all 4 cores for ~2 s) so it never blocks dogzillad's
// control/ROS event loop. The model is loaded once and kept warm.
class WhisperSpeechToText : public QObject
{
    Q_OBJECT
    QML_ELEMENT
    Q_PROPERTY(QString modelPath READ modelPath WRITE setModelPath NOTIFY modelPathChanged FINAL)
    Q_PROPERTY(bool busy READ busy NOTIFY busyChanged FINAL)
public:
    explicit WhisperSpeechToText(QObject *parent = nullptr);
    ~WhisperSpeechToText() override;

    QString modelPath() const { return m_modelPath; }
    void setModelPath(const QString &path);
    bool busy() const { return m_busy.load(); }

public slots:
    // Transcribe 16 kHz mono int16 PCM. Runs on a worker thread; if a previous
    // transcription is still running the new utterance is dropped (single-flight).
    void transcribe(const QByteArray &pcmS16);

signals:
    void transcriptReady(const QString &text);
    void modelPathChanged(const QString &path);
    void busyChanged(bool busy);
    void errorOccurred(const QString &message);

private:
    // All ctx access happens on the worker thread, serialised by m_busy.
    bool ensureContext(const QString &path);

    QString m_modelPath;
    QString m_loadedPath;             // worker-thread only
    whisper_context *m_ctx = nullptr; // worker-thread only
    std::atomic_bool m_busy = false;
};

#endif // WHISPERSPEECHTOTEXT_H
