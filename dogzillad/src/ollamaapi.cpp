#include "ollamaapi.h"
#include "ollamaresponse.h"

#include <QEventLoop>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkReply>

#include <memory>

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

void OllamaApi::setModel(const QString &name)
{
    if (m_modelName == name)
        return;
    m_modelName = name;
    emit modelChanged();
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

void OllamaApi::chat(const QString &message)
{
    QUrl url = getApiUrl();
    if (!url.isValid())
        return;
    url.setPath("/api/chat");

    // Single-turn request: just this utterance, no prior history.
    QJsonObject userMessage;
    userMessage.insert("role", "user");
    userMessage.insert("content", message);
    QJsonObject body;
    body.insert("model", m_modelName);
    body.insert("messages", QJsonArray{ userMessage });

    QNetworkRequest request{url};
    request.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
    QNetworkReply *reply = post(request, QJsonDocument{body}.toJson());

    setGenerating(true);

    // /api/chat streams NDJSON: one JSON object per line, each with an
    // incremental message.content. Accumulate across downloadProgress events
    // (readLine drains only the newly-arrived bytes) into a shared buffer so the
    // finished handler can emit the full reply.
    auto accumulated = std::make_shared<QString>();

    auto onReply = [this, reply, accumulated](qint64, qint64){
        OllamaResponse ollamaResponse;
        while (reply->bytesAvailable() > 0) {
            ollamaResponse.reset(reply->readLine());
            if (ollamaResponse.hasError())
                continue; // partial line: wait for the rest on the next event
            *accumulated += ollamaResponse.getChatContent();
        }
        emit responseChanged(*accumulated);
    };

    auto onReplyFinished = [this, accumulated](){
        setGenerating(false);
        emit responseReceived(*accumulated);
    };

    connect(this, &OllamaApi::stopGenerating, reply, &QNetworkReply::abort);
    connect(reply, &QNetworkReply::finished, reply, &QNetworkReply::deleteLater);
    connect(reply, &QNetworkReply::finished, this, onReplyFinished);
    connect(reply, &QNetworkReply::downloadProgress, this, onReply);
}
