// Copyright (C) 2026 The Qt Company Ltd.
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR BSD-3-Clause
#include "occupancyinstancetable.h"

OccupancyInstanceTable::OccupancyInstanceTable(QQuick3DObject *parent) : QQuick3DInstancing(parent)
{
}


/*!
    \qmlproperty size OccupancyInstanceTable::size

    This property holds the size of the map in \l resolution units.

    The robot's world model can be thought of as a planar grid in which each
    square holds the probability of a solid obstacle. The size is simply the
    number of squares along the width and length of the known area so far.
    It may grow as the robot explores more areas and the SLAM algorithm
    expands the map.
*/
void OccupancyInstanceTable::setSize(const QSize &newSize)
{
    if (m_size == newSize)
        return;
    m_size = newSize;
    m_dirty = true;
    markDirty();
    emit sizeChanged();
}

/*!
    \qmlproperty real OccupancyInstanceTable::resolution

    This property holds the resolution of the map in meters per grid square.
    The default is 0.01 (1 cm).
*/
void OccupancyInstanceTable::setResolution(qreal newResolution)
{
    if (qFuzzyCompare(m_resolution, float(newResolution)))
        return;
    m_resolution = newResolution;
    emit resolutionChanged();
}

/*!
    \qmlproperty real OccupancyInstanceTable::resolution

    This property holds the array of probabilities in the map's grid squares,
    as signed bytes in row-major order.

    \value -1   The unknown probability (the laser never got there)
    \value 0    Known to be empty space: obstacle never detected
    \value 100  Known to be a solid continuously-detected obstacle

    Other values between 0 and 100 indicate fuzzy probabilities.
*/
void OccupancyInstanceTable::setProbabilities(const QByteArray &newProbabilities)
{
    if (m_probabilities == newProbabilities)
        return;
    m_probabilities = newProbabilities;
    m_dirty = true;
    markDirty();
    emit probabilitiesChanged();
}

/*!
    Create the instances for each square in the grid in which
    \c {probability >= 50}.
 */
QByteArray OccupancyInstanceTable::getInstanceBuffer(int *instanceCount)
{
    if (m_dirty) {
        m_instanceCount = 0;
        m_instanceData.resize(0);
        for (int i = 0; i < m_probabilities.size(); ++i) {
            const signed char probability = m_probabilities.at(i);
            if (probability < 50)
                continue;
            const QPoint pos(i % m_size.width(), i / m_size.width());
            const int gray = 255 - 2 * probability; // 100% solid: white; 0: dark gray
            auto entry = calculateTableEntry({ float(pos.y() * m_resolution), 0, float(pos.x() * m_resolution) },
                                             { m_resolution, float(gray) / 200.0f * m_resolution, m_resolution }, // scale
                                             { 0.0, 0.0, 0.0 }, // eulerRotation
                                             QColor(gray, gray, gray), {});
            m_instanceData.append(reinterpret_cast<const char *>(&entry), sizeof(entry));
            ++m_instanceCount;
        }
        m_dirty = false;
    }
    if (m_instanceCount)
        *instanceCount = m_instanceCount;

    return m_instanceData;
}
