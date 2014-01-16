

#ifndef RTN_HPP__
#define RTN_HPP__

#include "bson.h"
#include "dms.hpp"

// define the storage file name
#define RTN_FILE_NAME "data.1"

using namespace bson;

class rtn {
private:
    dmsFile         *_dmsFile;
public:
    rtn();
    ~rtn();
    int init();
    int rtnInsert(BSONObj &record);
    int rtnFind(BSONObj &inrecord, BSONObj &outRecord);
    int rtnRemove(BSONObj &record);
};

#endif