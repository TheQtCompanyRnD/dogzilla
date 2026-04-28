#ifndef RECEIVEDIMAGEPROVIDER_H
#define RECEIVEDIMAGEPROVIDER_H

#include <QImage>
#include <QObject>
#include <QQmlEngine>
#include <QQuickImageProvider>

class ReceivedImageProvider : public QQuickImageProvider
{
    Q_OBJECT
    QML_SINGLETON
    QML_NAMED_ELEMENT(ReceivedImageProvider)
    Q_PROPERTY(int currentFrame READ currentFrame NOTIFY currentFrameChanged FINAL)
public:
    static ReceivedImageProvider *instance();
    static ReceivedImageProvider *create(QQmlEngine *engine, QJSEngine *)
    {
        auto *p = instance();
        QQmlEngine::setObjectOwnership(p, QQmlEngine::CppOwnership);
        return p;
    }
    QImage requestImage(const QString &id, QSize *size, const QSize &requestedSize) override;
    Q_INVOKABLE void setImage(const QImage &im, int frame, int sec, int nsec);

    int currentFrame() const { return m_currentFrame; }

signals:
    void updated(int frame, qreal sec);
    void currentFrameChanged();

private:
    ReceivedImageProvider();

    QImage m_currentImage;
    int m_currentFrame;
    int m_sec;
    int m_nsec;
};

#endif // RECEIVEDIMAGEPROVIDER_H
