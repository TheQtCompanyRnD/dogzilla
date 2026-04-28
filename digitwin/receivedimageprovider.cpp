#include "receivedimageprovider.h"
#include <QLoggingCategory>
#include <QPainter>

Q_STATIC_LOGGING_CATEGORY(lcImprov, "dogzilla.imageprovider")

ReceivedImageProvider *ReceivedImageProvider::instance()
{
    static ReceivedImageProvider *self = nullptr;
    if (!self)
        self = new ReceivedImageProvider();
    return self;
}

ReceivedImageProvider::ReceivedImageProvider()
  : QQuickImageProvider(QQuickImageProvider::Image)
  , m_currentImage(640, 480, QImage::Format_RGB32)
{
    QPainter p(&m_currentImage);
    p.fillRect(m_currentImage.rect(), Qt::gray);
    p.drawText(m_currentImage.rect(), Qt::AlignCenter, "awaiting image");
}

QImage ReceivedImageProvider::requestImage(const QString &id, QSize *size, const QSize &requestedSize)
{
    Q_ASSERT(size);
    *size = m_currentImage.size();
    qCDebug(lcImprov) << id << requestedSize << *size;
    return m_currentImage;
}

void ReceivedImageProvider::setImage(const QImage &im, int frame, int sec, int nsec)
{
    qCDebug(lcImprov) << im << frame << sec << nsec;
    m_currentImage = im;
    m_currentFrame = frame;
    m_sec = sec;
    m_nsec = nsec;
    emit updated(frame, sec + nsec / 1000000000.0);
    emit currentFrameChanged();
}
