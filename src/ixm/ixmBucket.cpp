
#include "core.hpp"
#include "ixmBucket.hpp"
#include "pd.hpp"

using namespace std;
using namespace bson;

unsigned int hashFunc (const char *data, int len) {
    return 5;
}

int ixmBucketManager::isIDExist(BSONObj &record) {
    int rc                  = EDB_OK;
    unsigned int hashNum    = 0;
    unsigned int bucketIdx  = 0;
    ixmEleHash eleHash;
    dmsRecordID recordID;

    rc = _processData(record, recordID, hashNum, eleHash, bucketIdx);
    if (rc) {
        goto error;
    }
    rc = _bucket[bucketIdx]->isIDExist(hashNum, eleHash);
    if (rc) {
        goto error;
    }
done:
    return rc;
error:
    goto done;
}

int ixmBucketManager::init() {
    int rc = EDB_OK;
    ixmBucket *temp = nullptr;
    for (int i = 0; i < IXM_HASH_MAP_SIZE; i++) {
        temp = new(std::nothrow)ixmBucket();
        if (temp == nullptr) {
            rc = EDB_OOM;
            PD_LOG(PDERROR, "Failed to allocate new ixmBucket");
            goto error;
        }
        _bucket.push_back(temp);
        temp = nullptr;
    }
done:
    return rc;
error:
    goto done;
}

int ixmBucketManager::createIndex(BSONObj &record, dmsRecordID &recordID) {
    int rc                  = EDB_OK;
    unsigned int hashNum    = 0;
    unsigned int bucketIdx  = 0;
    ixmEleHash eleHash;

    rc = _processData(record, recordID, hashNum, eleHash, bucketIdx);
    if (rc) {
        goto error;
    }
    rc = _bucket[bucketIdx]->createIndex(hashNum, eleHash);
    if (rc) {
        goto error;
    }
    recordID = eleHash.recordID;
done:
    return rc;
error:
    goto done;
}

int ixmBucketManager::removeIndex(BSONObj &record, dmsRecordID &recordID) {
    int rc                  = EDB_OK;
    unsigned int hashNum    = 0;
    unsigned int bucketIdx  = 0;
    ixmEleHash eleHash;

    rc = _processData(record, recordID, hashNum, eleHash, bucketIdx);
    if (rc) {
        goto error;
    }
    rc = _bucket[bucketIdx]->removeIndex(hashNum, eleHash);
    if (rc) {
        goto error;
    }
    recordID = eleHash.recordID;
done:
    return rc;
error:
    goto done;
}

int ixmBucketManager::findIndex(BSONObj &record, dmsRecordID &recordID) {
    int rc                  = EDB_OK;
    unsigned int hashNum    = 0;
    unsigned int bucketIdx  = 0;
    ixmEleHash eleHash;

    rc = _processData(record, recordID, hashNum, eleHash, bucketIdx);
    if (rc) {
        goto error;
    }
    rc = _bucket[bucketIdx]->findIndex(hashNum, eleHash);
    if (rc) {
        goto error;
    }
    recordID = eleHash.recordID;
done:
    return rc;
error:
    goto done;
}


int ixmBucketManager::ixmBucket::createIndex(unsigned int hashNum, ixmEleHash &eleHash) {
    int rc = EDB_OK;
    _mutex.get();
    _bucketMap.insert(pair<unsigned int, ixmEleHash>(hashNum, eleHash));
    _mutex.release();
    return rc;
}

int ixmBucketManager::ixmBucket::isIDExist(unsigned int hashNum, ixmEleHash &eleHash) {
    int rc = EDB_OK;
    BSONElement destEle;
    BSONElement srcEle;
    ixmEleHash existEle;
    pair<multimap<unsigned int, ixmEleHash>::iterator,
        multimap<unsigned int, ixmEleHash>::iterator> ret;
    /***********************CRITICAL SECTION*********************/
    _mutex.get_shared();
    ret = _bucketMap.equal_range(hashNum);
    srcEle = BSONElement(eleHash.data);
    for (multimap<unsigned int, ixmEleHash>::iterator it = ret.first;
        it != ret.second; it++) {
        existEle = it->second;
        destEle = BSONElement(existEle.data);
        if (srcEle.type() == destEle.type()) {
            if (srcEle.valuesize() == destEle.valuesize()) {
                if (!memcmp(srcEle.value(), destEle.value(), destEle.valuesize())) {
                    rc = EDB_IXM_ID_EXIST;
                    PD_LOG(PDERROR, "record _id does exist");
                    goto error;
                }
            }
        }
    }
    /*****************END OF CRITICAL SECTION*********************/
done:
    _mutex.release_shared();
    return rc;
error:
    goto done;
}

