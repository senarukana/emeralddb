
#ifndef DMS_HPP__
#define DMS_HPP__

#include "ossLatch.hpp"
#include "ossMmapFile.hpp"
#include "bson.h"
#include "dmsRecord.hpp"
#include <vector>

#define DMS_EXTEND_SIZE 65536
// 4MB for page size
#define DMS_PAGE_SIZE 4194304
#define DMS_MAX_RECORD (DMS_PAGE_SIZE-sizeof(dmsHeader)-sizeof(dmsRecord)-sizeof(SLOTOFF))
#define DMS_MAX_PAGES 262144
typedef unsigned int SLOTOFF;
#define DMS_INVALID_SLOTID  0xFFFFFFFF
#define DMS_INVALID_PAGEID  0xFFFFFFFF

#define DMS_KEY_FILENAME    "_id"

extern const char *gKeyFieldName;

// each record has the following header, include 4 bytes size and 4 bytes flag
#define DMS_RECORD_FLAG_NORMAL 0
#define DMS_RECORD_FLAG_DROPPED 1
struct dmsRecord {
    unsigned int    _size;
    unsigned int    _flag;
    char            _data[0];
};

#define DMS_HEADER_MAGIC    "LTED"
#define DMS_HEADER_MAGIC_LEN 4

#define DMS_HEADER_FLAG_NORMAL 0
#define DMS_HEADER_FLAG_DROPPED 1

#define DMS_HEADER_VERSION_0     0
#define DMS_HEADER_VERSION_CURRENT DMS_HEADER_VERSION_0

struct dmsHeader {
    char            _magic[DMS_HEADER_MAGIC_LEN];
    unsigned int    _size;
    unsigned int    _flag;
    unsigned int    _version;
};


// page structure
/********************************
PAGE STRUCTURE
=============================
|       PAGE HEADER         |
=============================
|       Slot List           |
=============================
|       Free Space          |
=============================
|       Data                |
==============================
*********************************/

#define DMS_PAGE_MAGIC "PAGE"
#define DMS_PAGE_MAGIC_LEN 4
#define DMS_PAGE_FLAG_NORMAL 0
#define DMS_PAGE_FLAG_UNALLOC 1
#define DMS_SLOT_EMPTY          0xFFFFFFFF


struct dmsPageHeader {
    char        _magic[DMS_PAGE_MAGIC_LEN];
    unsigned int _size;
    unsigned int _flag;
    unsigned int _numSlots;
    unsigned int _slotOffset;
    unsigned int _freeSpace;
    unsigned int _freeOffset;
    char         _data[0];
};

#define DMS_FILE_SEGMENT_SIZE 134217728
#define DMS_FILE_HEADER_SIZE 65536
#define DMS_PAGES_PER_SEGMENT   (DMS_FILE_SEGMENT_SIZE/DMS_PAGE_SIZE)
#define DMS_MAX_SEGMENTS        (DMS_MAX_PAGES/DMS_PAGES_PER_SEGMENT)

class dmsFile: public ossMmapFile {
private:
    //points to memory where header is located
    dmsHeader           *_header;
    std::vector<char *> _body; //segment list
    // free space to page id map
    multimap<unsigned int, PAGEID> _freeSpaceMap;
    ossSLatch           _mutex;
    ossXLatch           _extendMutex;
    char                *_pFileName;
public:
    dmsFile();
    ~dmsFile();
    // initialize the dms file
    int init(const char *pFileName);
    //insert into file
    int insert(bson::BSONObj &record, bson::BSONObj &outRecord, dmsRecordID &rid);
    int remove(dmsRecordID &rid);
    int find(dmsRecordID &rid, bson::BSONObj &result);
private:
    // create a new segment for the current file
    int _extendSegment();
    // init from empty file, creating header only
    int _initHeader();
    // extend the file for given bytes
    int _extendFile(int size);
    // load data from beginning
    int _loadData();
    // search slot
    int _searchSlot(char *page,
                    dmsRecordID &recordID,
                    SLOTOFF &slot);
    // reorg page
    void _recoverSpace(char *page);
    // update free space
    void _updateFreeSpace(dmsPageHeader *header, int changeSize,
                        PAGEID pageID);
    // find a page id to insert, return invalid_pageid if there's no page can be found for request size bytes
    PAGEID _findPage(size_t requireSize);
public:
    inline unsigned int getNumSegments() {
        return _body.size();
    }
    inline unsigned int getNumPages() {
        return getNumSegments() * DMS_PAGES_PER_SEGMENT;
    }
    inline char *pageToOffset(PAGEID pageID) {
        if (pageID >= getNumPages()) {
            return NULL;
        } 
        return _body[pageID/DMS_PAGES_PER_SEGMENT] + DMS_PAGE_SIZE * (pageID % DMS_PAGES_PER_SEGMENT);
    }

    inline bool validSize(size_t size) {
        if (size < DMS_FILE_HEADER_SIZE) {
            return false;
        }
        size = size - DMS_FILE_HEADER_SIZE;
        if (size % DMS_FILE_SEGMENT_SIZE != 0) {
            return false;
        }
        return true;
    }

};

#endif