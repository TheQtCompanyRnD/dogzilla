// Copyright (C) 2026 The Qt Company Ltd.
// SPDX-License-Identifier: GPL-3.0-only

#include "languagemodel.h"
#include "lmresponse.h"
#include "settings.h"

#include <QDirIterator>
#include <QEventLoop>
#include <QJsonArray>
#include <QJsonObject>
#include <QMimeDatabase>
#include <QNetworkReply>
#include <QRegularExpression>
#include <QRestReply>
#include <QQuickTextDocument>
#include <QTextBlock>
#include <QTextCursor>
#include <QTextDocumentFragment>
#include <QTextStream>

static inline QNetworkAccessManager *getUnderlyingNetworkManager()
{
    static auto gUnderlyingManager = std::make_unique<QNetworkAccessManager>();
    return gUnderlyingManager.get();
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
}


QQuickTextDocument *LanguageModel::outputDocument() const
{
    return m_outputDocument;
}

void LanguageModel::setOutputDocument(QQuickTextDocument *newOutputDocument)
{
    if (m_outputDocument == newOutputDocument)
        return;
    m_outputDocument = newOutputDocument;
    emit outputDocumentChanged();
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
        LmResponse ollamaResponse{reply->readAll()};
        if (ollamaResponse.hasError())
            return QStringList{};
        return ollamaResponse.getModelNames();
    } else {
        qDebug() << "Error:" << reply->errorString();
        return QStringList{};
    }

    return QStringList{};
}

void LanguageModel::startChat(const QString &modelName)
{
    if (m_model.getName() == modelName)
        return;
    m_model.setName(modelName);
}

void LanguageModel::chat(const QString &message)
{
    QUrl url = getApiUrl();
    if (!url.isValid())
        return;
    // OpenAI-compatible endpoint, supported by both Ollama and llama.cpp's server.
    url.setPath("/v1/chat/completions");

    if (m_chatLog.isOpen()) {
        m_chatLog.write(message.toUtf8());
        m_chatLog.write("\n- - -\n\n");
    }
    m_model.addMessage(message, true);
    emit requested(message);

    const QByteArray data = m_model.toJson();
    QNetworkRequest request{url};
    request.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
    QNetworkReply *reply = post(request, data);

    m_model.addMessage("", false);
    if (m_appendCursor.isNull()) {
        auto *doc = m_outputDocument->textDocument();
        m_appendCursor = QTextCursor(doc);
        m_appendCursor.movePosition(QTextCursor::End);
    }
    m_generationBeginPos = m_appendCursor.position();
    // qDebug() << "generating begins @" << m_generationBeginPos;
    setGenerating(true);

    auto onReply = [this, reply](qint64, qint64){
        QString response;
        LmResponse ollamaResponse;
        while (reply->bytesAvailable() > 0) {
            auto line = reply->readLine().trimmed();
            // The OpenAI-compatible streaming format is Server-Sent Events: each chunk
            // arrives as a "data: {json}" line, with a final "data: [DONE]" sentinel.
            // Strip the "data:" prefix and skip keep-alive blanks and the sentinel.
            if (line.isEmpty())
                continue;
            if (line.startsWith("data:"))
                line = line.sliced(5).trimmed();
            if (line.isEmpty() || line == "[DONE]")
                continue;
            ollamaResponse.reset(line);
            // qDebug() << line << ollamaResponse.getResponse() << ollamaResponse.getChatContent();
            QString text = ollamaResponse.getChatContent();
            response += text;
            // qDebug() << text;
            bool codeBlockEnded = false;
            if (text.contains("``")) {
                if (m_inCodeBlock)
                    codeBlockEnded = true;
                m_inCodeBlock = !m_inCodeBlock;
                // qDebug() << "code block?" << m_inCodeBlock << "ended?" << codeBlockEnded << text;
            }
            if (m_chatLog.isOpen())
                m_chatLog.write(text.toUtf8());
            if (m_outputDocument) {
                if (!text.isEmpty()) {
                    // qDebug() << text;
                    // If a new paragraph is being started, reparse the previous one as markdown.
                    if (codeBlockEnded || (!m_inCodeBlock && text.startsWith("\n") && !text.contains('|'))) {
                        m_chatLog.flush();
                        if (!codeBlockEnded)
                            text = text.mid(text.startsWith("\n\n") ? 2 : 1); // remainder can be inserted as plain text
                        m_appendCursor.setPosition(m_lastMarkdownOutputPos, QTextCursor::KeepAnchor);
                        m_appendCursor.removeSelectedText();
                        const bool isListItem = m_partialBlock.startsWith('-') || m_partialBlock.startsWith('+') ||
                                                m_partialBlock.startsWith("* ") || m_partialBlock.at(0).isDigit();
                        // Ensure that previous list is terminated when the new block is not a list item
                        if (m_inList && !isListItem)
                            m_appendCursor.setBlockFormat({});
                        m_appendCursor.insertMarkdown(m_partialBlock);
                        m_appendCursor.insertBlock();
                        m_lastMarkdownOutputPos = m_appendCursor.position();
                        m_partialBlock.clear();
                        m_inList = isListItem;
                    }
                    m_partialBlock += text;
                    m_appendCursor.insertText(text);
                }
            }
            emit responded(text);
        }
        m_model.updateMessage(response);
    };

    auto onReplyFinished = [this](){
        if (m_chatLog.isOpen()) {
            m_chatLog.write("\n- - -\n\n");
            m_chatLog.flush();
        }
        findDiffs();
        setGenerating(false);
    };

    connect(this, &LanguageModel::stopGenerating, [this, reply]() {
        m_partialBlock.clear();
        m_appendCursor.insertText("⏹");
        m_appendCursor.insertBlock();
        reply->abort();
    });
    connect(reply, &QNetworkReply::finished, reply, &QNetworkReply::deleteLater);
    connect(reply, &QNetworkReply::finished, this, onReplyFinished);
    connect(reply, &QNetworkReply::downloadProgress, this, onReply);
}

