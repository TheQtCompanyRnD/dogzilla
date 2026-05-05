#ifndef LIDARINSTANCETABLE_H
#define LIDARINSTANCETABLE_H

#include <qqmlintegration.h>
#include <QtQuick3D/qquick3dinstancing.h>

class LidarInstanceTable : public QQuick3DInstancing
{
    Q_OBJECT
    QML_ELEMENT

    Q_PROPERTY(qreal angleIncrement READ angleIncrement WRITE setAngleIncrement NOTIFY angleIncrementChanged FINAL)
    Q_PROPERTY(QList<float> ranges READ ranges WRITE setRanges NOTIFY rangesChanged FINAL)
    Q_PROPERTY(QList<float> intensities READ intensities WRITE setIntensities NOTIFY intensitiesChanged FINAL)

public:
    LidarInstanceTable(QQuick3DObject *parent = nullptr);

    qreal angleIncrement() const { return m_angleIncrement; }
    void setAngleIncrement(qreal newAngleIncrement);

    QList<float> ranges() const { return m_ranges; }
    void setRanges(const QList<float> &newRanges);

    QList<float> intensities() const { return m_intensities; }
    void setIntensities(const QList<float> &newIntensities);

signals:
    void angleIncrementChanged();
    void rangesChanged();
    void intensitiesChanged();

protected:
    QByteArray getInstanceBuffer(int *instanceCount) override;

private:
    QByteArray m_instanceData;
    int m_instanceCount = 0;
    bool m_dirty = false;

    // "spacing" of samples in radians
    qreal m_angleIncrement;
    // parallel arrays: each LiDAR sample has a range and an intensity; if NaN, it's not valid
    QList<float> m_ranges;
    QList<float> m_intensities;
};

#endif // LIDARINSTANCETABLE_H
