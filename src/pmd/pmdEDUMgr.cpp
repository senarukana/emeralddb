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
#include "pmdEDUCB.hpp"
#include "pmdEDUMgr.hpp"


int pmdEDUMgr::destroyAll() {
    _setDestroyed(true);
    setQuiesced(true);

    //stop all user edus
    unsigned int timeCounter = 0;
    unsigned int eduCount = _getEDUCount(EDU_USER);
    while (eduCount != 0) {

    }
}

int pmdEDUMgr::startEDU(EDU_TYPES type, void *arg, EDUID *eduid) {
    int rc = EDB_OK;
    EDUID eduID = 0;
    pmdEDUCB *eduCB;
    std::map<EDUID, pmdEDUCB*>::iterator it = _idleQueue.begin();

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
    eduCB = (*it).second;
    _idleQueue.erase(eduID);
    eduCB->setType(type);
    eduCB->setStatus(PMD_EDU_WAITING);
    _runQueue[eduID] = eduCB;
    *eduid = eduID;
    _mutex.release();
    /*************** END CRITICAL SECTION **********************/
    // the edu is start, need post a resume event
    eduCB->postEvent(pmdEDUEvent(PMD_EDU_EVENT_RESUME, false, arg));

done:
    return rc;
error:
    goto done;
}


// move the edu from _idlequeue to runqueue and set the status
int pmdEDUMgr::activateEDU(EDUID eduID) {
    int rc = EDB_OK;
    EDUCB *cb;
    EDU_TYPES type;
    map<EDUID, *pmdEDUCB>::iterator it;
    map<unsigned int, EDUID>::iterator tidIt;

    /***************CRITICAL SECTION*********************/
    _mutex.get();
    if (_idleQueue.end() == (it = _idleQueue.find(eduID))) {
        if (_runQueue.end() == (it = _runQueue.find(eduID))) {
            rc = EDB_SYS;
            PD_LOG("not found edu %lld in RunQueue and IdleQueue", eduID);
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
    EDUCB *cb;
    EDUID id;

    if (isQuiesced()) {
        rc = EDB_QUIESCED;
        goto done;
    }

    cb = new(std::nothrow)pmdEDUCB(this, type);
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
        boost:thread agentThread(pmdEDUEntryPoint,
                        type, cb, arg);
        // detach the agent so that he's all on his own
        agentThread.detach();
    } catch(std::exception e) {
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
    eduCB->postEvent(pmdEDUEvent(PMD_EDU_EVENT_RESUME, false, arg));
done:
    return rc;
error:
    goto done;
}

// EMUMgr should decide whether put the EDU to pool or destroy it
int pmdEDUMgr::returnEDU(EDUID eduID, bool force, bool* destroyed ) {
    int rc = EDB_OK;
    EDUCB *educb;
    EDU_TYPES type;
    map<EDUID, *pmdEDUCB>::iterator it;

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
    edbcb = (*it).second;
    type = educb->getType();
    _mutex.release_shared();
    if (!isPoolable(type) || force || 
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
    EDUCB *cb;
    EDU_TYPES type;
    map<EDUID, *pmdEDUCB>::iterator it;
    map<unsigned int, EDUID>::iterator tidIt;

    /***************CRITICAL SECTION*********************/
    _mutex.get();
    if(_runQueue.end() == (it = _runQueue.find(eduID))) {
        if(_idleQueue.end() == (it = _idleQueue.find(eduID))) {
            rc = EDB_SYS;
            PD_LOG("not found edu %lld in RunQueue and IdleQueue", eduID);
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
    if (eduCB) {
        delete(eduCB);
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
    EDUCB *cb;
    EDU_TYPES type;
    map<EDUID, *pmdEDUCB>::iterator it;

    /**************CRITICAL SECTION**************/
    _mutex.get();
    if(_runQueue.end() == (it = _runQueue.find(eduID))) {
        if(_idleQueue.end() == (it = _idleQueue.find(eduID))) {
            rc = EDB_SYS;
            PD_LOG("not found edu %lld in RunQueue and IdleQueue", eduID);
            goto error;
        }
        cb = (*it).second;
        cb->setStatus(PMD_EDU_WAITING);
    } else {
        // move the edu from runqueue to idlequeue
        cb = (*it).second;
        _runQueue.erase(eduID);
         cb->setStatus(PMD_EDU_WAITING);
         _idleQueue[eduID] = cb;
    }
done:
    _mutex.release();
    return rc;
error:
    goto done;
}

// post an event to EDU
int pmdEDUMgr::postEDUPost(EDUID eduID, pmdEDUEventTypes type,
                    bool release, void *pData) {
    int rc = EDB_OK;
    EDUCB *cb;
    pmdEDUEvent *event;
    map<EDUID, *pmdEDUCB>::iterator it;

    /**************CRITICAL SECTION**************/
    _mutex.get();
    // find it from the queue
    if(_runQueue.end() == (it = _runQueue.find(eduID))) {
        if(_idleQueue.end() == (it = _idleQueue.find(eduID))) {
            rc = EDB_SYS;
            PD_LOG("not found edu %lld in RunQueue and IdleQueue", eduID);
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
    pmdEDUCB* eduCB = NULL ;
    std::map<EDUID, pmdEDUCB*>::iterator it ;
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
            PD_LOG("not found edu %lld in RunQueue and IdleQueue", eduID);
            goto error ;
        }
    }
    eduCB = ( *it ).second ;
    // wait for event. when millsecond is 0, it should always return true
    if ( !eduCB->waitEvent( event, millsecond )){
        rc = EDB_TIMEOUT ;
        goto error ;
    }
done:
    _mutex.release_shared () ;
    return rc ;
error:
    goto done ;
}