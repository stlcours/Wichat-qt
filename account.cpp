﻿#include "common.h"
#include "account.h"
#include "Private/account_p.h"
#include "serverconnection.h"

#define WICHAT_SERVER_PATH_ACCOUNT_LOGIN "/Account/log/login.php"
#define WICHAT_SERVER_PATH_ACCOUNT_ACTION "/Account/acc/action.php"
#define WICHAT_SERVER_PATH_RECORD_ACTION "/Record/query/action.php"
#define WICHAT_SERVER_PATH_RECORD_GET "/Record/query/get.php"

#define WICHAT_ACCOUNT_STATE_DEFAULT 0
#define WICHAT_ACCOUNT_STATE_ONLINE 1
#define WICHAT_ACCOUNT_STATE_OFFLINE 2
//#define WICHAT_ACCOUNT_STATE_LOGOUT 3
#define WICHAT_ACCOUNT_STATE_BUSY 4
#define WICHAT_ACCOUNT_STATE_HIDE 5

#define WICHAT_RELATION_STATE_NONE 0
#define WICHAT_RELATION_STATE_WAITING 1
#define WICHAT_RELATION_STATE_ESTABLISHED 2
#define WICHAT_RELATION_STATE_BREAKING 3
//#define WICHAT_RELATION_STATE_REFUSED 4


Account::Account()
{
    this->d_ptr = new AccountPrivate;
    connect(d_ptr,
            &AccountPrivate::privateEvent,
            this,
            &Account::onPrivateEvent);
}

bool Account::checkID(QString ID)
{
    int length = ID.length();
    if (length < 1 || length > MaxIDLen)
        return false;
    for (int i=0; i<length; i++)
    {
        char ch = ID[i].toLatin1();
        if (ch < '0' || ch > '9')
            return false;
    }
    return true;
}

bool Account::checkPassword(QString password)
{
    int length = password.length();
    if (length < 1 || length > MaxPasswordLen)
        return false;
    for (int i=0; i<length; i++)
    {
        char ch = password[i].toLatin1();
        if (!((ch >= '0' && ch <= '9') ||
              (ch >= 'A' && ch <= 'Z') ||
              (ch >= 'a' && ch <= 'z')))
            return false;
    }
    return true;
}

// TODO: make login requests asynchronous
Account::VerifyError Account::verify(QString ID, QString password)
{
    Q_D(Account);
    if (!checkID(ID))
        return VerifyError::IDFormatError;
    if (!checkPassword(password))
        return VerifyError::PasswordFormatError;

    // Build pre-login request
    QByteArray bufferIn, bufferOut;
    QByteArray hashedPassword = d->encoder.getSHA256(password.toLatin1())
                                          .left(KeyLen);
    QByteArray tempKey = d->encoder.genKey("", true).left(KeyLen);
    QByteArray tempKey2;
    d->encoder.encrypt(Encryptor::AES,
                       tempKey,
                       hashedPassword,
                       tempKey2);
    bufferIn.append(char(WICHAT_CLIENT_DEVICE)).append(char(1));
    bufferIn.append(d->encoder.fuse(d->formatID(ID), tempKey2));
    bufferIn.append(tempKey2);

    // Send pre-login request
    if (d->server.sendRawData(bufferIn, bufferOut,
                              WICHAT_SERVER_ID_ACCOUNT,
                              d->serverObjectToPath(
                                    AccountPrivate::ServerObject::AccountLogin))
                == RequestManager::CannotConnect)
        return VerifyError::NetworkError;
    if (bufferOut[0] == char(WICHAT_SERVER_RESPONSE_DEVICE_UNSUPPORTED))
        return VerifyError::VersionNotSupported;
    if (bufferOut[0] != char(WICHAT_SERVER_RESPONSE_SUCCESS))
        return VerifyError::UnknownError;

    // Extract pre-encryption key
    bufferOut.remove(0, 1);
    d->encoder.decrypt(Encryptor::AES,
                       bufferOut,
                       hashedPassword,
                       tempKey2);
    tempKey = d->encoder.byteXOR(tempKey, tempKey2);


    // Build login request
    bufferIn.clear();
    bufferIn.append(hashedPassword);
    bufferIn.append(char(OnlineState::Online));
    d->encoder.encrypt(Encryptor::AES, bufferIn, tempKey, bufferOut);
    bufferIn.clear();
    bufferIn.append(char(WICHAT_CLIENT_DEVICE)).append(char(2));
    bufferIn.append(d->encoder.getHMAC(d->formatID(ID), tempKey)
                              .left(MaxIDLen));
    bufferIn.append(bufferOut);

    // Send login request
    if (d->server.sendRawData(bufferIn, bufferOut,
                              WICHAT_SERVER_ID_ACCOUNT,
                              d->serverObjectToPath(
                                    AccountPrivate::ServerObject::AccountLogin))
                == RequestManager::CannotConnect)
        return VerifyError::NetworkError;
    if (bufferOut[0] != char(WICHAT_SERVER_RESPONSE_SUCCESS))
        return VerifyError::VerificationFailed;

    // Extract session and account information
    d->currentSession = bufferOut.mid(1, SessionLen);
    d->sessionValidTime = *((qint16*)(bufferOut.mid(SessionLen + 1, 2).data()));
    d->currentState = d->intToOnlineState(bufferOut.mid(SessionLen + 3, 1)[0]);
    bufferOut.remove(0, SessionLen + 4);
    d->encoder.decrypt(Encryptor::AES,
                       bufferOut,
                       tempKey,
                       bufferIn);
    d->sessionKey = bufferIn.mid(4, KeyLen);
    d->currentOfflineMsg = QString(bufferIn.mid(4 + KeyLen)).trimmed();
    d->currentID = ID;

    d->server.setSessionInfo(d->currentSession, d->sessionKey);
    return VerifyError::Ok;
}

