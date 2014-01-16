
#include "core.hpp"
#include "pmd.hpp"
#include "rtn.hpp"
#include "pd.hpp"

using namespace bson;

rtn::rtn():_dmsFile(NULL) {
}

rtn::~rtn() {
    if (_dmsFile) {
        delete(_dmsFile);
    }
}

int rtn::init() {
    int rc = EDB_OK;
    _dmsFile = new(std::nothrow)dmsFile();
    if (!_dmsFile) {
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
done:
    return rc;
error:
    goto done;
}

int rtn::rtnInsert(BSONObj &record) {
    int rc = EDB_OK;
    dmsRecordID recordID;
    BSONObj outRecord;
    // write data into file
    rc = _dmsFile->insert(record, outRecord, recordID);
    if (rc) {
        PD_LOG(PDERROR, "Failed to call dms insert, rc = %d", rc);
        goto error;
    }
done:
    return rc;
error:
    goto done;
}

int rtn::rtnFind(BSONObj &inRecord, BSONObj &outRecord) {
    return EDB_OK;
}

int rtn::rtnRemove(BSONObj &record) {
    // int rc = EDB_OK;
    return EDB_OK;
}