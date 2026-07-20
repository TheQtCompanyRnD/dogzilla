// Copyright (C) 2026 The Qt Company Ltd.
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR BSD-3-Clause
#include "volumecontroller.h"

#include <QProcess>
#include <QLoggingCategory>

Q_LOGGING_CATEGORY(lcVolume, "dogzilla.volume")

static const QString wpctl = QStringLiteral("wpctl");
static const QString defaultSink = QStringLiteral("@DEFAULT_AUDIO_SINK@");

VolumeController::VolumeController(QObject *parent) : QObject(parent)
{
    readVolume();
}

// Async startup read: `wpctl get-volume @DEFAULT_AUDIO_SINK@` prints
// "Volume: 0.40" (with a " [MUTED]" suffix when muted). The property starts
// at 0 and snaps to the real value when the process finishes; the QML side
// only observes a change notification, same as any later set.
void VolumeController::readVolume()
{
    auto *p = new QProcess(this);
    connect(p, &QProcess::finished, this,
            [this, p](int exitCode, QProcess::ExitStatus status) {
        p->deleteLater();
        const QString out = QString::fromUtf8(p->readAllStandardOutput()).trimmed();
        if (status != QProcess::NormalExit || exitCode != 0) {
            qCWarning(lcVolume) << "wpctl get-volume failed:" << out
                                << p->readAllStandardError();
            return;
        }
        // "Volume: 0.40" or "Volume: 0.40 [MUTED]"
        const QStringList parts = out.split(QLatin1Char(' '), Qt::SkipEmptyParts);
        bool ok = false;
        const qreal v = parts.size() >= 2 ? parts.at(1).toDouble(&ok) : 0.0;
        if (!ok) {
            qCWarning(lcVolume) << "unexpected wpctl get-volume output:" << out;
            return;
        }
        qCDebug(lcVolume) << "startup volume" << v;
        if (qFuzzyCompare(1.0 + m_volume, 1.0 + v))
            return;
        m_volume = v;
        emit volumeChanged(m_volume);
    });
    p->start(wpctl, {QStringLiteral("get-volume"), defaultSink});
}

void VolumeController::setVolume(qreal volume)
{
    const qreal v = qBound<qreal>(0.0, volume, 1.0);
    if (qFuzzyCompare(1.0 + m_volume, 1.0 + v))
        return;
    m_volume = v;
    emit volumeChanged(m_volume);

    // Don't apply immediately: serialize (see applyVolume()).
    m_setPending = true;
    applyVolume();
}

// Run at most one `wpctl set-volume` at a time, always applying the latest
// stored value; further writes while one is in flight coalesce into a single
// trailing run. Guarantees the sink ends up on the last-set value.
void VolumeController::applyVolume()
{
    if (m_setRunning || !m_setPending)
        return;
    m_setPending = false;
    m_setRunning = true;

    const QString arg = QString::number(m_volume, 'f', 2);
    qCDebug(lcVolume) << "volume ->" << arg << "(wpctl set-volume" << defaultSink << arg << ")";
    auto *p = new QProcess(this);
    connect(p, &QProcess::finished, this,
            [this, p](int exitCode, QProcess::ExitStatus status) {
        p->deleteLater();
        if (status != QProcess::NormalExit || exitCode != 0) {
            qCWarning(lcVolume) << "wpctl set-volume failed:"
                                << p->readAllStandardError();
        }
        m_setRunning = false;
        applyVolume();   // drain a value set while we were running
    });
    p->start(wpctl, {QStringLiteral("set-volume"), defaultSink, arg});
}
