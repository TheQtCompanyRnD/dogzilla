// Copyright (C) 2026 The Qt Company Ltd.
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR BSD-3-Clause
#ifndef VOLUMECONTROLLER_H
#define VOLUMECONTROLLER_H

#include <QtQmlIntegration/qqmlintegration.h>
#include <QObject>
#include <QTimer>

#include <string>

struct pa_context;
struct pa_threaded_mainloop;

// System-wide audio mixer via libpulse, talking to PipeWire's pipewire-pulse
// server: the "master" channel is the default sink (speaker), "mic" the
// default source (capture gain). dogzillad runs in pi's user session and
// shares its PipeWire instance, so no sudo is needed. QtMultimedia can't do
// this: QAudioOutput.volume and QTextToSpeech.volume are per-stream, not the
// device master.
//
// Unlike the earlier wpctl/QProcess implementation, libpulse gives us a
// subscription: volume changes made by other means (alsamixer, another app,
// a default-device switch) update the properties too, so bound state -- the
// audio.* node parameters -- stays live. Writes are clamped to [0, 1] to
// avoid over-amplification, and PulseAudio protocol commands execute in
// submission order, so rapid writes need no serialization workaround.
//
// Threading: libpulse callbacks run on its own mainloop thread and marshal
// results to the Qt thread; Qt-side calls into the context take the mainloop
// lock. Values are quantized to two decimals (as wpctl displays them) so the
// read-back echo after our own set is stable.
class VolumeController : public QObject
{
    Q_OBJECT
    QML_ELEMENT
    Q_PROPERTY(qreal master READ master WRITE setMaster NOTIFY masterChanged FINAL)
    Q_PROPERTY(qreal mic READ mic WRITE setMic NOTIFY micChanged FINAL)
public:
    explicit VolumeController(QObject *parent = nullptr);
    ~VolumeController() override;

    qreal master() const { return m_master.volume; }
    qreal mic() const { return m_mic.volume; }

public slots:
    void setMaster(qreal volume);
    void setMic(qreal volume);

signals:
    void masterChanged(qreal volume);
    void micChanged(qreal volume);

private:
    struct Channel {
        bool isSink = true;       // sink (master) or source (mic)
        std::string device;       // current default device name (Qt thread)
        quint8 paChannels = 2;    // channel count of the device's volume
        qreal volume = 0;
    };

    void connectContext();
    void teardownContext();
    void refresh();                                   // PA thread, lock held
    void applyChannel(Channel &c, qreal volume);      // Qt thread
    void channelInfo(Channel &c, const std::string &device,
                     quint8 paChannels, qreal volume); // Qt thread
    void notifyChanged(const Channel &c);

    static void contextStateCallback(pa_context *ctx, void *userdata);

    Channel m_master;
    Channel m_mic;
    pa_threaded_mainloop *m_loop = nullptr;
    pa_context *m_ctx = nullptr;
    QTimer m_reconnectTimer;

    friend struct VolumeControllerPaCallbacks;
};

#endif // VOLUMECONTROLLER_H
