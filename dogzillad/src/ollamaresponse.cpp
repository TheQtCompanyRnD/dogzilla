#include "ollamaresponse.h"

#include <QJsonArray>
#include <QJsonObject>

OllamaResponse::OllamaResponse(const QByteArray &data)
{
    reset(data);
}

void OllamaResponse::reset(const QByteArray &data)
{
    jsonDoc = QJsonDocument::fromJson(data, &error);
    if (error.error != QJsonParseError::NoError)
        qDebug() << "Ollama response parse error:" << error.errorString();
}

QStringList OllamaResponse::getModelNames() const
{
    if (!jsonDoc.isObject())
        return QStringList{};
    const QJsonObject rootObject = jsonDoc.object();
    const QJsonValue models = rootObject.value("models");
    Q_ASSERT(models.isArray());
    const QJsonArray modelsArray = models.toArray();
    QStringList modelNames;
    for (auto it = modelsArray.begin(); it != modelsArray.end(); ++it) {
        Q_ASSERT(it->isObject());
        const QJsonObject model = it->toObject();
        const QJsonValue name = model.value("name");
        Q_ASSERT(name.isString());
        modelNames << name.toString();
    }
    return modelNames;
}

bool OllamaResponse::isDone() const
{
    if (!jsonDoc.isObject())
        return false;
    const QJsonObject rootObject = jsonDoc.object();
    const QJsonValue done = rootObject.value("done");
    Q_ASSERT(done.isBool());
    return done.toBool();
}

QString OllamaResponse::getResponse() const
{
    if (!jsonDoc.isObject())
        return QString{};
    const QJsonObject rootObject = jsonDoc.object();
    const QJsonValue response = rootObject.value("response");
    Q_ASSERT(response.isString());
    return response.toString();
}

QString OllamaResponse::getChatContent() const
{
    if (!jsonDoc.isObject())
        return QString{};
    const QJsonObject rootObject = jsonDoc.object();
    const QJsonValue message = rootObject.value("message");
    Q_ASSERT(message.isObject());
    const QJsonObject messageObject = message.toObject();
    auto it = messageObject.find("content");
    Q_ASSERT(it != messageObject.end());
    return it->toString();
}
