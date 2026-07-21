// Copyright (C) 2026 The Qt Company Ltd.
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR BSD-3-Clause
#include "volumecontroller.h"

#include <QCoreApplication>
#include <QLoggingCategory>
#include <QPointer>

#include <pulse/pulseaudio.h>

Q_LOGGING_CATEGORY(lcVolume, "dogzilla.volume")

// Quantize to two decimals (what wpctl displays) so the subscription
// read-back after our own set compares equal to what we stored.
static qreal quantize(qreal v)
{
    return qRound(v * 100.0) / 100.0;
}

// All pa_* callbacks run on the libpulse mainloop thread with the mainloop
// lock held. They must only copy data and marshal to the Qt thread.
struct VolumeControllerPaCallbacks
{
    static void serverInfo(pa_context *ctx, const pa_server_info *info, void *userdata)
    {
        auto *self = static_cast<VolumeController *>(userdata);
        if (!info)
            return;
        if (info->default_sink_name) {
            pa_operation *op = pa_context_get_sink_info_by_name(
                ctx, info->default_sink_name, sinkInfo, userdata);
            if (op)
                pa_operation_unref(op);
        }
        if (info->default_source_name) {
            pa_operation *op = pa_context_get_source_info_by_name(
                ctx, info->default_source_name, sourceInfo, userdata);
            if (op)
                pa_operation_unref(op);
        }
        Q_UNUSED(self);
    }

    static void sinkInfo(pa_context *, const pa_sink_info *info, int eol, void *userdata)
    {
        if (eol || !info)
            return;
        forwardInfo(static_cast<VolumeController *>(userdata), /*isSink=*/true,
                    info->name, info->volume);
    }

    static void sourceInfo(pa_context *, const pa_source_info *info, int eol, void *userdata)
    {
        if (eol || !info)
            return;
        forwardInfo(static_cast<VolumeController *>(userdata), /*isSink=*/false,
                    info->name, info->volume);
    }

    static void forwardInfo(VolumeController *self, bool isSink,
                            const char *name, const pa_cvolume &cv)
    {
        const std::string device = name ? name : "";
        const quint8 channels = cv.channels;
        const qreal volume = quantize(qreal(pa_cvolume_avg(&cv)) / PA_VOLUME_NORM);
        QPointer<VolumeController> weakThis(self);
        QMetaObject::invokeMethod(
            qApp,
            [weakThis, isSink, device, channels, volume] {
                if (weakThis)
                    weakThis->channelInfo(isSink ? weakThis->m_master : weakThis->m_mic,
                                          device, channels, volume);
            },
            Qt::QueuedConnection);
    }
};

VolumeController::VolumeController(QObject *parent) : QObject(parent)
{
    m_master.isSink = true;
    m_mic.isSink = false;

    m_reconnectTimer.setInterval(3000);
    m_reconnectTimer.setSingleShot(true);
    connect(&m_reconnectTimer, &QTimer::timeout, this, &VolumeController::connectContext);

    connectContext();
}

VolumeController::~VolumeController()
{
    teardownContext();
}

void VolumeController::connectContext()
{
    teardownContext();

    m_loop = pa_threaded_mainloop_new();
    if (!m_loop || pa_threaded_mainloop_start(m_loop) < 0) {
        qCWarning(lcVolume) << "failed to start libpulse mainloop";
        m_reconnectTimer.start();
        return;
    }

    pa_threaded_mainloop_lock(m_loop);
    m_ctx = pa_context_new(pa_threaded_mainloop_get_api(m_loop), "dogzillad");
    pa_context_set_state_callback(m_ctx, &VolumeController::contextStateCallback, this);
    if (pa_context_connect(m_ctx, nullptr, PA_CONTEXT_NOFLAGS, nullptr) < 0) {
        qCWarning(lcVolume) << "pa_context_connect failed:"
                            << pa_strerror(pa_context_errno(m_ctx));
        pa_threaded_mainloop_unlock(m_loop);
        m_reconnectTimer.start();
        return;
    }
    pa_threaded_mainloop_unlock(m_loop);
}

void VolumeController::teardownContext()
{
    if (!m_loop)
        return;
    pa_threaded_mainloop_lock(m_loop);
    if (m_ctx) {
        pa_context_set_state_callback(m_ctx, nullptr, nullptr);
        pa_context_set_subscribe_callback(m_ctx, nullptr, nullptr);
        pa_context_disconnect(m_ctx);
        pa_context_unref(m_ctx);
        m_ctx = nullptr;
    }
    pa_threaded_mainloop_unlock(m_loop);
    pa_threaded_mainloop_stop(m_loop);
    pa_threaded_mainloop_free(m_loop);
    m_loop = nullptr;
}

