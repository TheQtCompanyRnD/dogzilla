// Copyright (C) 2026 The Qt Company Ltd.
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR BSD-3-Clause
#ifndef VOLUMECONTROLLER_H
#define VOLUMECONTROLLER_H

#include <QtQmlIntegration/qqmlintegration.h>
#include <QObject>
#include <QString>

// System-wide audio mixer via wpctl (WirePlumber/PipeWire): the "master"
// channel is the default sink (speaker), "mic" the default source (capture
// gain). dogzillad runs in pi's user session and shares its PipeWire
// instance, so no sudo is needed (unlike FanController). QtMultimedia can't
// do this: QAudioOutput.volume and QTextToSpeech.volume are per-stream, not
// the sink master.
//
// Each channel is read once at startup (async `wpctl get-volume`) and echoed
// on every set; changes made by other means (alsamixer, another app) are NOT
// tracked -- that would need libpulse's subscription API. Writes are clamped
// to [0, 1] to avoid over-amplification.
//
// Per channel, sets are serialized latest-wins: at most one
// `wpctl set-volume` runs at a time, and rapid successive writes (a dragged
// slider) coalesce so the final value is guaranteed to be applied last.
// Detached fire-and-forget would let near-simultaneous wpctl invocations
// finish out of order, leaving the sink on a stale value.
class VolumeController : public QObject
{
    Q_OBJECT
    QML_ELEMENT
    Q_PROPERTY(qreal master READ master WRITE setMaster NOTIFY masterChanged FINAL)
    Q_PROPERTY(qreal mic READ mic WRITE setMic NOTIFY micChanged FINAL)
public:
    explicit VolumeController(QObject *parent = nullptr);

    qreal master() const { return m_master.volume; }
    qreal mic() const { return m_mic.volume; }

    // Channel-addressed set for the SetVolume service. Returns the volume
    // actually applied (after clamping), or NaN for an unknown channel.
    Q_INVOKABLE qreal setChannelVolume(const QString &channel, qreal value);

public slots:
    void setMaster(qreal volume);
    void setMic(qreal volume);

signals:
    void masterChanged(qreal volume);
    void micChanged(qreal volume);

private:
    struct Channel {
        QString name;    // ROS-facing channel name (matches a Mixer.msg field)
        QString target;  // wpctl object, e.g. "@DEFAULT_AUDIO_SINK@"
        qreal volume = 0;
        bool setRunning = false;
        bool setPending = false;
    };

    Channel *channelByName(const QString &name);
    void readChannel(Channel &c);
    void setChannel(Channel &c, qreal value);
    void applyChannel(Channel &c);
    void notifyChanged(const Channel &c);

    Channel m_master;
    Channel m_mic;
};

#endif // VOLUMECONTROLLER_H
