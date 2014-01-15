

#include "core.hpp"
#include "pmdEDU.hpp"
#include "pmdEDUEvent.hpp"
#include "pmdEDUMgr.hpp"
#include "pmd.hpp"
#include "pd.hpp"

using namespace std;

static map<EDU_TYPES, string> mapEDUName;
static map<EDU_TYPES, EDU_TYPES> mapEDUTypeSys;
static map<EDU_TYPES, pmdEntryPoint> mapEDUFunction;

int registerEDUName(EDU_TYPES type, const char *name, bool system) {
    int rc = EDB_OK;
    map<EDU_TYPES, string>::iterator it = mapEDUName.find(type);
    if (it != mapEDUName.end()) {
        PD_LOG(PDERROR, "EDU Type conflict[type:%d, %s<->%s]", 
            (int)type, it->second.c_str(), name);
        rc = EDB_SYS;
        goto error;
    }
    mapEDUName[type] = string(name);
    if (system) {
        mapEDUTypeSys[type] = type;
    }
done:
    return rc;
error:
    goto done;
}


bool isSystemEDU (EDU_TYPES type) {
    map<EDU_TYPES,EDU_TYPES>::iterator it = mapEDUTypeSys.find(type) ;
    return it == mapEDUTypeSys.end()?false:true ;
}

const char *getEDUName (EDU_TYPES type){
    map<EDU_TYPES, string>::iterator it =
         mapEDUName.find ( type ) ;
    if (it != mapEDUName.end()) {
        return it->second.c_str() ;
    }
    return "Unknown";
}


void initEDUFunctionMap() {
    registerEDUName(EDU_TYPE_TCPLISTENER, "TcpListener", false);
    mapEDUFunction[EDU_TYPE_AGENT] = pmdAgentEntryPoint;
    mapEDUFunction[EDU_TYPE_TCPLISTENER] = pmdTcpListenerEntryPoint;
}

pmdEntryPoint getEntryFuncByType(EDU_TYPES type) {
    map<EDU_TYPES, pmdEntryPoint>::iterator it = mapEDUFunction.find(type);
    if (it == mapEDUFunction.end()) {
        PD_LOG(PDERROR, "EDUType %d doesn't existed in mapEDUFunction", (int)type);
        return NULL;
    } else {
        return (*it).second;
    }
}

int pmdRecv(char *pBuffer, int recvSize, ossSocket *sock, pmdEDUCB *cb) {
    // if (cb->isForced()) {
    //     return EDB_APP_FORCED;
    // }
    // return sock->recv(pBuffer, recvSize);
    int rc = EDB_OK;
    while (true) {
        if (cb->isForced()) {
            rc = EDB_APP_FORCED;
            goto done;
        }
        rc = sock->recv(pBuffer, recvSize);
        if (rc == EDB_TIMEOUT) {
            continue;
        }
        goto done;
    }
done:
    return rc;
}

int pmdSend(char *pBuffer, int sendSize, ossSocket *sock, pmdEDUCB *cb) {
    // if (cb->isForced()) {
    //     return EDB_APP_FORCED;
    // }
    // return sock->send(pBuffer, sendSize);
    int rc = EDB_OK;
    while (true) {
        if (cb->isForced()) {
            rc = EDB_APP_FORCED;
            goto done;
        }
        rc = sock->send(pBuffer, sendSize);
        if (rc == EDB_TIMEOUT) {
            continue;
        }
        goto done;
    }
done:
    return rc;
}

int pmdEDUEntryPoint(EDU_TYPES type, pmdEDUCB *cb, void *arg) {
    int rc = EDB_OK;
    pmdEntryPoint entryFunc;
    pmdEDUEvent event;
    EDUID id = cb->getID();
    pmdEDUMgr *mgr = cb->getEDUMgr();
    bool eduDestroyed = false;
    bool force = false;
    bool hasMsg = false;

    while (!eduDestroyed) {
        if ((hasMsg = cb->waitEvent(event, cb->maxIdleTime())) == false) {
            if (cb->isForced()) {
                 PD_LOG (PDEVENT, "EDU %lld is forced", id); 
                 force = true;
            } else {
                if (isSystemEDU(type)) {
                    continue;
                } else {
                    // cb has waited for a long time, deactivate it
                    PD_LOG(PDEVENT,"EDU %lld, type %d is idle for enough time", id, type);
                }
            }
        }
        if (hasMsg) {
            if (!force && event._eventType == PMD_EDU_EVENT_RESUME) {
                // resume from the waiting idle
                // set the status to wait
                mgr->waitEDU(id);
                // find the main function
                if ((entryFunc = getEntryFuncByType(type)) == NULL) {
                    PD_LOG(PDCRITICAL, "EDU %lld type %d entry point is NULL",
                            id, type);
                    EDB_SHUTDOWN_DB
                    rc = EDB_SYS;
                }
                // run the main function
                rc = entryFunc(cb, event._Data);
                // sanity check
                if (EDB_IS_DB_UP) {
                    if (isSystemEDU(type)) {
                        PD_LOG(PDCRITICAL, "System EDU: %lld, type %s exits with %d",
                            id, getEDUName(type), rc);
                        EDB_SHUTDOWN_DB
                    } else if(rc) {
                        PD_LOG(PDWARNING, "EDU %lld, type %s exits with %d",
                            id, getEDUName(type), rc);
                    }
                }
                // set the status to wait
                mgr->waitEDU(id);
            } else if(!force && event._eventType == PMD_EDU_EVENT_TERM) {
                PD_LOG(PDEVENT, "EDU %lld, type %s is forced", id, getEDUName(type));
                force = true;
            } else if (!force) {
                // neither RESUME nor TERM, shouldn't happen
                PD_LOG(PDERROR, "Receive the wrong event %d in EDU %lld,  type %s", id, getEDUName(type));
                force = true;
                rc = EDB_SYS; 
            }
        }

        // release the event data if necessary
        if (event._Data && event._release) {
            free(event._Data);
            event.reset();
        }

        rc = mgr->returnEDU(id, force, &eduDestroyed);
        if (rc) {
            PD_LOG(PDERROR, "Invalid EDU Status for EDU %lld, type %s is forced", id, getEDUName(type));
        }
    }
    return 0;

}