QString pathToFile(const QString &fname, const QString &rootPath, const QStringList &contextPaths)
{
    QString toFind = "/" + fname;
    for (const auto &path : std::as_const(contextPaths))
        if (path.endsWith(toFind) || path == fname) {
            qDebug() << "found in contextPaths:" << fname << path;
            return path;
        }
    // OK it wasn't found in contextPaths (or perhaps that's empty): try harder to find the file
    // TODO find all context files in one pass, as an optimization?
    QDirIterator it(rootPath, {fname}, QDir::Files, QDirIterator::Subdirectories);
    if (it.hasNext()) {
        QFileInfo ret = it.nextFileInfo();
        qDebug() << "found in project dir:" << fname << ret;
        return ret.absoluteFilePath();
    }
    return {};
}

/*!
    Finds all the files referenced by `@filename` in \a message (searching \a contextPaths first,
    and then searching on the filesystem under \a rootPath if not found in \a contextPaths);
    reads the files and substitutes them into markdown code blocks labeled with
    the language (mime type) and filename; and calls \l chat() with the prompt thus expanded.
    Returns the expanded prompt (for adding to the chat log).
 */
QString LanguageModel::chatWithContext(const QString &message, const QString &rootPath, const QStringList &contextPaths)
{
    // qDebug() << message << contextPaths;
    QString out;
    // Expand @file references by reading the files into Markdown code blocks
    static const QRegularExpression atNameRe("@([\\.\\w]+)");
    QRegularExpressionMatch match = atNameRe.match(message);
    qsizetype lastRefIdx = 0; // beginning of message
    QMimeDatabase mdb;
    while (match.hasMatch()) {
        QString fname = match.captured(1);
        QString path = pathToFile(fname, rootPath, contextPaths);
        // TODO or should we use mimeTypeForData()? or shorten it, like "C++" instead of "text/x-c++src"?
        auto types = mdb.mimeTypesForFileName(fname);
        qsizetype refIdx = match.capturedStart(1);
        // qDebug() << refIdx << fname << path << types;
        out += message.mid(lastRefIdx, refIdx - lastRefIdx - 1);
        QFile ctxfile(path);
        if (ctxfile.open(QIODeviceBase::ReadOnly)) {
            m_contextPaths.insert(fname, path);
            QString content = QString::fromUtf8(ctxfile.readAll());
            if (content.size()) {
                out += QString("\n```");
                if (!types.isEmpty())
                    out += types.first().name();
                out += " " + fname + "\n" + content + "\n```\n";
            }
            ctxfile.close();
        } else {
            qWarning() << "failed to open" << path;
        }

        lastRefIdx = refIdx; // + match.capturedLength(1);
        match = atNameRe.match(message, lastRefIdx + match.capturedLength(1));
    }
    out += message.mid(lastRefIdx);
    // qDebug() << out;
    chat(out);
    return out;
}