int ixmBucketManager::ixmBucket::findIndex(unsigned int hashNum, ixmEleHash &eleHash) {
    int rc = EDB_OK;
    BSONElement destEle;
    BSONElement srcEle;
    ixmEleHash existEle;
    pair<multimap<unsigned int, ixmEleHash>::iterator,
        multimap<unsigned int, ixmEleHash>::iterator> ret;
    /***********************CRITICAL SECTION*********************/
    _mutex.get_shared();
    ret = _bucketMap.equal_range(hashNum);
    srcEle = BSONElement(eleHash.data);
    for (multimap<unsigned int, ixmEleHash>::iterator it = ret.first;
        it != ret.second; it++) {
        existEle = it->second;
        destEle = BSONElement(existEle.data);
        if (srcEle.type() == destEle.type()) {
            if (srcEle.valuesize() == destEle.valuesize()) {
                if (!memcmp(srcEle.value(), destEle.value(), destEle.valuesize())) {
                    eleHash.recordID = existEle.recordID;
                    goto done;
                }
            }
        }
    }
    rc = EDB_INVALIDARG;
    PD_LOG(PDERROR, "element not found");
    goto error;
    /*****************END OF CRITICAL SECTION*********************/
done:
    _mutex.release_shared();
    return rc;
error:
    goto done;
}

int ixmBucketManager::ixmBucket::removeIndex(unsigned int hashNum, ixmEleHash &eleHash) {
    int rc = EDB_OK;
    BSONElement destEle;
    BSONElement srcEle;
    ixmEleHash existEle;
    pair<multimap<unsigned int, ixmEleHash>::iterator,
        multimap<unsigned int, ixmEleHash>::iterator> ret;
    /***********************CRITICAL SECTION*********************/
    _mutex.get();
    ret = _bucketMap.equal_range(hashNum);
    srcEle = BSONElement(eleHash.data);
    for (multimap<unsigned int, ixmEleHash>::iterator it = ret.first;
        it != ret.second; it++) {
        existEle = it->second;
        destEle = BSONElement(existEle.data);
        if (srcEle.type() == destEle.type()) {
            if (srcEle.valuesize() == destEle.valuesize()) {
                if (!memcmp(srcEle.value(), destEle.value(), destEle.valuesize())) {
                    eleHash.recordID = existEle.recordID;
                    _bucketMap.erase(it);
                    goto done;
                }
            }
        }
    }
    rc = EDB_INVALIDARG;
    PD_LOG(PDERROR, "element not found");
    goto error;
    /*****************END OF CRITICAL SECTION*********************/
done:
    _mutex.release();
    return rc;
error:
    goto done;
}

int ixmBucketManager::_processData(BSONObj &record, dmsRecordID &recordID, //in
                    unsigned int &hashNum, ixmEleHash &eleHash, //out
                    unsigned int &bucketIdx) {
    int rc                  = EDB_OK;
    BSONElement element     = record.getField(IXM_KEY_FIELD_NAME);
    // check if _id exists and correct 
    if (element.eoo() ||
        (element.type() != NumberInt && element.type() != String)) {
        rc = EDB_INVALIDARG;
        PD_LOG(PDERROR, "record must be with _id");
        goto error;
    }
    // hash _id
    hashNum = hashFunc(element.value(), element.valuesize());
    bucketIdx = hashNum % IXM_HASH_MAP_SIZE;
    eleHash.data = element.rawdata();
    eleHash.recordID = recordID;
done:
    return rc;
error:
    goto done;
}