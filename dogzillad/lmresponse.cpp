// Copyright (C) 2026 The Qt Company Ltd.
// SPDX-License-Identifier: GPL-3.0-only

#include "lmresponse.h"

#include <QJsonArray>
#include <QJsonObject>

LmResponse::LmResponse(const QByteArray &data)
{
    reset(data);
}

void LmResponse::reset(const QByteArray &data)
{
    jsonDoc = QJsonDocument::fromJson(data, &error);
    if (error.error != QJsonParseError::NoError)
        qDebug() << "LM response parse error:" << error.errorString();
}

QStringList LmResponse::getModelNames() const
{
    if (!jsonDoc.isObject())
        return QStringList{};
    const QJsonObject rootObject = jsonDoc.object();
    // OpenAI-compatible /v1/models uses {"data": [{"id": ...}]} (both llama.cpp and
    // Ollama support it); Ollama's native /api/tags uses {"models": [{"name": ...}]}.
    const bool openAiStyle = rootObject.contains("data");
    const QJsonValue models = rootObject.value(openAiStyle ? "data" : "models");
    if (!models.isArray())
        return QStringList{};
    const QJsonArray modelsArray = models.toArray();
    const QString key = openAiStyle ? QStringLiteral("id") : QStringLiteral("name");
    QStringList modelNames;
    for (const QJsonValue &value : modelsArray) {
        if (!value.isObject())
            continue;
        const QJsonValue name = value.toObject().value(key);
        if (name.isString())
            modelNames << name.toString();
    }
    return modelNames;
}

bool LmResponse::isDone() const
{
    if (!jsonDoc.isObject())
        return false;
    const QJsonObject rootObject = jsonDoc.object();
    const QJsonValue done = rootObject.value("done");
    Q_ASSERT(done.isBool());
    return done.toBool();
}

QString LmResponse::getResponse() const
{
    if (!jsonDoc.isObject())
        return QString{};
    const QJsonObject rootObject = jsonDoc.object();
    const QJsonValue response = rootObject.value("response");
    if (!response.isString()) {
        qWarning() << "response is not a string:" << rootObject;
        return {};
    }
    return response.toString();
}

QString LmResponse::getChatContent() const
{
    if (!jsonDoc.isObject())
        return QString{};
    const QJsonObject rootObject = jsonDoc.object();

    // OpenAI-compatible /v1/chat/completions (both llama.cpp and Ollama): the streamed
    // content is in choices[0].delta.content; a non-streamed reply uses
    // choices[0].message.content instead.
    const QJsonValue choices = rootObject.value("choices");
    if (choices.isArray()) {
        const QJsonArray choicesArray = choices.toArray();
        if (choicesArray.isEmpty() || !choicesArray.first().isObject())
            return {};
        const QJsonObject choice = choicesArray.first().toObject();
        const QJsonObject delta = choice.value("delta").toObject();
        if (const QJsonValue content = delta.value("content"); content.isString())
            return content.toString();
        const QJsonObject message = choice.value("message").toObject();
        return message.value("content").toString();
    }

    // Ollama's native /api/chat: content is in message.content.
    const QJsonValue message = rootObject.value("message");
    if (!message.isObject()) {
        auto err = rootObject.value("error");
        if (err.isString())
            return err.toString();
        // OpenAI-compatible errors nest the message: {"error": {"message": ...}}.
        if (err.isObject())
            return err.toObject().value("message").toString();
        qWarning() << "unexpected chat response:" << rootObject;
        return {};
    }
    return message.toObject().value("content").toString();
}
