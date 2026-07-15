// Copyright (C) 2026 The Qt Company Ltd.
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR BSD-3-Clause
#include "whisperspeechtotext.h"

#include <QtConcurrent/QtConcurrent>
#include <QLoggingCategory>

#include <whisper.h>

#include <vector>

Q_LOGGING_CATEGORY(lcStt, "dogzilla.stt")

WhisperSpeechToText::WhisperSpeechToText(QObject *parent) : QObject(parent) {}

WhisperSpeechToText::~WhisperSpeechToText()
{
    if (m_ctx)
        whisper_free(m_ctx);
}

void WhisperSpeechToText::setModelPath(const QString &path)
{
    if (m_modelPath == path)
        return;
    m_modelPath = path;
    // Don't touch m_ctx here (it's worker-owned); ensureContext() notices the
    // path changed and reloads on the next transcribe().
    emit modelPathChanged(m_modelPath);
}

// Worker-thread only. Loads the model if not already loaded (or if the path
// changed). Returns true if m_ctx is ready.
bool WhisperSpeechToText::ensureContext(const QString &path)
{
    if (m_ctx && m_loadedPath == path)
        return true;
    if (m_ctx) {
        whisper_free(m_ctx);
        m_ctx = nullptr;
    }
    if (path.isEmpty()) {
        emit errorOccurred(QStringLiteral("no modelPath set"));
        return false;
    }
    whisper_context_params cparams = whisper_context_default_params();
    m_ctx = whisper_init_from_file_with_params(path.toLocal8Bit().constData(), cparams);
    if (!m_ctx) {
        emit errorOccurred(QStringLiteral("failed to load whisper model: ") + path);
        return false;
    }
    m_loadedPath = path;
    qCDebug(lcStt) << "loaded model" << path;
    return true;
}

void WhisperSpeechToText::transcribe(const QByteArray &pcmS16)
{
    bool expected = false;
    if (!m_busy.compare_exchange_strong(expected, true)) {
        qCWarning(lcStt) << "still transcribing; dropping utterance";
        return;
    }
    emit busyChanged(true);

    const QString modelPath = m_modelPath;   // snapshot for the worker
    (void)QtConcurrent::run([this, pcmS16, modelPath]() {
        QString text;
        if (ensureContext(modelPath)) {
            // int16 -> normalised float [-1, 1], mono
            const auto *s = reinterpret_cast<const qint16 *>(pcmS16.constData());
            const int n = int(pcmS16.size() / sizeof(qint16));
            std::vector<float> pcmf(n);
            for (int i = 0; i < n; ++i)
                pcmf[i] = float(s[i]) / 32768.0f;

            whisper_full_params p = whisper_full_default_params(WHISPER_SAMPLING_GREEDY);
            p.n_threads         = 4;             // Pi 5 has 4 A76 cores
            p.greedy.best_of    = 1;
            p.beam_search.beam_size = 1;
            p.language          = "en";
            p.translate         = false;
            p.no_timestamps     = true;
            p.single_segment    = true;          // short PTT utterances
            p.print_progress    = false;
            p.print_realtime    = false;
            p.print_timestamps  = false;

            if (whisper_full(m_ctx, p, pcmf.data(), n) == 0) {
                const int segs = whisper_full_n_segments(m_ctx);
                for (int i = 0; i < segs; ++i)
                    text += QString::fromUtf8(whisper_full_get_segment_text(m_ctx, i));
                text = text.trimmed();
            } else {
                emit errorOccurred(QStringLiteral("whisper_full() failed"));
            }
        }

        m_busy.store(false);
        // Emitted from the worker thread; queued to the main thread because
        // this QObject lives there, so QML handlers run on the GUI thread.
        emit busyChanged(false);
        if (!text.isEmpty()) {
            qCDebug(lcStt) << "transcript:" << text;
            emit transcriptReady(text);
        }
    });
}
