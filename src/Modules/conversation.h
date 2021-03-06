﻿#ifndef CONVERSATION_H
#define CONVERSATION_H

#include <QObject>
#include <QDateTime>


class PeerSession;
class ServerConnection;
class ConversationPrivate;

class Conversation : public QObject
{
    Q_OBJECT
    Q_DECLARE_PRIVATE(Conversation)
protected:
    ConversationPrivate* d_ptr;

public:
    enum class VerifyError
    {
        Ok = 0,
        NetworkError = 1,
        VersionNotSupported = 2,
        SessionFormatError = 3,
        KeyFormatError = 4,
        VerificationFailed = 5,
        UnknownError = 255
    };
    enum class QueryError
    {
        Ok = 0,
        NetworkError = 1,
        VersionNotSupported = 2,
        UnknownError = 255
    };
    struct MessageListEntry
    {
        QString ID;
    };
    struct MessageEntry
    {
        QString source;
        QDateTime time;
        int length;
        QByteArray content;
    };

    static const int MaxIDLen = 8;
    static const int SessionLen = 16;
    static const int KeyLen = 16;
    static const int RecordSaltLen = 16;

    explicit Conversation();
    explicit Conversation(ServerConnection& server);
    ~Conversation();

    void setUserDirectory(QString path);
    void setPeerSession(PeerSession& sessionList);
    bool verify(QByteArray sessionID, QByteArray sessionKey);
    QByteArray& keySalt();
    bool resetSession();
    bool sendMessage(QString ID, QByteArray& content, int& queryID);
    bool getMessageList();
    bool receiveMessage(QString ID, int& queryID);
    bool fixBrokenConnection(QString ID, int& queryID);

signals:
    void queryError(int queryID, Conversation::QueryError errorCode);
    void connectionBroken(QString ID);
    void verifyFinished(int queryID, Conversation::VerifyError errorCode);
    void resetSessionFinished(int queryID, bool successful);
    void sendMessageFinished(int queryID, bool successful);
    void getMessageListFinished(int queryID,
                                QList<Conversation::MessageListEntry>& msgList);
    void receiveMessageFinished(int queryID,
                                QList<Conversation::MessageEntry>& messages);
    void fixBrokenConnectionFinished(int queryID, bool successful);

private:
    void dispatchQueryRespone(int requestID);

private slots:
    void onPrivateEvent(int eventType, int data);
};

#endif // CONVERSATION_H
