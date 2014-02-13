#ifndef IXM_HPP__
#define IXM_HPP__

#include "ossLatch.hpp"
#include "bson.h"
#include "dmsRecord.hpp"
#include <map>

using namespace std;
using namespace bson;

#define IXM_KEY_FIELD_NAME "_id"
#define IXM_HASH_MAP_SIZE  1000

struct ixmEleHash {
    const char *data;
    dmsRecordID recordID;
};

class ixmBucketManager {
private:
    class ixmBucket {
        // the map is hashNum and eleHash
    private:
        multimap<unsigned int, ixmEleHash> _bucketMap;
        ossSLatch _mutex;
    public:
        // get the record whether exist
        int isIDExist(unsigned int hashNum, ixmEleHash &eleHash);
        int createIndex(unsigned int hashNum, ixmEleHash &eleHash);
        int findIndex(unsigned int hashNum, ixmEleHash &eleHash);
        int removeIndex(unsigned int hashNum, ixmEleHash &eleHash);
    };
    int _processData(BSONObj &record, dmsRecordID &recordID, //in
                    unsigned int &hashNum, ixmEleHash &eleHash, //out
                    unsigned int &random);
private:
    vector<ixmBucket *> _bucket;
public:
    ixmBucketManager(){}
    ~ixmBucketManager() {
        ixmBucket *pIxmBucket = nullptr;
        for (int i = 0; i < IXM_HASH_MAP_SIZE; i++) {
            pIxmBucket = _bucket[i];
            if (pIxmBucket)
                delete pIxmBucket;
        }
    }
    int init();
    int isIDExist(BSONObj &record);
    int createIndex(BSONObj &record, dmsRecordID &recordID);
    int findIndex(BSONObj &record, dmsRecordID &recordID);
    int removeIndex(BSONObj &record, dmsRecordID &recordID);
};

#endif