#ifndef OLLAMAAPI_H
#define OLLAMAAPI_H

#include <QJsonArray>
#include <QObject>
#include <QQmlEngine>
#include <QRestAccessManager>
#include <QUrl>

// Minimal Ollama /api/chat client for the daemon. Session-based and
// multi-turn: once apiUrl, model and promptSource are all set, the system
// prompt in promptSource is sent once to open the session (its reply is only
// logged, not spoken), and the conversation history is then carried across
// every chat() call so the dog stays in character. Unlike the digital twin's
// ChatModel-backed version, dogzillad has no GUI -- it feeds whisper
// transcripts in via chat() and emits the reply via responseReceived(), which
// main.qml routes to speak() (TTS + the /speech/log chat topic the twin
// renders).
class OllamaApi : public QRestAccessManager
{
    Q_OBJECT
    QML_ELEMENT

    Q_PROPERTY(QUrl apiUrl READ getApiUrl WRITE setApiUrl NOTIFY apiUrlChanged FINAL)
    Q_PROPERTY(QString model READ model WRITE setModel NOTIFY modelChanged FINAL)
    Q_PROPERTY(QUrl promptSource READ promptSource WRITE setPromptSource NOTIFY promptSourceChanged FINAL)
    Q_PROPERTY(bool generating READ isGenerating NOTIFY generatingChanged FINAL)

public:
    explicit OllamaApi(QObject *parent = nullptr);

    const QUrl &getApiUrl() const { return m_apiUrl; }
    void setApiUrl(const QUrl &url);

    QString model() const { return m_modelName; }
    void setModel(const QString &name);

    QUrl promptSource() const { return m_promptSource; }
    void setPromptSource(const QUrl &newPromptSource);

    bool isGenerating() const { return m_generating; }
    void setGenerating(bool generating);

    Q_INVOKABLE QStringList list();
    Q_INVOKABLE void chat(const QString &message);

signals:
    void apiUrlChanged();
    void modelChanged();
    void generatingChanged();
    void stopGenerating();

    // Incremental assistant text as the reply streams in (for a live view).
    void responseChanged(const QString &partial);
    // The complete assistant reply, once generation finishes. This is what the
    // dog "says": main.qml connects it to speak().
    void responseReceived(const QString &response);

    void promptSourceChanged();

private:
    // Send the system prompt once, when apiUrl/model/promptSource are all set.
    void maybeSendPrompt();
    // POST the current m_messages to /api/chat. isPrompt=true is the priming
    // turn: the reply is logged rather than emitted via responseReceived().
    void sendConversation(bool isPrompt);

private:
    QUrl m_apiUrl;
    QString m_modelName;
    QUrl m_promptSource;
    // Running conversation history (system, then alternating user/assistant),
    // resent in full on every request so the session keeps its context.
    QJsonArray m_messages;
    bool m_promptSent = false;
    bool m_generating = false;
};

#endif // OLLAMAAPI_H
