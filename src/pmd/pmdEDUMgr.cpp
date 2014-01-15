/*******************************************************************************
   Copyright (C) 2013 SequoiaDB Software Inc.

   This program is free software: you can redistribute it and/or modify
   it under the terms of the GNU Affero General Public License, version 3,
   as published by the Free Software Foundation.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
   GNU Affero General Public License for more details.

   You should have received a copy of the GNU Affero General Public License
   along with this program. If not, see <http://www.gnu.org/license/>.
*******************************************************************************/
#include "core.hpp"
#include "pmd.hpp"
#include "pd.hpp"
#include "pmdEDU.hpp"
#include "pmdEDUMgr.hpp"
#include "ossUtil.hpp"

using namespace std;

int pmdEDUMgr::_destroyAll() {
    _setDestroyed(true);
    setQuiesced(true);

    //stop all user edus
    unsigned int timeCounter = 0;
    unsigned int eduCount = _getEDUCount(EDU_USER);
    while (eduCount != 0) {
        if (timeCounter % 50 == 0) {
            _forceEDUs(EDU_USER);
        }
        ++timeCounter;
        ossSleepMillis(100);
        eduCount = _getEDUCount(EDU_USER);
    }

    // stop all system edus
    timeCounter = 0;
    eduCount = _getEDUCount(EDU_SYSTEM);
    while (eduCount != 0) {
        if (timeCounter % 50 == 0) {
            _forceEDUs(EDU_SYSTEM);
        }
        ++timeCounter;
        ossSleepMillis(100);
        eduCount = _getEDUCount(EDU_SYSTEM);
    }
    return EDB_OK;
}

unsigned int pmdEDUMgr::_getEDUCount(int property) {
    unsigned int eduCount = 0;
    map<EDUID, pmdEDUCB*>::iterator it;
    /**************** CRITICAL SECTION********************/
    _mutex.get_shared();
    for (it = _runQueue.begin(); it != _runQueue.end(); ++it) {
        if (((EDU_SYSTEM & property) && _isSystemEDU( it->first))
            || ((EDU_USER & property) && !_isSystemEDU( it->first))) {
            eduCount++;
        }
    }

    for (it = _idleQueue.begin(); it != _idleQueue.end(); it++) {
        if (((EDU_SYSTEM & property) && _isSystemEDU( it->first))
            || ((EDU_USER & property) && !_isSystemEDU( it->first))) {
            eduCount++;
        }
    }
    _mutex.release_shared();
    /*************** END CRITICAL SECTION **********************/
    return eduCount;
}

// block all new request and attemps to terminate existing requests
int pmdEDUMgr::_forceEDUs(int property) {
    map<EDUID, pmdEDUCB*>::iterator it;

    /*********CRITICAL SECTION********************/
    _mutex.get();
    for (it = _runQueue.begin(); it != _runQueue.end(); ++it) {
       if (((EDU_SYSTEM & property) && _isSystemEDU( it->first))
            || ((EDU_USER & property) && !_isSystemEDU( it->first))) {
            (*it).second->force() ;
            PD_LOG (PDTRACE, "force edu[ID:%lld]", it->first ) ;
        }
    }

    for (it = _idleQueue.begin(); it != _idleQueue.end(); it++) {
         if (((EDU_SYSTEM & property) && _isSystemEDU( it->first))
            || ((EDU_USER & property) && !_isSystemEDU( it->first))) {
            (*it).second->force() ;
            PD_LOG (PDTRACE, "force edu[ID:%lld]", it->first ) ;
        }
    }
    _mutex.release();
    /*************** END CRITICAL SECTION **********************/
    return EDB_OK;
}

int pmdEDUMgr::startEDU(EDU_TYPES type, void *arg, EDUID *eduid) {
    int rc = EDB_OK;
    EDUID eduID = 0;
    pmdEDUCB *cb;
    map<EDUID, pmdEDUCB*>::iterator it = _idleQueue.begin();

    if (isQuiesced()) {
        rc = EDB_QUIESCED;
        goto done;
    }
     /****************** CRITICAL SECTION **********************/
    // get exclusive latch, we don't latch the entire function
    // in order to avoid creating new thread while holding latch
    _mutex.get();
    if (_idleQueue.empty() || !isPoolable(type)) {
        // note that EDU types other than "agent" shouldn't be pooled at all
        // release latch before calling _createNewEDU
        _mutex.release();
        rc = _createNewEDU(type, arg, eduid);
        if (rc == EDB_OK) {
            goto done;
        }
        goto error;
    }

    // get EDU from IdleQueue
    eduID = (*it).first;
    cb = (*it).second;
    _idleQueue.erase(eduID);
    cb->setType(type);
    cb->setStatus(PMD_EDU_WAITTING);
    _runQueue[eduID] = cb;
    *eduid = eduID;
    _mutex.release();
    /*************** END CRITICAL SECTION **********************/
    // the edu is start, need post a resume event
    cb->postEvent(pmdEDUEvent(PMD_EDU_EVENT_RESUME, false, arg));

done:
    return rc;
error:
    goto done;
}


