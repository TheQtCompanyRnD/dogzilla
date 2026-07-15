// Copyright (C) 2026 The Qt Company Ltd.
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR BSD-3-Clause
#ifndef FANCONTROLLER_H
#define FANCONTROLLER_H

#include <QtQmlIntegration/qqmlintegration.h>
#include <QObject>

// Silences the Pi 5 chassis fan (it sits right next to the USB mic) during
// push-to-talk capture, by invoking the root helper `dogzilla-fan` over sudo
// (shipped + sudoers-scoped by the dogzilla-fan recipe in meta-dogzilla).
// setQuiet(true) suspends the cpu-thermal governor and forces the fan off;
// setQuiet(false) restores automatic control. The helper self-heals after a
// timeout if 'auto' is never sent.
class FanController : public QObject
{
    Q_OBJECT
    QML_ELEMENT
    Q_PROPERTY(bool quiet READ quiet WRITE setQuiet NOTIFY quietChanged FINAL)
public:
    explicit FanController(QObject *parent = nullptr);

    bool quiet() const { return m_quiet; }

public slots:
    void setQuiet(bool quiet);

signals:
    void quietChanged(bool quiet);

private:
    bool m_quiet = false;
};

#endif // FANCONTROLLER_H
