// Copyright (C) 2026 The Qt Company Ltd.
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR BSD-3-Clause
#ifndef VOLUMECONTROLLER_H
#define VOLUMECONTROLLER_H

#include <QtQmlIntegration/qqmlintegration.h>
#include <QObject>

// System-wide master volume via wpctl (WirePlumber/PipeWire default sink).
// dogzillad runs in pi's user session and shares its PipeWire instance, so no
// sudo is needed (unlike FanController). QtMultimedia can't do this:
// QAudioOutput.volume and QTextToSpeech.volume are per-stream, not the sink
// master.
//
// The volume property is read once at startup (async `wpctl get-volume`) and
// echoed on every set; changes made by other means (alsamixer, another app)
// are NOT tracked -- that would need libpulse's subscription API. Writes are
// clamped to [0, 1] to avoid over-amplification.
//
// Sets are serialized, latest-wins: at most one `wpctl set-volume` runs at a
// time, and rapid successive writes (a dragged slider) coalesce so the final
// value is guaranteed to be applied last. Detached fire-and-forget would let
// near-simultaneous wpctl invocations finish out of order, leaving the sink
// on a stale value.
class VolumeController : public QObject
{
    Q_OBJECT
    QML_ELEMENT
    Q_PROPERTY(qreal volume READ volume WRITE setVolume NOTIFY volumeChanged FINAL)
public:
    explicit VolumeController(QObject *parent = nullptr);

    qreal volume() const { return m_volume; }

public slots:
    void setVolume(qreal volume);

signals:
    void volumeChanged(qreal volume);

private:
    void readVolume();
    void applyVolume();

    qreal m_volume = 0;
    bool m_setRunning = false;
    bool m_setPending = false;
};

#endif // VOLUMECONTROLLER_H
