// Copyright (C) 2026 The Qt Company Ltd.
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR BSD-3-Clause
#include "volumecontroller.h"

#include <QProcess>
#include <QLoggingCategory>

Q_LOGGING_CATEGORY(lcVolume, "dogzilla.volume")

static const QString wpctl = QStringLiteral("wpctl");

VolumeController::VolumeController(QObject *parent) : QObject(parent)
{
    m_master.name = QStringLiteral("master");
    m_master.target = QStringLiteral("@DEFAULT_AUDIO_SINK@");
    m_mic.name = QStringLiteral("mic");
    m_mic.target = QStringLiteral("@DEFAULT_AUDIO_SOURCE@");
    readChannel(m_master);
    readChannel(m_mic);
}

void VolumeController::notifyChanged(const Channel &c)
{
    if (&c == &m_master)
        emit masterChanged(c.volume);
    else
        emit micChanged(c.volume);
}

// Async startup read: `wpctl get-volume <target>` prints "Volume: 0.40"
// (with a " [MUTED]" suffix when muted). The property starts at 0 and snaps
// to the real value when the process finishes; the QML side only observes a
// change notification, same as any later set.
void VolumeController::readChannel(Channel &c)
{
    auto *p = new QProcess(this);
    connect(p, &QProcess::finished, this,
            [this, p, &c](int exitCode, QProcess::ExitStatus status) {
        p->deleteLater();
        const QString out = QString::fromUtf8(p->readAllStandardOutput()).trimmed();
        if (status != QProcess::NormalExit || exitCode != 0) {
            qCWarning(lcVolume) << c.name << "wpctl get-volume failed:" << out
                                << p->readAllStandardError();
            return;
        }
        const QStringList parts = out.split(QLatin1Char(' '), Qt::SkipEmptyParts);
        bool ok = false;
        const qreal v = parts.size() >= 2 ? parts.at(1).toDouble(&ok) : 0.0;
        if (!ok) {
            qCWarning(lcVolume) << c.name << "unexpected wpctl get-volume output:" << out;
            return;
        }
        qCDebug(lcVolume) << c.name << "startup volume" << v;
        if (qFuzzyCompare(1.0 + c.volume, 1.0 + v))
            return;
        c.volume = v;
        notifyChanged(c);
    });
    p->start(wpctl, {QStringLiteral("get-volume"), c.target});
}

void VolumeController::setChannel(Channel &c, qreal value)
{
    const qreal v = qBound<qreal>(0.0, value, 1.0);
    if (qFuzzyCompare(1.0 + c.volume, 1.0 + v))
        return;
    c.volume = v;
    notifyChanged(c);

    // Don't apply immediately: serialize (see applyChannel()).
    c.setPending = true;
    applyChannel(c);
}

// Run at most one `wpctl set-volume` per channel at a time, always applying
// the latest stored value; further writes while one is in flight coalesce
// into a single trailing run. Guarantees the channel ends up on the
// last-set value.
void VolumeController::applyChannel(Channel &c)
{
    if (c.setRunning || !c.setPending)
        return;
    c.setPending = false;
    c.setRunning = true;

    const QString arg = QString::number(c.volume, 'f', 2);
    qCDebug(lcVolume) << c.name << "->" << arg << "(wpctl set-volume" << c.target << arg << ")";
    auto *p = new QProcess(this);
    connect(p, &QProcess::finished, this,
            [this, p, &c](int exitCode, QProcess::ExitStatus status) {
        p->deleteLater();
        if (status != QProcess::NormalExit || exitCode != 0) {
            qCWarning(lcVolume) << c.name << "wpctl set-volume failed:"
                                << p->readAllStandardError();
        }
        c.setRunning = false;
        applyChannel(c);   // drain a value set while we were running
    });
    p->start(wpctl, {QStringLiteral("set-volume"), c.target, arg});
}

void VolumeController::setMaster(qreal volume)
{
    setChannel(m_master, volume);
}

void VolumeController::setMic(qreal volume)
{
    setChannel(m_mic, volume);
}
