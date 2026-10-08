#ifndef OCCUPANCYINSTANCETABLE_H
#define OCCUPANCYINSTANCETABLE_H

#include <qqmlintegration.h>
#include <QtQuick3D/qquick3dinstancing.h>

class OccupancyInstanceTable : public QQuick3DInstancing
{
    Q_OBJECT
    QML_ELEMENT

    Q_PROPERTY(QSize size READ size WRITE setSize NOTIFY sizeChanged FINAL)
    Q_PROPERTY(qreal resolution READ resolution WRITE setResolution NOTIFY resolutionChanged FINAL)
    Q_PROPERTY(QByteArray probabilities READ probabilities WRITE setProbabilities NOTIFY probabilitiesChanged FINAL)

public:
    OccupancyInstanceTable(QQuick3DObject *parent = nullptr);

    QSize size() const { return m_size; }
    void setSize(const QSize &newSize);

    qreal resolution() const { return m_resolution; }
    void setResolution(qreal newResolution);

    QByteArray probabilities() const { return m_probabilities; }
    void setProbabilities(const QByteArray &newProbabilities);

signals:
    void sizeChanged();
    void resolutionChanged();
    void probabilitiesChanged();

protected:
    QByteArray getInstanceBuffer(int *instanceCount) override;

private:
    QByteArray m_instanceData;
    int m_instanceCount = 0;
    bool m_dirty = false;

    QSize m_size;
    float m_resolution = 0.01f;
    QByteArray m_probabilities;
};

#endif // OCCUPANCYINSTANCETABLE_H
