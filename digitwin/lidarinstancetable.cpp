#include "lidarinstancetable.h"

// Mikhail Matrosov sextic fit to matplotlib's viridis colormap.
// https://www.shadertoy.com/view/WlfXRN
QColor viridis(float t) {
    t = std::clamp(t, 0.0f, 1.0f);
    const QVector3D c0( 0.2777273272234177f,   0.005407344544966578f,  0.3340998053353061f);
    const QVector3D c1( 0.1050930431085774f,   1.404613529898575f,     1.384590162594685f);
    const QVector3D c2(-0.3308618287255563f,   0.214847559468213f,     0.09509516302823659f);
    const QVector3D c3(-4.634230498983486f,   -5.799100973351585f,   -19.33244095627987f);
    const QVector3D c4( 6.228269936347081f,   14.17993336680509f,     56.69055260068105f);
    const QVector3D c5( 4.776384997670288f,  -13.74514537774601f,    -65.35303263337234f);
    const QVector3D c6(-5.435455855934631f,    4.645852612197189f,    26.3124352495832f);
    const QVector3D rgb = c0 + t*(c1 + t*(c2 + t*(c3 + t*(c4 + t*(c5 + t*c6)))));
    return QColor::fromRgbF(std::clamp(rgb.x(), 0.0f, 1.0f),
                            std::clamp(rgb.y(), 0.0f, 1.0f),
                            std::clamp(rgb.z(), 0.0f, 1.0f));
}

LidarInstanceTable::LidarInstanceTable(QQuick3DObject *parent) : QQuick3DInstancing(parent)
{
}

void LidarInstanceTable::setAngleIncrement(qreal newAngleIncrement)
{
    if (qFuzzyCompare(m_angleIncrement, newAngleIncrement))
        return;
    m_angleIncrement = newAngleIncrement;
    m_dirty = true;
    markDirty();
    emit angleIncrementChanged();
}

void LidarInstanceTable::setRanges(const QList<float> &newRanges)
{
    if (m_ranges == newRanges)
        return;
    m_ranges = newRanges;
    m_dirty = true;
    markDirty();
    emit rangesChanged();
}

void LidarInstanceTable::setIntensities(const QList<float> &newIntensities)
{
    if (m_intensities == newIntensities)
        return;
    m_intensities = newIntensities;
    m_dirty = true;
    markDirty();
    emit intensitiesChanged();
}

QByteArray LidarInstanceTable::getInstanceBuffer(int *instanceCount)
{
    if (m_dirty) {
        m_instanceCount = 0;
        m_instanceData.resize(0);
        for (int i = 0; i < m_ranges.size() && i < m_intensities.size(); ++i) {
            const auto range = m_ranges.at(i) * 1000;
            const auto intensity = m_intensities.at(i);
            if (!qIsNaN(range) && !qIsNaN(intensity) && intensity > 0.0f) {
                const qreal angle = m_angleIncrement * i;
                const QColor color = viridis(intensity / 240.0f);
                auto entry = calculateTableEntry({ float(range * qSin(angle)), 0, float(range * qCos(angle)) },
                                                 { 1.0, 1.0, 1.0 }, // scale
                                                 { 0.0, float(angle * 180 / M_PI), 0.0 }, // eulerRotation
                                                 color, {});
                m_instanceData.append(reinterpret_cast<const char *>(&entry), sizeof(entry));
                ++m_instanceCount;
            }
        }
        m_dirty = false;
    }
    if (m_instanceCount)
        *instanceCount = m_instanceCount;

    return m_instanceData;
}

#include "lidarinstancetable.moc"
