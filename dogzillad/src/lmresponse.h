// Copyright (C) 2026 The Qt Company Ltd.
// SPDX-License-Identifier: GPL-3.0-only

#ifndef OLLAMARESPONSE_H
#define OLLAMARESPONSE_H

#include <QByteArray>
#include <QJsonDocument>

class LmResponse
{
public:
    LmResponse() = default;
    LmResponse(const QByteArray &data);

    void reset(const QByteArray &data);

    bool hasError() const { return error.error != QJsonParseError::NoError; }

    QStringList getModelNames() const;
    bool isDone() const;
    QString getResponse() const;
    QString getChatContent() const;

private:
    QJsonDocument jsonDoc;
    QJsonParseError error;
};

#endif // OLLAMARESPONSE_H
