#ifndef CHATMODEL_H
#define CHATMODEL_H

#include <QAbstractListModel>
#include <QJsonArray>
#include <QQmlEngine>

class ChatModel : public QAbstractListModel
{
    Q_OBJECT
    QML_ELEMENT

    Q_PROPERTY(QString name READ getName WRITE setName NOTIFY nameChanged FINAL)
    Q_PROPERTY(int size READ getSize NOTIFY sizeChanged FINAL)

public:
    ChatModel(QObject *parent = nullptr);

    QString getName() const { return m_modelName; }
    void setName(const QString &name);

    int getSize() const { return rowCount(QModelIndex{}); }

    void addMessage(const QString &message, bool user);
    void updateMessage(const QString &message);

    QByteArray toJson() const;

    Q_INVOKABLE void reset();

    // QAbstractItemModel interface
    virtual int rowCount(const QModelIndex &parent) const override;
    virtual QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;
    virtual QHash<int, QByteArray> roleNames() const override;

signals:
    void nameChanged();
    void sizeChanged();

private:
    QString m_modelName;
    QJsonArray m_messages;
};

#endif // CHATMODEL_H
