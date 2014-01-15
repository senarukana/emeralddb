
#ifndef PMD_EDU_MGR_HPP__
#define PMD_EDU_MGR_HPP__

#include "core.hpp"
#include "pmdEDU.hpp"
#include "ossLatch.hpp"

#define EDU_SYSTEM      0x01
#define EDU_USER        0x02
#define EDU_ALL         (EDU_USER | EDU_SYSTEM)

class pmdEDUMgr {
private:
    std::map<EDUID, pmdEDUCB*> _runQueue;
    std::map<EDUID, pmdEDUCB*> _idleQueue;
    std::map<unsigned int, EDUID> _tid_eduid_map;

    ossSLatch _mutex;
    // incremental-only EDU id
    EDUID _EDUID;
    // list of system EDUs
    std::map<unsigned int, EDUID> _mapSystemEDUS;
    //no new requests are allowed
    bool _isQuiesced;
    bool _isDestroyed;
public:
    pmdEDUMgr():
    _EDUID(1),
    _isQuiesced(false),
    _isDestroyed(false) {}

    ~pmdEDUMgr() {
        reset();
    }
    void reset() {
        _destroyAll();
    }

    unsigned int size() {
        unsigned int num = 0;
        _mutex.get_shared();
        num = (unsigned int)_runQueue.size() + (unsigned int)_idleQueue.size();
        _mutex.release_shared();
        return num;
    }

    unsigned int sizeRun() {
        unsigned int num = 0;
        _mutex.get_shared();
        num = (unsigned int)_runQueue.size();
        _mutex.release_shared();
        return num;
    }

    unsigned int sizeIdle (){
        unsigned int num = 0;
        _mutex.get_shared();
        num = (unsigned int) _idleQueue.size();
        _mutex.release_shared();
        return num ;
    }

    bool isSystemEDU(EDUID eduID) {
        bool isSys = false;
        _mutex.get_shared();
        isSys = _isSystemEDU(eduID);
        _mutex.release_shared();
        return isSys;
    }

    bool isQuiesced () {
        return _isQuiesced ;
    }

    void setQuiesced ( bool b ) {
        _isQuiesced = b ;
    }

    bool isDestroyed () {
        return _isDestroyed ;
    }

    static bool isPoolable (EDU_TYPES type) {
        return (EDU_TYPE_AGENT == type) ;
    }

private:
    int _createNewEDU(EDU_TYPES type, void *arg, EDUID *eduid);
    int _destoryEDU(EDUID eduID);
    int _deactiveEDU(EDUID eduID);
    int _destroyAll();
    int _forceEDUs(int property = EDU_ALL);
    unsigned int _getEDUCount(int property = EDU_ALL);
    void _setDestroyed(bool b) {
        _isDestroyed = b;
    }
    bool _isSystemEDU ( EDUID eduID ){
        std::map<unsigned int, EDUID>::iterator it = _mapSystemEDUS.begin() ;
        while ( it != _mapSystemEDUS.end() )
        {
            if ( eduID == it->second )
            {
                return true ;
            }
            ++it ;
        }
        return false ;
   }
public:
    // move an waitting/creating EDU to running status
    int activateEDU(EDUID eduID);
    // move an running EDU to waitting status
    int waitEDU(EDUID eduID);
    // start an edu from the idle queue or create a new one
    int startEDU(EDU_TYPES type, void *arg, EDUID *eduid);
    // move an waitting/creating EDU to idle status or destory this edu
    int returnEDU(EDUID eduID, bool force, bool* destroyed);
    // post a message to EDU
    int postEDUPost(EDUID eduID, pmdEDUEventTypes type,
                    bool release = false, void *pData = NULL);

    int waitEDUPost(EDUID eduID, pmdEDUEvent& event,
                    long long millsecond);

    int forceUserEDU(EDUID eduID);

    // get EDUCB by threadID
    pmdEDUCB *getEDU(unsigned int tid);
    // get EDUCB by eduID
    pmdEDUCB *getEDU(EDUID eduID);
    // get EDUCB for the current thread
    pmdEDUCB *getEDU();

};



#endif