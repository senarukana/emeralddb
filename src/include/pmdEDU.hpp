
#ifndef PMDEDU_HPP__
#define PMDEDU_HPP__

#include "core.hpp"
#include "pmdEDUEvent.hpp"
#include "ossQueue.hpp"
#include "ossSocket.hpp"

#define PMD_INVALID_EDUID 0
#define PMD_IS_EDU_CREATING(x)          (PMD_EDU_CREATING == x )
#define PMD_IS_EDU_RUNNING(x)           (PMD_EDU_RUNNING == x)
#define PMD_IS_EDU_WAITTING(x)          (PMD_EDU_WAITTING == x)
#define PMD_IS_EDU_IDLE(x)              (PMD_EDU_IDLE == x)
#define PMD_IS_EDU_DESTORY(x)           (PMD_EDU_DESTROY == x)

#define EDU_MAX_IDLE_TIME       1000

typedef unsigned long long EDUID;

enum EDU_TYPES {
    EDU_TYPE_TCPLISTENER = 0,
    EDU_TYPE_AGENT,

    EDU_TYPE_UNKNOWN,
    EDU_TYPE_MAXIMUM = EDU_TYPE_UNKNOWN
};

enum EDU_STATUS {
    PMD_EDU_CREATING,
    PMD_EDU_RUNNING,
    PMD_EDU_WAITTING,
    PMD_EDU_IDLE,
    PMD_EDU_DESTROY,
    PMD_EDU_UNKNOWN,
    PMD_EDU_STATUS_MAXIMUM = PMD_EDU_UNKNOWN
};

class pmdEDUMgr;

class pmdEDUCB {
private:
    EDU_TYPES   _type;
    pmdEDUMgr   *_mgr;
    EDU_STATUS  _status;
    EDUID       _id;
    bool        _isForced;
    bool        _isDisconnected;
    int         _maxIdleTime; //ms
    ossQueue<pmdEDUEvent> _queue;  
public:
    pmdEDUCB(pmdEDUMgr *mgr, EDU_TYPES type):
    _type(type),
    _mgr(mgr),
    _status(PMD_EDU_CREATING),
    _id(0),
    _isForced(false),
    _isDisconnected(false),
    _maxIdleTime(EDU_MAX_IDLE_TIME)
    {
    }

    inline EDUID getID() { return _id; }
    inline void postEvent( pmdEDUEvent const &data) {
        _queue.push(data);
    }
    bool waitEvent(pmdEDUEvent &data, long long millsec) {
        bool waitMsg = false;
        if (_status != PMD_EDU_IDLE) {
            _status = PMD_EDU_WAITTING;
        }
        if (millsec <= 0) {
            _queue.wait_and_pop(data);
            waitMsg = true;
        } else {
            waitMsg = _queue.timed_wait_and_pop(data, millsec);
        }

        if (waitMsg) {
            if (data._eventType == PMD_EDU_EVENT_TERM) {
                _isDisconnected = true;
            } else {
                _status = PMD_EDU_RUNNING;
            }
        }
        return waitMsg;
    }
    inline void force() {
        _isForced = true;
    }
    inline void disconnect() {
        _isDisconnected = true;
    }
    inline EDU_TYPES getType() {
        return _type;
    }
    inline EDU_STATUS getStatus() {
        return _status;
    }
    inline pmdEDUMgr *getEDUMgr() {
        return _mgr;
    }
    inline void setType(EDU_TYPES type) {
        _type = type;
    }
    inline void setID(EDUID id) {
        _id = id;
    }
    inline void setStatus(EDU_STATUS status) {
        _status = status;
    }
    inline bool isForced() {
        return _isForced;
    }
    inline int maxIdleTime() {
        return _maxIdleTime;
    }     
}; 

typedef int (*pmdEntryPoint) (pmdEDUCB *, void *);
pmdEntryPoint getEntryFuncByType(EDU_TYPES type);

void initEDUFunctionMap();

int pmdAgentEntryPoint(pmdEDUCB *cb, void *arg);
int pmdTcpListenerEntryPoint(pmdEDUCB *cb, void *arg);
int pmdEDUEntryPoint(EDU_TYPES type, pmdEDUCB *cb, void *arg);

int pmdRecv(char *pBuffer, int recvSize,
              ossSocket *sock, pmdEDUCB *cb );
int pmdSend(char *pBuffer, int sendSize, ossSocket *sock, pmdEDUCB *cb);

#endif