QString Account::ID()
{
    Q_D(Account);
    return d->currentID;
}

bool Account::setPassword(QString oldPassword, QString newPassword)
{
    Q_D(Account);
    if (!checkPassword(oldPassword) || !checkPassword(newPassword))
        return false;
    QByteArray bufferIn, bufferOut;
    bufferIn.append(char(9)).append(char(qrand() * 256));
    bufferIn.append(d->encoder.getSHA256(oldPassword.toLatin1()).left(KeyLen));
    bufferIn.append(d->encoder.getSHA256(newPassword.toLatin1()).left(KeyLen));
    if (!d->server.sendData(bufferIn, bufferOut,
                            WICHAT_SERVER_ID_ACCOUNT,
                            d->serverObjectToPath(
                                AccountPrivate::ServerObject::AccountAction)))
        return false;
    if (bufferOut[0] != char(WICHAT_SERVER_RESPONSE_SUCCESS))
        return false;
    return true;
}

bool Account::resetSession(int& queryID)
{
    Q_D(Account);
    QByteArray bufferIn, bufferOut;
    bufferIn.append(char(5)).append(char(qrand() * 256));
    if (!d->server.sendData(bufferIn, bufferOut,
                            WICHAT_SERVER_ID_ACCOUNT,
                            d->serverObjectToPath(
                                AccountPrivate::ServerObject::AccountAction),
                            false, &queryID))
        return false;

    d->server.setRecordType(queryID,
                            RequestManager::RequestType(
                                AccountPrivate::RequestType::ResetSession));
    return true;
}

Account::OnlineState Account::state()
{
    Q_D(Account);
    return d->currentState;
}

bool Account::setState(OnlineState newState, int& queryID)
{
    Q_D(Account);
    if (d->currentState == OnlineState::None)
        return false;

    QByteArray bufferIn, bufferOut;
    bufferIn.append(char(8)).append(char(newState));
    if (!d->server.sendData(bufferIn, bufferOut,
                            WICHAT_SERVER_ID_ACCOUNT,
                            d->serverObjectToPath(
                                AccountPrivate::ServerObject::AccountAction),
                            false, &queryID))
        return false;

    d->server.setRecordType(queryID,
                            RequestManager::RequestType(
                                AccountPrivate::RequestType::SetState));
    return true;
}

QString Account::offlineMsg()
{
    Q_D(Account);
    return d->currentOfflineMsg;
}

bool Account::setOfflineMsg(QString newMessage, int &queryID)
{
    Q_D(Account);
    newMessage.truncate(MaxOfflineMsgLen - 2);

    QByteArray bufferIn, bufferOut;
    bufferIn.append(char(7)).append(char(qrand() * 256));
    bufferIn.append("<MSG>")
            .append(newMessage.toLatin1())
            .append("</MSG>");
    if (!d->server.sendData(bufferIn, bufferOut,
                            WICHAT_SERVER_ID_ACCOUNT,
                            d->serverObjectToPath(
                                AccountPrivate::ServerObject::AccountAction),
                            false, &queryID))
        return false;

    d->currentOfflineMsg = newMessage;
    d->server.setRecordType(queryID,
                            RequestManager::RequestType(
                                AccountPrivate::RequestType::SetOfflineMsg));
    return true;
}

