#include "ollamaapi.h"
#include "ollamaresponse.h"

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

OllamaApi::OllamaApi(QObject *parent)
    : QRestAccessManager{getUnderlyingNetworkManager(), parent}
{}

void OllamaApi::setApiUrl(const QUrl &url)
{
    if (m_apiUrl == url)
        return;
    m_apiUrl = url;
    emit apiUrlChanged();
    maybeSendPrompt();
}

void OllamaApi::setModel(const QString &name)
{
    if (m_modelName == name)
        return;
    m_modelName = name;
    emit modelChanged();
    maybeSendPrompt();
}

void OllamaApi::setPromptSource(const QUrl &newPromptSource)
{
    if (m_promptSource == newPromptSource)
        return;
    m_promptSource = newPromptSource;
    emit promptSourceChanged();
    maybeSendPrompt();
}

void OllamaApi::maybeSendPrompt()
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
    // Append this utterance to the running history, then send the whole
    // conversation so the model keeps its context across turns.
    QJsonObject userMessage;
    userMessage.insert("role", "user");
    userMessage.insert("content", message);
    m_messages.append(userMessage);

    sendConversation(/*isPrompt*/ false);
}

void OllamaApi::sendConversation(bool isPrompt)
{
    QUrl url = getApiUrl();
    if (!url.isValid())
        return;
    url.setPath("/api/chat");

    QJsonObject body;
    body.insert("model", m_modelName);
    body.insert("messages", m_messages);

    QNetworkRequest request{url};
    request.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
    const auto json = QJsonDocument{body}.toJson();
    QNetworkReply *reply = post(request, json);
    qCDebug(lcLlm) << "request:" << json;

    setGenerating(true);

    // /api/chat streams NDJSON: one JSON object per line, each with an
    // incremental message.content. Accumulate across downloadProgress events
    // (readLine drains only the newly-arrived bytes) into a shared buffer so the
    // finished handler can emit the full reply.
    auto accumulated = std::make_shared<QString>();

    auto onReply = [this, reply, accumulated, isPrompt](qint64, qint64){
        OllamaResponse ollamaResponse;
        while (reply->bytesAvailable() > 0) {
            ollamaResponse.reset(reply->readLine());
            if (ollamaResponse.hasError())
                continue; // partial line: wait for the rest on the next event
            *accumulated += ollamaResponse.getChatContent();
        }
        if (!isPrompt) {
            qCDebug(lcLlm) << "response:" << *accumulated;
            emit responseChanged(*accumulated);
        }
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

    connect(this, &OllamaApi::stopGenerating, reply, &QNetworkReply::abort);
    connect(reply, &QNetworkReply::finished, reply, &QNetworkReply::deleteLater);
    connect(reply, &QNetworkReply::finished, this, onReplyFinished);
    connect(reply, &QNetworkReply::downloadProgress, this, onReply);
}
