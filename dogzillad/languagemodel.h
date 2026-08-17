// Copyright (C) 2026 The Qt Company Ltd.
// SPDX-License-Identifier: GPL-3.0-only

#ifndef OLLAMAAPI_H
#define OLLAMAAPI_H

#include "chatmodel.h"

#include <QFile>
#include <QObject>
#include <QQmlEngine>
#include <QRestAccessManager>
#include <QTextCursor>

class QQuickTextDocument;

class LanguageModel : public QRestAccessManager
{
    Q_OBJECT
    QML_ELEMENT

    Q_PROPERTY(QUrl apiUrl READ getApiUrl WRITE setApiUrl NOTIFY apiUrlChanged FINAL)
    Q_PROPERTY(QUrl chatLog READ chatLog WRITE setChatLog NOTIFY chatLogChanged FINAL)
    Q_PROPERTY(QQuickTextDocument* outputDocument READ outputDocument WRITE setOutputDocument NOTIFY outputDocumentChanged FINAL)
    Q_PROPERTY(ChatModel* model READ model NOTIFY modelChanged FINAL)
    Q_PROPERTY(bool generating READ isGenerating NOTIFY generatingChanged FINAL)

    // workaround for lack of settings array support in QtCore Settings
    Q_PROPERTY(QStringList savedApiUrls READ savedApiUrls WRITE setSavedApiUrls NOTIFY savedApiUrlsChanged FINAL)

public:
    explicit LanguageModel(QObject *parent = nullptr);

    const QUrl &getApiUrl() const { return m_apiUrl; }
    void setApiUrl(const QUrl &url);

    QQuickTextDocument *outputDocument() const;
    void setOutputDocument(QQuickTextDocument *newOutputDocument);

    ChatModel *model() { return &m_model; }

    bool isGenerating() const { return m_generating; }
    void setGenerating(bool generating);

    Q_INVOKABLE QStringList list();
    Q_INVOKABLE void startChat(const QString &modelName);
    Q_INVOKABLE void chat(const QString &message);
    Q_INVOKABLE QString chatWithContext(const QString &message, const QString &rootPath,
                                        const QStringList &contextPaths = {});

    QStringList savedApiUrls() const;
    void setSavedApiUrls(const QStringList &list);

    QUrl chatLog() const;
    void setChatLog(const QUrl &url);

signals:
    // property notifiers
    void apiUrlChanged();
    void chatLogChanged();
    void outputDocumentChanged();
    void modelChanged();
    void generatingChanged();
    void savedApiUrlsChanged();

    // other signals
    void stopGenerating();
    void error(QString error);
    void requested(QString text);
    void responded(QString text);
    void patchGenerated(QString filename, QString diff);

private:
    void findDiffs();
    void applyDiff(QString diff);

private:
    QUrl m_apiUrl;
    ChatModel m_model;
    bool m_generating = false;
    bool m_inList = false; // is m_partialBlock a list item?
    bool m_inCodeBlock = false;
    int m_lastMarkdownOutputPos = 0; // where was m_appendCursor after last time we called insertMarkdown()
    int m_generationBeginPos; // where was m_appendCursor when generating started
    QQuickTextDocument *m_outputDocument = nullptr;
    QTextCursor m_appendCursor;
    QString m_partialBlock;
    QUrl m_chatLogUrl;
    QFile m_chatLog;
    QHash<QString, QString> m_contextPaths; // map from @context to discovered file path
};

#endif // OLLAMAAPI_H
