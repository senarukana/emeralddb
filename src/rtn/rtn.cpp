
#include "core.hpp"
#include "pmd.hpp"
#include "rtn.hpp"
#include "ixmBucket.hpp"
#include "pd.hpp"

using namespace bson;

rtn::rtn():_dmsFile(NULL), _ixmBucketMgr(NULL) {
}

rtn::~rtn() {
    if (_dmsFile) {
        delete(_dmsFile);
    }
    if (_ixmBucketMgr) {
        delete(_ixmBucketMgr);
    }
}

int rtn::init() {
    int rc = EDB_OK;
    // init index
    _ixmBucketMgr = new(std::nothrow)ixmBucketManager();
    if (_ixmBucketMgr == nullptr) {
        rc = EDB_OOM;
        PD_LOG(PDERROR, "Failed to new index");
    }
    rc = _ixmBucketMgr->init();
    if (rc) {
         PD_LOG(PDERROR, "Failed to init ixmManager, rc = %d", rc);
        goto error;
    }
    _dmsFile = new(std::nothrow)dmsFile(_ixmBucketMgr);
    if (_dmsFile == nullptr) {
        rc = EDB_OOM;
        PD_LOG(PDERROR, "Failed to new dms file");
        goto error;
    }
    // init dms
    rc = _dmsFile->init(pmdGetKRCB()->getDataFilePath());
    if (rc) {
        PD_LOG(PDERROR, "Failed to init dms, rc = %d", rc);
        goto error;
    }
    PD_LOG(PDERROR, "init dms file complete, rc = %d" ,rc);
done:
    return rc;
error:
    goto done;
}

int rtn::rtnInsert(BSONObj &record) {
    int rc = EDB_OK;
    dmsRecordID recordID;
    BSONObj outRecord;

    // check if _id exists
    rc = _ixmBucketMgr->isIDExist(record);
    PD_RC_CHECK(rc, PDERROR, "Failed to call isIDExist, rc = %d", rc);
    // write data into file
    rc = _dmsFile->insert(record, outRecord, recordID);
    if (rc) {
        PD_LOG(PDERROR, "Failed to call dms insert, rc = %d", rc);
        goto error;
    }
    rc = _ixmBucketMgr->createIndex(outRecord, recordID);
    PD_RC_CHECK(rc, PDERROR, "Failed to call ixmBucketMgr create index, rc = %d", rc);
done:
    return rc;
error:
    goto done;
}

int rtn::rtnFind(BSONObj &inRecord, BSONObj &outRecord) {
    int rc = EDB_OK;
    dmsRecordID recordID;
    rc = _ixmBucketMgr->findIndex(inRecord, recordID);
    PD_RC_CHECK(rc, PDERROR, "Failed to call ixm findindex, rc = %d", rc);
    rc = _dmsFile->find(recordID, outRecord);
    PD_RC_CHECK(rc, PDERROR, "Failed to call dms find, rc = %d", rc);
done:
    return rc;
error:
    goto done;
}

int rtn::rtnRemove(BSONObj &record) {
    int rc = EDB_OK;
    dmsRecordID recordID;
    rc = _ixmBucketMgr->removeIndex(record, recordID);
    PD_RC_CHECK(rc, PDERROR, "Failed to call ixm removeIndex, rc = %d", rc);
    rc = _dmsFile->remove(recordID);
    PD_RC_CHECK(rc, PDERROR, "Failed to call dms remove, rc = %d", rc);
done:
    return rc;
error:
    goto done;
}