bool Account::queryFriendList(int& queryID)
{
    Q_D(Account);

    QByteArray bufferIn, bufferOut;
    bufferIn.append(char(1)).append(char(2));
    if (!d->server.sendData(bufferIn, bufferOut,
                            WICHAT_SERVER_ID_ACCOUNT,
                            d->serverObjectToPath(
                                AccountPrivate::ServerObject::AccountAction),
                            false, &queryID))
        return false;

    d->server.setRecordType(queryID,
                            RequestManager::RequestType(
                                AccountPrivate::RequestType::GetFriendList));
    return true;
}

bool Account::addFriend(QString ID, int &queryID)
{
    Q_D(Account);

    QByteArray bufferIn, bufferOut;
    bufferIn.append(char(2)).append(char(qrand() * 256));
    bufferIn.append("<IDList><ID>")
            .append(d->formatID(ID))
            .append("</ID></IDList>");
    if (!d->server.sendData(bufferIn, bufferOut,
                            WICHAT_SERVER_ID_ACCOUNT,
                            d->serverObjectToPath(
                                  AccountPrivate::ServerObject::AccountAction),
                            false, &queryID))
        return false;

    d->server.setRecordType(queryID,
                            RequestManager::RequestType(
                                AccountPrivate::RequestType::AddFriend));
    return true;
}

bool Account::removeFriend(QString ID, int& queryID)
{
    Q_D(Account);

    QByteArray bufferIn, bufferOut;
    bufferIn.append(char(3)).append(char(qrand() * 256));
    bufferIn.append("<IDList><ID>>")
            .append(d->formatID(ID))
            .append("</ID></IDList>");
    if (!d->server.sendData(bufferIn, bufferOut,
                            WICHAT_SERVER_ID_ACCOUNT,
                            d->serverObjectToPath(
                                AccountPrivate::ServerObject::AccountAction),
                            false, &queryID))
        return false;

    d->server.setRecordType(queryID,
                            RequestManager::RequestType(
                                AccountPrivate::RequestType::RemoveFriend));
    return true;
}

bool Account::queryFriendRemarks(QList<QString> IDs, int &queryID)
{
    Q_D(Account);

    QByteArray bufferIn, bufferOut;
    bufferIn.append(char(11)).append(char(qrand() * 256));
    bufferIn.append("<IDList>");
    for (int i=0; i<IDs.count(); i++)
    {
        bufferIn.append("<ID>")
                .append(d->formatID(IDs[i]))
                .append("</ID>");
    }
    bufferIn.append("</IDList>");
    if (!d->server.sendData(bufferIn, bufferOut,
                            WICHAT_SERVER_ID_ACCOUNT,
                            d->serverObjectToPath(
                                AccountPrivate::ServerObject::AccountAction),
                            false, &queryID))
        return false;

    d->server.setRecordType(queryID,
                            RequestManager::RequestType(
                                AccountPrivate::RequestType::GetFriendRemarks));
    return true;
}

bool Account::setFriendRemarks(QString ID, QString remarks, int &queryID)
{
    Q_D(Account);

    QByteArray bufferIn, bufferOut;
    qint16 remarksLength = remarks.length();
    bufferIn.append(char(12)).append(char(qrand() * 256));
    bufferIn.append((char*)(&remarksLength))
            .append(d->formatID(ID));
    if (!d->server.sendData(bufferIn, bufferOut,
                            WICHAT_SERVER_ID_ACCOUNT,
                            d->serverObjectToPath(
                                AccountPrivate::ServerObject::AccountAction),
                            false, &queryID))
        return false;

    d->server.setRecordType(queryID,
                            RequestManager::RequestType(
                                AccountPrivate::RequestType::SetFriendRemarks));
    return true;
}

bool Account::queryFriendInfo(QString ID, int& queryID)
{
    Q_D(Account);

    QByteArray bufferIn, bufferOut;
    bufferIn.append(char(10)).append(char(1));
    bufferIn.append("<IDList><ID>")
            .append(d->formatID(ID))
            .append("</ID></IDList>");
    if (!d->server.sendData(bufferIn, bufferOut,
                            WICHAT_SERVER_ID_ACCOUNT,
                            d->serverObjectToPath(
                                AccountPrivate::ServerObject::AccountAction),
                            false, &queryID))
        return false;

    d->server.setRecordType(queryID,
                            RequestManager::RequestType(
                                AccountPrivate::RequestType::GetFriendInfo));
    return true;
}