// move the edu from _idlequeue to runqueue and set the status
int pmdEDUMgr::activateEDU(EDUID eduID) {
    int rc = EDB_OK;
    pmdEDUCB *cb;
    map<EDUID, pmdEDUCB*>::iterator it;
    // map<unsigned int, EDUID>::iterator tidIt;

    /***************CRITICAL SECTION*********************/
    _mutex.get();
    if (_idleQueue.end() == (it = _idleQueue.find(eduID))) {
        if (_runQueue.end() == (it = _runQueue.find(eduID))) {
            rc = EDB_SYS;
            PD_LOG(PDERROR, "not found edu %lld in RunQueue and IdleQueue", eduID);
            goto error;
        }
        cb = (*it).second;
        cb->setStatus(PMD_EDU_RUNNING);
        goto done;
    }

    cb = (*it).second;
    _idleQueue.erase(eduID);
    _runQueue[eduID] = cb;
    cb->setStatus(PMD_EDU_RUNNING);
done:
    _mutex.release();
    return rc;
error:
    goto done;
}

int pmdEDUMgr::_createNewEDU(EDU_TYPES type, void *arg, EDUID *eduid) {
    int rc = EDB_OK;
    pmdEDUCB *cb;
    EDUID id;

    if (isQuiesced()) {
        rc = EDB_QUIESCED;
        goto done;
    }

    cb = new(nothrow)pmdEDUCB(this, type);
    if (!cb) {
        rc = EDB_OOM;
        PD_LOG(PDERROR, "Out of memory to create agent control block");
        goto error;
    }
    cb->setStatus(PMD_EDU_CREATING);

    /***************CRITICAL SECTION************************/
    _mutex.get();

    cb->setID(_EDUID);
    if (eduid) {
        *eduid = _EDUID;
    }
    _runQueue[_EDUID] = cb;
    id = _EDUID;
    _EDUID++;
    try {
        boost::thread agentThread(pmdEDUEntryPoint,
                        type, cb, arg);
        // detach the agent so that he's all on his own
        agentThread.detach();
    } catch(exception e) {
        _runQueue.erase(id);
        rc = EDB_SYS;
        PD_LOG ( PDERROR,"Failed to create new agent, rc = %d", rc);
        if (cb) {
            delete cb;
        }
        goto error;
    }
    _mutex.release();
    /*************End CRITICAL SECTION*********************/
    // the edu is start, need post a resume event
    cb->postEvent(pmdEDUEvent(PMD_EDU_EVENT_RESUME, false, arg));
done:
    return rc;
error:
    goto done;
}

// EMUMgr should decide whether put the EDU to pool or destroy it
int pmdEDUMgr::returnEDU(EDUID eduID, bool force, bool* destroyed ) {
    int rc = EDB_OK;
    pmdEDUCB *cb;
    EDU_TYPES type;
    map<EDUID, pmdEDUCB*>::iterator it;

    /**************CRITICAL SECTION**********************/
    _mutex.get_shared();
    if (_runQueue.end() == (it = _runQueue.find(eduID))) {
        if(_idleQueue.end() == (it = _idleQueue.find(eduID))) {
            rc = EDB_SYS;
            if (destroyed) {
                *destroyed = false;
                _mutex.release_shared();
                goto error;
            }
        }
    }
    cb = (*it).second;
    type = cb->getType();
    _mutex.release_shared();
    /**************END OF CRITICAL SECTION**********************/
    if (!isPoolable(type) || force || isDestroyed() || 
        (unsigned int)pmdGetKRCB()->getMaxPool() > size()) {
        rc = _destoryEDU(eduID);
        if (destroyed) {
            if (rc == EDB_OK || rc == EDB_SYS) {
                *destroyed = true;
            }else {
                *destroyed = false;
            }
        }
    } else {
        rc = _deactiveEDU(eduID);
        if (destroyed) {
            if (rc == EDB_OK || rc == EDB_SYS) {
                *destroyed = true;
            } else {
                *destroyed = false;
            }
        }
    }
done:
    return rc;
error:
    goto done;
}

