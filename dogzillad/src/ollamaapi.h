#ifndef OLLAMAAPI_H
#define OLLAMAAPI_H

#include "chatmodel.h"

#include <QObject>
#include <QQmlEngine>
#include <QRestAccessManager>

class OllamaApi : public QRestAccessManager
{
    Q_OBJECT
    QML_ELEMENT

    Q_PROPERTY(QUrl apiUrl READ getApiUrl WRITE setApiUrl NOTIFY apiUrlChanged FINAL)
    Q_PROPERTY(ChatModel* model READ model NOTIFY modelChanged FINAL)
    Q_PROPERTY(bool generating READ isGenerating NOTIFY generatingChanged FINAL)

public:
    explicit OllamaApi(QObject *parent = nullptr);

    const QUrl &getApiUrl() const { return m_apiUrl; }
    void setApiUrl(const QUrl &url);

    ChatModel *model() { return &m_model; }

    bool isGenerating() const { return m_generating; }
    void setGenerating(bool generating);

    Q_INVOKABLE QStringList list();
    Q_INVOKABLE void startChat(const QString &modelName);
    Q_INVOKABLE void chat(const QString &message);

signals:
    void apiUrlChanged();
    void modelChanged();
    void generatingChanged();
    void stopGenerating();

private:
    QUrl m_apiUrl;
    ChatModel m_model;
    bool m_generating = false;
};

#endif // OLLAMAAPI_H