void Account::dispatchQueryRespone(int queryID)
{
    Q_D(Account);

    bool successful;
    QByteArray data;
    QList<QByteArray> tempList;
    AccountPrivate::RequestType requestType =
            AccountPrivate::RequestType(d->server.getRecordType(queryID));
    RequestManager::RequestError errCode = d->server.getData(queryID, data);
    if (errCode == RequestManager::Ok)
    {
        if (!d->processReplyData(requestType, data))
        {
            emit queryError(queryID, QueryError::UnknownError);
            return;
        }
        switch (requestType)
        {
            // For some operations, the first byte (indicating if
            // it is successful or not) has already been checked in
            // d->processReplyData(), and no further checking is needed
            // So we simply pass "true" to "successful" in these signals
            case AccountPrivate::RequestType::ResetSession:
            {
                successful = data.length() > KeyLen;
                if (successful)
                {
                    d->currentSession = data.left(SessionLen);
                    d->sessionKey = data.mid(SessionLen, KeyLen);
                    d->server.setSessionInfo(d->currentSession, d->sessionKey);
                }
                emit resetSessionFinished(queryID, successful);
                break;
            }
            case AccountPrivate::RequestType::SetPassword:
            {
                emit setPasswordFinished(queryID, true);
                break;
            }
            case AccountPrivate::RequestType::SetState:
            {
                d->currentState = OnlineState(data.at(1));
                emit setStateFinished(queryID, true, d->currentState);
                break;
            }
            case AccountPrivate::RequestType::SetOfflineMsg:
            {
                emit setOfflineMsgFinished(queryID, true);
                break;
            }
            case AccountPrivate::RequestType::GetFriendList:
            {
                QList<AccountListEntry> idList;
                d->parseAccountList(data, "c", idList);
                emit queryFriendListFinished(queryID, idList);
                break;
            }
            case AccountPrivate::RequestType::AddFriend:
            {
                QList<AccountListEntry> idList;
                d->parseAccountList(data, "f", idList);
                emit addFriendFinished(queryID, idList.isEmpty());
                break;
            }
            case AccountPrivate::RequestType::RemoveFriend:
            {
                QList<AccountListEntry> idList;
                d->parseAccountList(data, "f", idList);
                emit removeFriendFinished(queryID, idList.isEmpty());
                break;
            }
            case AccountPrivate::RequestType::GetFriendRemarks:
            {
                QList<QString> remarkList;
                d->parseMixedList(data, "NOTE", tempList);
                for (int i=0; i<tempList.count(); i++)
                    remarkList.push_back(tempList[i]);
                emit queryFriendRemarksFinished(queryID, remarkList);
                break;
            }
            case AccountPrivate::RequestType::SetFriendRemarks:
            {
                emit setFriendRemarksFinished(queryID, true);
                break;
            }
            case AccountPrivate::RequestType::GetFriendInfo:
            {
                QList<AccountInfoEntry> accountInfoList;
                AccountInfoEntry accountInfo;
                d->parseMixedList(data, "ID", tempList);
                for (int i=0; i<tempList.count(); i++)
                {
                    accountInfo.ID = tempList[i];
                    accountInfoList.push_back(accountInfo);
                }
                d->parseMixedList(data, "MSG", tempList);
                for (int i=0; i<tempList.count(); i++)
                {
                    accountInfoList[i].offlineMsg = tempList[i];
                }
                emit queryFriendInfoFinished(queryID, accountInfoList);
                break;
            }
            default:
                emit queryError(queryID, QueryError::UnknownError);
        }
    }
    else if (errCode == RequestManager::VersionTooOld)
        emit queryError(queryID, QueryError::VersionNotSupported);
    else if (errCode == RequestManager::CannotConnect)
        emit queryError(queryID, QueryError::NetworkError);
    else
        emit queryError(queryID, QueryError::UnknownError);
}

void Account::onPrivateEvent(int eventType, int data)
{
    switch (AccountPrivate::PrivateEventType(eventType))
    {
        case AccountPrivate::RequestFinished:
            dispatchQueryRespone(data);
            break;
        default:;
    }
}