// remove it from the queue, and free the pmdCB
int pmdEDUMgr::_destoryEDU(EDUID eduID) {
    int rc = EDB_OK;
    pmdEDUCB *cb;
    map<EDUID, pmdEDUCB*>::iterator it;
    map<unsigned int, EDUID>::iterator tidIt;

    /***************CRITICAL SECTION*********************/
    _mutex.get();
    if(_runQueue.end() == (it = _runQueue.find(eduID))) {
        if(_idleQueue.end() == (it = _idleQueue.find(eduID))) {
            rc = EDB_SYS;
            PD_LOG(PDERROR, "not found edu %lld in RunQueue and IdleQueue", eduID);
            goto error;
        }
        cb = (*it).second;

        cb->setStatus(PMD_EDU_DESTROY);
        _idleQueue.erase(eduID);
    } else {
        cb = (*it).second;
        cb->setStatus(PMD_EDU_DESTROY);
        _runQueue.erase(eduID);
    }

    // clean up tid/eduid map
    for (tidIt = _tid_eduid_map.begin(); tidIt != _tid_eduid_map.end(); tidIt++) {
        if ((*tidIt).second == eduID) {
            _tid_eduid_map.erase(tidIt);
            break;
        }
    }
    if (cb) {
        delete(cb);
    }
done:
    _mutex.release();
    return rc;
error:
    goto done;
}


// return the edu to the pool and set the edu status to waitting
int pmdEDUMgr::_deactiveEDU(EDUID eduID) {
    int rc = EDB_OK;
    pmdEDUCB *cb;
    map<EDUID, pmdEDUCB*>::iterator it;

    /**************CRITICAL SECTION**************/
    _mutex.get();
    if(_runQueue.end() == (it = _runQueue.find(eduID))) {
        if(_idleQueue.end() == (it = _idleQueue.find(eduID))) {
            rc = EDB_SYS;
            PD_LOG(PDERROR, "not found edu %lld in RunQueue and IdleQueue", eduID);
            goto error;
        }
        cb = (*it).second;
        cb->setStatus(PMD_EDU_WAITTING);
    } else {
        // move the edu from runqueue to idlequeue
        cb = (*it).second;
        _runQueue.erase(eduID);
         cb->setStatus(PMD_EDU_WAITTING);
         _idleQueue[eduID] = cb;
    }
done:
    _mutex.release();
    return rc;
error:
    goto done;
}

// change edu status from running to waiting
int pmdEDUMgr::waitEDU(EDUID id) {
    int rc = EDB_OK;
    pmdEDUCB *cb;
    unsigned int eduStatus = PMD_EDU_CREATING;
    map<EDUID, pmdEDUCB*>::iterator it;

    /**************CRITICAL SECTION**************/
    _mutex.get();
    if (_runQueue.end() == (it = _runQueue.find(id))) {
        // can't find EDU in run queue
        rc = EDB_SYS;
        goto error;
    }
    cb = (*it).second;

    eduStatus = cb->getStatus();

    // if it's already waitting, let's do nothing
    if (PMD_IS_EDU_WAITTING(eduStatus)) {
        goto done;
    }
    if (!PMD_IS_EDU_RUNNING(eduStatus)) {
        rc = EDB_EDU_INVAL_STATUS;
        goto error;
    }
    cb->setStatus(PMD_EDU_WAITTING);
done :
    _mutex.release() ;
    /************* END CRITICAL SECTION **************/
    return rc ;
error :
    goto done ;

}

// post an event to EDU
int pmdEDUMgr::postEDUPost(EDUID eduID, pmdEDUEventTypes type,
                    bool release, void *pData) {
    int rc = EDB_OK;
    pmdEDUCB *cb;
    map<EDUID, pmdEDUCB*>::iterator it;

    /**************CRITICAL SECTION**************/
    _mutex.get();
    // find it from the queue
    if(_runQueue.end() == (it = _runQueue.find(eduID))) {
        if(_idleQueue.end() == (it = _idleQueue.find(eduID))) {
            rc = EDB_SYS;
            PD_LOG(PDERROR, "not found edu %lld in RunQueue and IdleQueue", eduID);
            goto error;
        }
    }
    cb = (*it).second;
    cb->postEvent(pmdEDUEvent(type, release, pData));
done:
    _mutex.release();
    return rc;
error:
    goto done;
}

// wait event
int pmdEDUMgr::waitEDUPost(EDUID eduID, pmdEDUEvent& event,
                             long long millsecond) {
    int rc = EDB_OK ;
    pmdEDUCB* cb = NULL ;
    map<EDUID, pmdEDUCB*>::iterator it ;
    /**************CRITICAL SECTION**************/
    _mutex.get_shared () ;
    if ( _runQueue.end () == (it = _runQueue.find ( eduID ))){
        // if we cannot find it in runqueue, we search for idle queue
        // note that during the time, we already have EDUMgr locked,
        // so thread cannot change queue from idle to run
        // that means we are safe to exame both queues
        if ( _idleQueue.end () == ( it = _idleQueue.find ( eduID )) ){
            // we can't find edu id anywhere
            rc = EDB_SYS ;
            PD_LOG(PDERROR,"not found edu %lld in RunQueue and IdleQueue", eduID);
            goto error ;
        }
    }
    cb = ( *it ).second ;
    // wait for event. when millsecond is 0, it should always return true
    if ( !cb->waitEvent( event, millsecond )){
        rc = EDB_TIMEOUT ;
        goto error ;
    }
done:
    _mutex.release_shared () ;
    return rc ;
error:
    goto done ;
}