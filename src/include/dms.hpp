
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
#define DMS_MAX_RECORD (DMS_PAGESIZE-sizeof(dmsHeader)-sizeof(dmsRecord)-sizeof(SLOTOFF))
#define DMS_MAX_PAGES 262144
typedef unsigned int SLOTOFF
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

#define DMS_HEADER_VERSIO_0     0
#define DMS_HEADER_VERSION_CURRENT DMS_HEADER_VERSIO_0

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
#define DMS_HEADER_FLAG_NORMAL 0
#define DMS_HEADER_FLAG_UNALLOC 1
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

#endif