AccountPrivate::AccountPrivate(Account* parent)
{
    this->q_ptr = parent;
    connect(&server,
            SIGNAL(onRequestFinished(int)),
            this,
            SLOT(onRequestFinished(int)));
}

bool AccountPrivate::processReplyData(RequestType type, QByteArray& data)
{
    switch (type)
    {
        case RequestType::ResetSession:
        case RequestType::SetPassword:
        case RequestType::SetState:
        case RequestType::SetOfflineMsg:
        case RequestType::GetFriendList:
        case RequestType::AddFriend:
        case RequestType::RemoveFriend:
        case RequestType::GetFriendRemarks:
        case RequestType::SetFriendRemarks:
        case RequestType::GetFriendInfo:
            if (data.length() < 1 ||
                data[0] != char(WICHAT_SERVER_RESPONSE_SUCCESS))
                return false;
            break;
        default:;
    }
    return true;
}

void AccountPrivate::parseAccountList(QByteArray& data,
                                      QByteArray listType,
                                      QList<Account::AccountListEntry>& list)
{
    int p1, p2, p3, pS, pE;
    unsigned char accountState;
    Account::AccountListEntry account;
    if (listType.isEmpty())
        p1 = data.indexOf("<IDList>");
    else
        p1 = data.indexOf(QByteArray("<IDList t=")
                          .append(listType)
                          .append(">"));
    pE = data.indexOf("</IDList>");
    if (p1 >= 0 && pE > 16)
    {
        p3 = p2 = p1;
        while (true)
        {
            p1 = data.indexOf("<ID", p1 + 1);
            p2 = data.indexOf(">", p1 + 1);
            p3 = data.indexOf("</ID>", p2 + 1);
            if (p1 < 0 || p1 < 0 || p1 < 0 || p1 > pE)
                break;

            account.ID = QString(data.mid(p2 + 1, p3 - p2 - 1)).trimmed();
            pS = data.indexOf("s=", p1);
            if (pS + 2 < p2)
                accountState = data.at(pS + 2) - '0';
            else
                accountState = 0;
            account.state = Account::OnlineState(accountState);
            list.append(account);
        }
    }
}

void AccountPrivate::parseMixedList(QByteArray& data,
                                    QByteArray fieldName,
                                    QList<QByteArray>& list)
{
    int p1, p2, pE, length;
    QByteArray fieldBegin, fieldEnd;
    p1 = data.indexOf("<MList>");
    pE = data.indexOf("</MList>");
    fieldBegin.append('<').append(fieldName).append('>');
    fieldEnd.append("</").append(fieldName).append('>');
    list.clear();
    if (p1 >= 0 && pE > 13)
    {
        p2 = p1;
        while (true)
        {
            p1 = data.indexOf(fieldBegin, p1 + 1);
            p2 = data.indexOf(fieldEnd, p2 + 1);
            if (p1 < 0 || p2 < 0)
                break;

            length = p2 - p1 - fieldBegin.length();
            if (length > 0)
                list.append(data.mid(p1 + fieldBegin.length(), length));
            else
                list.append(QByteArray());
        }
    }
}

QByteArray AccountPrivate::formatID(QString ID)
{
    Q_Q(Account);
    return ID.leftJustified(q->MaxIDLen, '\0', true).toLatin1();
}

Account::OnlineState AccountPrivate::intToOnlineState(int var)
{
    switch (var)
    {
        case WICHAT_ACCOUNT_STATE_ONLINE:
            return Account::OnlineState::Online;
        case WICHAT_ACCOUNT_STATE_OFFLINE:
            return Account::OnlineState::Offline;
        case WICHAT_ACCOUNT_STATE_BUSY:
            return Account::OnlineState::Busy;
        case WICHAT_ACCOUNT_STATE_HIDE:
            return Account::OnlineState::Hide;
        default:
            return Account::OnlineState::None;
    }
}

QString AccountPrivate::serverObjectToPath(ServerObject objectID)
{
    switch (objectID)
    {
        case ServerObject::AccountLogin:
            return WICHAT_SERVER_PATH_ACCOUNT_LOGIN;
            break;
        case ServerObject::AccountAction:
        case ServerObject::FriendAction:
            return WICHAT_SERVER_PATH_ACCOUNT_ACTION;
            break;
        default:
            return "";
    }
}

void AccountPrivate::onRequestFinished(int requestID)
{
    emit privateEvent(RequestFinished, requestID);
}