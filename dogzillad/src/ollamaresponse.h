#ifndef OLLAMARESPONSE_H
#define OLLAMARESPONSE_H

#include <QByteArray>
#include <QJsonDocument>

class OllamaResponse
{
public:
    OllamaResponse() = default;
    OllamaResponse(const QByteArray &data);

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
