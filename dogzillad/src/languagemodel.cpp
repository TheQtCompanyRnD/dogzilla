// Copyright (C) 2026 The Qt Company Ltd.
// SPDX-License-Identifier: GPL-3.0-only

#include "languagemodel.h"
#include "lmresponse.h"

#include <QEventLoop>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLoggingCategory>
#include <QNetworkReply>

#include <memory>

Q_LOGGING_CATEGORY(lcLlm, "dogzilla.llm")

static inline QNetworkAccessManager *getUnderlyingNetworkManager()
{
    static auto gUnderlyingManager = std::make_unique<QNetworkAccessManager>();
    return gUnderlyingManager.get();
}

// Read the prompt file addressed by a QUrl. Handles the forms promptSource can
// take: a resolved file: URL (Qt.resolvedUrl from disk), a qrc: URL (from a
// resource-loaded main.qml), or a bare filesystem path assigned to the QUrl
// property (e.g. "/usr/share/dogzillad/prompt.txt", which has no scheme).
static QString readPromptFile(const QUrl &url)
{
    QString path;
    if (url.scheme() == QLatin1String("qrc"))
        path = QLatin1Char(':') + url.path();
    else if (url.isLocalFile())
        path = url.toLocalFile();
    else
        path = url.path().isEmpty() ? url.toString() : url.path();

    QFile file{path};
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        qCWarning(lcLlm) << "could not open prompt" << path << ":" << file.errorString();
        return QString{};
    }
    return QString::fromUtf8(file.readAll());
}

LanguageModel::LanguageModel(QObject *parent)
    : QRestAccessManager{getUnderlyingNetworkManager(), parent}
{}

void LanguageModel::setApiUrl(const QUrl &url)
{
    if (m_apiUrl == url)
        return;
    m_apiUrl = url;
    emit apiUrlChanged();
    maybeSendPrompt();
}

void LanguageModel::setModel(const QString &name)
{
    if (m_modelName == name)
        return;
    m_modelName = name;
    emit modelChanged();
    maybeSendPrompt();
}

void LanguageModel::setPromptSource(const QUrl &newPromptSource)
{
    if (m_promptSource == newPromptSource)
        return;
    m_promptSource = newPromptSource;
    emit promptSourceChanged();
    maybeSendPrompt();
}

void LanguageModel::maybeSendPrompt()
{
    // Once per session: fire only when fully configured and not yet primed.
    if (m_promptSent || !m_apiUrl.isValid() || m_modelName.isEmpty()
        || m_promptSource.isEmpty())
        return;

    const QString prompt = readPromptFile(m_promptSource);
    if (prompt.isEmpty())
        return; // couldn't read it; retry if promptSource is set again

    m_promptSent = true;

    QJsonObject systemMessage;
    systemMessage.insert("role", "system");
    systemMessage.insert("content", prompt);
    m_messages.append(systemMessage);

    // Open the session: the model's acknowledgement of the system prompt is
    // logged rather than spoken (see sendConversation).
    sendConversation(/*isPrompt*/ true);
}

void LanguageModel::setGenerating(bool generating)
{
    if (m_generating == generating)
        return;
    m_generating = generating;
    emit generatingChanged();
}

QStringList LanguageModel::list()
{
    QUrl url = getApiUrl();
    if (!url.isValid())
        return QStringList{};
    // OpenAI-compatible endpoint, supported by both Ollama and llama.cpp's server.
    url.setPath("/v1/models");

    QNetworkRequest request{url};
    request.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
    std::unique_ptr<QNetworkReply> reply { get(request) };

    QEventLoop loop;
    connect(reply.get(), &QNetworkReply::finished, &loop, &QEventLoop::quit);
    loop.exec();

    if (reply->error() == QNetworkReply::NoError) {
        LmResponse response{reply->readAll()};
        if (response.hasError())
            return QStringList{};
        return response.getModelNames();
    } else {
        qCWarning(lcLlm) << "Error:" << reply->errorString();
        return QStringList{};
    }
}

void LanguageModel::chat(const QString &message)
{
    // Append this utterance to the running history, then send the whole
    // conversation so the model keeps its context across turns.
    QJsonObject userMessage;
    userMessage.insert("role", "user");
    userMessage.insert("content", message);
    m_messages.append(userMessage);

    sendConversation(/*isPrompt*/ false);
}

void LanguageModel::sendConversation(bool isPrompt)
{
    QUrl url = getApiUrl();
    if (!url.isValid())
        return;
    // OpenAI-compatible endpoint, supported by both Ollama and llama.cpp's server.
    url.setPath("/v1/chat/completions");

    QJsonObject body;
    body.insert("model", m_modelName);
    body.insert("messages", m_messages);
    body.insert("stream", true);

    QNetworkRequest request{url};
    request.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
    const auto json = QJsonDocument{body}.toJson();
    QNetworkReply *reply = post(request, json);
    qCDebug(lcLlm) << "request:" << json;

    setGenerating(true);

    // /v1/chat/completions streams Server-Sent Events: each chunk arrives as a
    // "data: {json}" line whose choices[0].delta.content holds the incremental
    // text, terminated by a final "data: [DONE]" sentinel. Accumulate across
    // downloadProgress events (readLine drains only newly-arrived bytes) into a
    // shared buffer so the finished handler can emit the full reply.
    auto accumulated = std::make_shared<QString>();

    auto onReply = [this, reply, accumulated, isPrompt](qint64, qint64){
        LmResponse response;
        while (reply->bytesAvailable() > 0) {
            auto line = reply->readLine().trimmed();
            if (line.isEmpty())
                continue;
            if (line.startsWith("data:"))
                line = line.sliced(5).trimmed();
            if (line.isEmpty() || line == "[DONE]")
                continue;
            response.reset(line);
            if (response.hasError())
                continue; // partial line: wait for the rest on the next event
            *accumulated += response.getChatContent();
        }
        if (!isPrompt)
            emit responseChanged(*accumulated);
    };

    auto onReplyFinished = [this, accumulated, isPrompt](){
        setGenerating(false);

        // Keep the assistant turn in history so the session continues.
        QJsonObject assistantMessage;
        assistantMessage.insert("role", "assistant");
        assistantMessage.insert("content", *accumulated);
        m_messages.append(assistantMessage);

        if (isPrompt)
            qCDebug(lcLlm) << "session primed; model said:" << *accumulated;
        else
            emit responseReceived(*accumulated);
    };

    connect(this, &LanguageModel::stopGenerating, reply, &QNetworkReply::abort);
    connect(reply, &QNetworkReply::finished, reply, &QNetworkReply::deleteLater);
    connect(reply, &QNetworkReply::finished, this, onReplyFinished);
    connect(reply, &QNetworkReply::downloadProgress, this, onReply);
}