QStringList LanguageModel::savedApiUrls() const
{
    static QStringList defaultUrls = {"", "http://localhost:11434"};
    QStringList ret = Settings::instance()->stringListOrDefault(
        Settings::mainGroup, Settings::llmUrls, "url", defaultUrls);
    if (!ret.first().isEmpty())
        ret.prepend({});
    return ret;
}

void LanguageModel::setSavedApiUrls(const QStringList &list)
{
    QStringList dedup;
    for (const auto &s : list)
        if (!dedup.contains(s))
            dedup.append(s);
    Settings::instance()->setStringList(
        Settings::mainGroup, Settings::llmUrls, "url", dedup);
    emit savedApiUrlsChanged();
}

QUrl LanguageModel::chatLog() const
{
    return m_chatLogUrl;
}

void LanguageModel::setChatLog(const QUrl &url)
{
    if (m_chatLogUrl == url)
        return;
    // qDebug() << url << url.toLocalFile();
    m_chatLogUrl = url;
    if (m_chatLog.isOpen())
        m_chatLog.close();
    m_chatLog.setFileName(url.toLocalFile());
    if (!m_chatLog.open(QIODeviceBase::WriteOnly | QIODeviceBase::Append))
        qWarning() << "failed to open" << url;
    emit chatLogChanged();
}

void LanguageModel::findDiffs()
{
    if (!m_outputDocument)
        return;
    QTextCursor cur(m_outputDocument->textDocument());
    bool ok = true;
    // qDebug() << "looking for code blocks starting from" << cur.position() << cur.block().text().first(16);
    bool isDiff = false;
    QString diff;
    while (ok) {
        auto bfmt = cur.blockFormat();
        if (bfmt.hasProperty(QTextFormat::BlockCodeLanguage)) {
            // qDebug() << "found code block" << bfmt.property(QTextFormat::BlockCodeLanguage) << cur.block().text();
            if (bfmt.property(QTextFormat::BlockCodeLanguage).toString() == "diff")
                isDiff = true;
        } else if (isDiff) {
            isDiff = false;
            applyDiff(diff);
            diff.clear();
        }
        if (isDiff) {
            diff.append(cur.block().text());
            diff.append('\n');
        }
        ok = cur.movePosition(QTextCursor::NextBlock);
    }
}

void LanguageModel::applyDiff(QString diff)
{
    QString fileName;
    // diff is a by-value copy so we can take its non-const address
    QTextStream str(&diff, QIODeviceBase::ReadOnly);
    while (fileName.isEmpty() && !str.atEnd()) {
        QString line = str.readLine();
        if (line.startsWith("---") || line.startsWith("+++")) {
            auto words = line.split(' ');
            if (words.size() > 1)
                fileName = words.at(1);
        }
    }
    QString path = m_contextPaths.value(fileName);
    if (path.isEmpty())
        path = fileName;
    // Deal with the case that the patch has a/filename.cpp or b/...
    int slashIdx = path.lastIndexOf('/');
    if (slashIdx == 1)
        path = path.mid(slashIdx + 1);
    // Let QML UI figure out what to do with the diff.
    qDebug() << "diff for" << path << ":\n" << diff;
    emit patchGenerated(path, diff);
}
