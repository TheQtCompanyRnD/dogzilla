// Copyright (C) 2026 The Qt Company Ltd.
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR BSD-3-Clause
#include "audiocapture.h"

#include <QAudioSource>
#include <QAudioDevice>
#include <QMediaDevices>
#include <QIODevice>
#include <QLoggingCategory>

Q_LOGGING_CATEGORY(lcAudio, "dogzilla.audio")

static constexpr int kSampleRate = 16000;   // == whisper.cpp WHISPER_SAMPLE_RATE

AudioCapture::AudioCapture(QObject *parent) : QObject(parent)
{
    m_format.setSampleRate(kSampleRate);
    m_format.setChannelCount(1);
    m_format.setSampleFormat(QAudioFormat::Int16);
}

AudioCapture::~AudioCapture()
{
    stop();
}

void AudioCapture::setListening(bool listening)
{
    if (m_listening == listening)
        return;
    m_listening = listening;
    if (listening)
        start();
    else
        stop();
    emit listeningChanged(m_listening);
}

void AudioCapture::start()
{
    const QAudioDevice dev = QMediaDevices::defaultAudioInput();
    if (dev.isNull()) {
        qCWarning(lcAudio) << "no default audio input device";
        return;
    }

    QAudioFormat fmt = m_format;
    if (!dev.isFormatSupported(fmt)) {
        // The PipeWire/Pulse backend usually resamples for us, so this branch
        // is unlikely; if it fires, capture won't be at 16 kHz and whisper will
        // get garbled audio. TODO: resample to 16 kHz mono here if needed.
        qCWarning(lcAudio) << "16k/mono/int16 not supported by" << dev.description()
                           << "-- backend should resample; preferred is"
                           << dev.preferredFormat();
    }

    m_buffer.clear();
    delete m_source;
    m_source = new QAudioSource(dev, fmt, this);
    m_io = m_source->start();   // pull mode: QIODevice we read from
    if (!m_io) {
        qCWarning(lcAudio) << "QAudioSource failed to start";
        m_source->deleteLater();
        m_source = nullptr;
        return;
    }
    connect(m_io, &QIODevice::readyRead, this, &AudioCapture::onReadyRead);
    qCDebug(lcAudio) << "capture started"
                     << m_source->format().sampleRate() << "Hz"
                     << m_source->format().channelCount() << "ch";
}

void AudioCapture::onReadyRead()
{
    if (m_io)
        m_buffer.append(m_io->readAll());
}

void AudioCapture::stop()
{
    if (!m_source)
        return;
    if (m_io) {
        m_buffer.append(m_io->readAll());   // drain whatever's left
        disconnect(m_io, nullptr, this, nullptr);
    }
    m_source->stop();
    m_source->deleteLater();
    m_source = nullptr;
    m_io = nullptr;

    qCDebug(lcAudio) << "capture stopped," << m_buffer.size() << "bytes";
    if (!m_buffer.isEmpty())
        emit captured(m_buffer);
    m_buffer.clear();
}
