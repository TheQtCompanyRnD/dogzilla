// Copyright (C) 2026 The Qt Company Ltd.
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR BSD-3-Clause
#ifndef OCCUPANCYWALLGEOMETRY_H
#define OCCUPANCYWALLGEOMETRY_H

#include <qqmlintegration.h>
#include <QtCore/qsize.h>
#include <QtQuick3D/qquick3dgeometry.h>

class OccupancyWallGeometry : public QQuick3DGeometry
{
    Q_OBJECT
    QML_ELEMENT

    Q_PROPERTY(QSize size READ size WRITE setSize NOTIFY sizeChanged FINAL)
    Q_PROPERTY(qreal resolution READ resolution WRITE setResolution NOTIFY resolutionChanged FINAL)
    Q_PROPERTY(QByteArray probabilities READ probabilities WRITE setProbabilities NOTIFY probabilitiesChanged FINAL)
    Q_PROPERTY(int threshold READ threshold WRITE setThreshold NOTIFY thresholdChanged FINAL)
    Q_PROPERTY(qreal wallHeight READ wallHeight WRITE setWallHeight NOTIFY wallHeightChanged FINAL)
    Q_PROPERTY(qreal tolerance READ tolerance WRITE setTolerance NOTIFY toleranceChanged FINAL)
    Q_PROPERTY(qreal creaseAngle READ creaseAngle WRITE setCreaseAngle NOTIFY creaseAngleChanged FINAL)
    Q_PROPERTY(int contourCount READ contourCount NOTIFY geometryRebuilt FINAL)
    Q_PROPERTY(int vertexCount READ vertexCount NOTIFY geometryRebuilt FINAL)

public:
    OccupancyWallGeometry(QQuick3DObject *parent = nullptr);

    QSize size() const { return m_size; }
    void setSize(const QSize &newSize);

    qreal resolution() const { return m_resolution; }
    void setResolution(qreal newResolution);

    QByteArray probabilities() const { return m_probabilities; }
    void setProbabilities(const QByteArray &newProbabilities);

    int threshold() const { return m_threshold; }
    void setThreshold(int newThreshold);

    qreal wallHeight() const { return m_wallHeight; }
    void setWallHeight(qreal newWallHeight);

    qreal tolerance() const { return m_tolerance; }
    void setTolerance(qreal newTolerance);

    qreal creaseAngle() const { return m_creaseAngle; }
    void setCreaseAngle(qreal newCreaseAngle);

    int contourCount() const { return m_contourCount; }
    int vertexCount() const { return m_vertexCount; }

signals:
    void sizeChanged();
    void resolutionChanged();
    void probabilitiesChanged();
    void thresholdChanged();
    void wallHeightChanged();
    void toleranceChanged();
    void creaseAngleChanged();
    void geometryRebuilt();

private:
    void scheduleRebuild();
    void rebuild();

    QSize m_size;
    float m_resolution = 0.05f;
    QByteArray m_probabilities;
    int m_threshold = 50;
    float m_wallHeight = 0.25f;
    float m_tolerance = 0.025f;
    float m_creaseAngle = 35;
    int m_contourCount = 0;
    int m_vertexCount = 0;
    bool m_rebuildPending = false;
};

#endif // OCCUPANCYWALLGEOMETRY_H
