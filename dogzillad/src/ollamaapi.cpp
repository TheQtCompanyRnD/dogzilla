#include "ollamaapi.h"
#include "ollamaresponse.h"

#include <QEventLoop>
#include <QJsonArray>
#include <QJsonObject>
#include <QNetworkReply>
#include <QRestReply>

static inline QNetworkAccessManager *getUnderlyingNetworkManager()
{
    static auto gUnderlyingManager = std::make_unique<QNetworkAccessManager>();
    return gUnderlyingManager.get();
}

OllamaApi::OllamaApi(QObject *parent)
    : QRestAccessManager{getUnderlyingNetworkManager(), parent}
{}

void OllamaApi::setApiUrl(const QUrl &url)
{
    if (m_apiUrl == url)
        return;
    m_apiUrl = url;
    emit apiUrlChanged();
}

void OllamaApi::setGenerating(bool generating)
{
    if (m_generating == generating)
        return;
    m_generating = generating;
    emit generatingChanged();
}

QStringList OllamaApi::list()
{
    QUrl url = getApiUrl();
    if (!url.isValid())
        return QStringList{};
    url.setPath("/api/tags");

    QNetworkRequest request{url};
    request.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
    std::unique_ptr<QNetworkReply> reply { get(request) };

    QEventLoop loop;
    connect(reply.get(), &QNetworkReply::finished, &loop, &QEventLoop::quit);
    loop.exec();

    if (reply->error() == QNetworkReply::NoError) {
        OllamaResponse ollamaResponse{reply->readAll()};
        if (ollamaResponse.hasError())
            return QStringList{};
        return ollamaResponse.getModelNames();
    } else {
        qDebug() << "Error:" << reply->errorString();
        return QStringList{};
    }

    return QStringList{};
}

void OllamaApi::startChat(const QString &modelName)
{
    if (m_model.getName() == modelName)
        return;
    m_model.setName(modelName);
}

void OllamaApi::chat(const QString &message)
{
    QUrl url = getApiUrl();
    if (!url.isValid())
        return;
    url.setPath("/api/chat");

    m_model.addMessage(message, true);
    const QByteArray data = m_model.toJson();

    QNetworkRequest request{url};
    request.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
    QNetworkReply *reply = post(request, data);

    m_model.addMessage("", false);
    setGenerating(true);

    auto onReply = [this, reply](qint64, qint64){
        QString response;
        OllamaResponse ollamaResponse;
        while (reply->bytesAvailable() > 0) {
            ollamaResponse.reset(reply->readLine());
            response += ollamaResponse.getChatContent();
        }
        m_model.updateMessage(response);
    };

    auto onReplyFinished = [this](){ setGenerating(false); };

    connect(this, &OllamaApi::stopGenerating, reply, &QNetworkReply::abort);
    connect(reply, &QNetworkReply::finished, reply, &QNetworkReply::deleteLater);
    connect(reply, &QNetworkReply::finished, this, onReplyFinished);
    connect(reply, &QNetworkReply::downloadProgress, this, onReply);
}
