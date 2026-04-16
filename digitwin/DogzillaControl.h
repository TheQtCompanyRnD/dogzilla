
// Manual extension point. This file is created once if missing.
#ifndef DOGZILLA_CONTROL_H
#define DOGZILLA_CONTROL_H

#include "DogzillaControlBase.h"
#include <QQmlEngine>

class DogzillaControl : public DogzillaControlBase
{
    Q_OBJECT
    QML_ELEMENT

public:
    explicit DogzillaControl(QObject *parent = nullptr);

    Q_INVOKABLE void updateJointState(QStringList names, QList<qreal> values);
};

#endif // DOGZILLA_CONTROL_H