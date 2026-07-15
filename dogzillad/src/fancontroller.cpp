// Copyright (C) 2026 The Qt Company Ltd.
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR BSD-3-Clause
#include "fancontroller.h"

#include <QProcess>
#include <QLoggingCategory>

Q_LOGGING_CATEGORY(lcFan, "dogzilla.fan")

FanController::FanController(QObject *parent) : QObject(parent) {}

void FanController::setQuiet(bool quiet)
{
    if (m_quiet == quiet)
        return;
    m_quiet = quiet;

    // Fire-and-forget: the helper is fast, and 'quiet' schedules its own timed
    // restore so a missed 'auto' can't leave the fan off forever.
    const QString action = quiet ? QStringLiteral("quiet") : QStringLiteral("auto");
    qCDebug(lcFan) << "fan ->" << action << "(sudo dogzilla-fan" << action << ")";
    if (!QProcess::startDetached(QStringLiteral("sudo"),
                                 {QStringLiteral("dogzilla-fan"), action})) {
        qCWarning(lcFan) << "failed to launch: sudo dogzilla-fan" << action;
    }
    emit quietChanged(m_quiet);
}
