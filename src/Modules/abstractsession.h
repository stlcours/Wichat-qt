#ifndef AbstractSession_H
#define AbstractSession_H

#include <QtGlobal>


class AbstractSessionPrivate;

class AbstractSession
{
    Q_DECLARE_PRIVATE(AbstractSession)
protected:
    AbstractSessionPrivate* d_ptr;

public:
    struct AbstractSessionData{};

    explicit AbstractSession();
    AbstractSession(AbstractSessionPrivate* d);
    ~AbstractSession();
    virtual int count() = 0;
    virtual bool exists(QString sessionID) = 0;
    virtual void add(QString sessionID) = 0;
    virtual void remove(QString sessionID) = 0;
    virtual void removeAll() = 0;
    virtual const AbstractSessionData& getSession(QString sessionID) = 0;
};

#endif // AbstractSession_H
