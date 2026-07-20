#include "chatmodel.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>

constexpr int MessageRole = Qt::UserRole + 1;

ChatModel::ChatModel(QObject *parent) : QAbstractListModel(parent) {}

void ChatModel::setName(const QString &name)
{
    if (m_modelName == name)
        return;
    m_modelName = name;
    emit nameChanged();
}

void ChatModel::addMessage(const QString &message, int source)
{
    QJsonObject object;
    object.insert("source", source);
    object.insert("content", message);
    beginInsertRows(QModelIndex{}, m_messages.size(), m_messages.size());
    m_messages.append(object);
    endInsertRows();
    emit sizeChanged();
}

void ChatModel::updateMessage(const QString &message)
{
    if (m_messages.empty())
        return;
    Q_ASSERT(m_messages.last().isObject());
    QJsonObject object = m_messages[m_messages.size() - 1].toObject();
    object["content"] = object["content"].toString() + message;
    m_messages[m_messages.size() - 1] = object;
    const QModelIndex lastIndex = index(getSize() - 1, 0);
    emit dataChanged(lastIndex, lastIndex);
}

QByteArray ChatModel::toJson() const
{
    QJsonObject object;
    object.insert("model", m_modelName);
    object.insert("messages", m_messages);
    return QJsonDocument{object}.toJson();
}

void ChatModel::reset()
{
    const int oldSize = m_messages.size();
    beginResetModel();
    m_messages = QJsonArray{};
    endResetModel();
    if (oldSize > 0)
        emit sizeChanged();
}

int ChatModel::rowCount(const QModelIndex &) const
{
    return m_messages.size();
}

QVariant ChatModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid())
        return QVariant{};
    if (index.row() >= m_messages.size())
        return QVariant{};

    if (role == Qt::DisplayRole) {
        const QJsonValue value = m_messages.at(index.row());
        Q_ASSERT(value.isObject());
        const QJsonObject object = value.toObject();
        auto it = object.find("content");
        Q_ASSERT(it != object.end());
        Q_ASSERT(it->isString());
        return it->toString();
    } else if (role == MessageRole) {
        const QJsonValue value = m_messages.at(index.row());
        Q_ASSERT(value.isObject());
        const QJsonObject object = value.toObject();
        auto it = object.find("source");
        Q_ASSERT(it != object.end());
        qDebug() << value << "expect source int" << it.value();
        return it->toInt();
    }

    return QVariant{};
}

QHash<int, QByteArray> ChatModel::roleNames() const
{
    QHash<int, QByteArray> roles = QAbstractListModel::roleNames();
    roles.insert(MessageRole, "source");
    return roles;
}