// PA mainloop thread.
void VolumeController::contextStateCallback(pa_context *ctx, void *userdata)
{
    auto *self = static_cast<VolumeController *>(userdata);
    switch (pa_context_get_state(ctx)) {
    case PA_CONTEXT_READY: {
        // Track external changes: any sink/source/server event triggers a
        // re-read of the default devices (cheap, idempotent, and covers
        // default-device switches too).
        pa_context_set_subscribe_callback(
            ctx,
            [](pa_context *, pa_subscription_event_type_t, uint32_t, void *ud) {
                static_cast<VolumeController *>(ud)->refresh();
            },
            userdata);
        pa_operation *op = pa_context_subscribe(
            ctx,
            static_cast<pa_subscription_mask_t>(PA_SUBSCRIPTION_MASK_SINK
                                                | PA_SUBSCRIPTION_MASK_SOURCE
                                                | PA_SUBSCRIPTION_MASK_SERVER),
            nullptr, nullptr);
        if (op)
            pa_operation_unref(op);
        self->refresh();
        break;
    }
    case PA_CONTEXT_FAILED:
    case PA_CONTEXT_TERMINATED: {
        QPointer<VolumeController> weakThis(self);
        QMetaObject::invokeMethod(
            qApp,
            [weakThis] {
                if (!weakThis)
                    return;
                qCWarning(lcVolume) << "connection to the audio server lost; reconnecting";
                weakThis->m_reconnectTimer.start();
            },
            Qt::QueuedConnection);
        break;
    }
    default:
        break;
    }
}


// PA mainloop thread (or READY callback), lock held: query the default
// devices; the info callbacks marshal results to the Qt thread.
void VolumeController::refresh()
{
    if (!m_ctx)
        return;
    pa_operation *op = pa_context_get_server_info(
        m_ctx, &VolumeControllerPaCallbacks::serverInfo, this);
    if (op)
        pa_operation_unref(op);
}

// Qt thread: a device info arrived (startup, external change, or the echo
// of our own set).
void VolumeController::channelInfo(Channel &c, const std::string &device,
                                   quint8 paChannels, qreal volume)
{
    c.device = device;
    c.paChannels = paChannels ? paChannels : 2;
    if (qFuzzyCompare(1.0 + c.volume, 1.0 + volume))
        return;
    c.volume = volume;
    qCDebug(lcVolume) << (c.isSink ? "master" : "mic") << "is now" << volume
                      << "(" << QString::fromStdString(device) << ")";
    notifyChanged(c);
}

void VolumeController::notifyChanged(const Channel &c)
{
    if (&c == &m_master)
        emit masterChanged(c.volume);
    else
        emit micChanged(c.volume);
}

// Qt thread.
void VolumeController::applyChannel(Channel &c, qreal volume)
{
    const qreal v = quantize(qBound<qreal>(0.0, volume, 1.0));
    if (qFuzzyCompare(1.0 + c.volume, 1.0 + v))
        return;
    c.volume = v;
    notifyChanged(c);   // echo-on-set: the property answers immediately

    if (!m_loop || !m_ctx)
        return;
    pa_threaded_mainloop_lock(m_loop);
    if (pa_context_get_state(m_ctx) == PA_CONTEXT_READY && !c.device.empty()) {
        pa_cvolume cv;
        pa_cvolume_set(&cv, c.paChannels,
                       static_cast<pa_volume_t>(v * PA_VOLUME_NORM + 0.5));
        pa_operation *op = c.isSink
            ? pa_context_set_sink_volume_by_name(m_ctx, c.device.c_str(), &cv, nullptr, nullptr)
            : pa_context_set_source_volume_by_name(m_ctx, c.device.c_str(), &cv, nullptr, nullptr);
        if (op)
            pa_operation_unref(op);
        qCDebug(lcVolume) << (c.isSink ? "master" : "mic") << "->" << v;
    } else {
        qCWarning(lcVolume) << "audio server not ready; volume not applied";
    }
    pa_threaded_mainloop_unlock(m_loop);
}

void VolumeController::setMaster(qreal volume)
{
    applyChannel(m_master, volume);
}

void VolumeController::setMic(qreal volume)
{
    applyChannel(m_mic, volume);
}
