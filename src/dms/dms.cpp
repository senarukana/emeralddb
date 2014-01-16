
#include "dms.hpp"
#include "pd.hpp"

using namespace bson;

const char *gKeyFieldName = DMS_KEY_FILENAME;

dmsFile::dmsFile():
    _header(NULL),
    _pFileName(NULL)
{
}

dmsFile::~dmsFile() {
    if (_pFileName) {
        free(_pFileName);
    }
    close();
}

int dmsFile::insert(BSONObj &record, BSONObj &outRecord, dmsRecordID &rid) {
    return EDB_OK;
}

int dmsFile::remove(dmsRecordID &rid) {
    return EDB_OK;
}

int dmsFile::find(dmsRecord &rid, BSONObj &result) {
    return EDB_OK;
}

void dmsFile::_updateFreeSpace(dmsPageHeader *header, int changeSize,
                                PAGEID pageID) {
    unsigned int freeSpace = header->_freeSpace;
    std::pair<std::multimap<unsigned int, PAGEID>::iterator,
                    std::multimap<unsigned int, PAGEID>::iterator> ret;
    ret = _freeSpaceMap.equal_range(freeSpace);
    for (std::multimap<unsigned int, PAGEID>::iterator it = ret.first;
        it != ret.second; it++) {
        if (it->second == pageID) {
            _freeSpaceMap.erase(it);
            break;
        }
    }
    // increase page free space
    freeSpace += changeSize;
    header->_freeSpace = freeSpace;
    // insert into free space map
    _freeSpaceMap.insert(pair<unsigned int, PAGEID>(freeSpace, pageID));
}

int dmsFile::init(const char *pFileName) {
    offsetType offset = 0;
    int rc = EDB_OK;
    // duplicate file name
    _pFileName = strdup(pFileName);
    if (!_pFileName) {
        rc = EDB_OOM;
        PD_LOG(PDERROR, "Failed to duplicate file name");
        goto error;
    }

    // open file
    rc = open(_pFileName, OSS_FILE_OP_OPEN_ALWAYS);
    PD_RC_CHECK(rc, PDERROR, "Failed to open file %s, rc = %d",
                _pFileName, rc);
getfilesize:
    // get file size
    rc = _fileOp.getSize(&offset);
    PD_RC_CHECK(rc, PDERROR, "Failed to get file size, rc = %d",
                rc);
    // if file size is 0, that means it's newly created file and we should init it
    if (!offset) {
        rc = _initHeader();
        PD_RC_CHECK(rc, PDERROR, "Failed to initialize file, rc = %d",rc);
        goto getfilesize;
    }

    // load data
    rc = _loadData();
    PD_RC_CHECK(rc, PDERROR, "Failed to load data, rc = %d", rc);
done:
    return rc;
error:
    goto done;
}

// caller must hold extend latch
int dmsFile::_extendSegment() {
    // extend a new segment
    int rc = EDB_OK;
    char *data  = NULL;
    int freeMapSize = 0;
    dmsPageHeader pageHeader;
    offsetType offset = 0;

    // first let's get the size of file before extend
    rc = _fileOp.getSize(&offset);
    PD_RC_CHECK(rc, PDERROR, "Failed to get file size, rc = %d", rc);
    // extend the file
    rc = _extendFile(DMS_FILE_SEGMENT_SIZE);
    PD_RC_CHECK(rc, PDERROR, "Failed to extend segment rc = %d", rc);

    // map from original end to new end
    rc = map(offset, DMS_FILE_SEGMENT_SIZE, (void **)&data);
    PD_RC_CHECK(rc, PDERROR, "Failed to map file, rc = %d", rc);

    // create page header structure and we are going to copy to each page
    pageHeader._size = DMS_PAGE_SIZE;
    pageHeader._flag = DMS_PAGE_FLAG_NORMAL;
    pageHeader._numSlots = 0;
    pageHeader._slotOffset = sizeof(dmsPageHeader);
    pageHeader._freeSpace = DMS_PAGE_SIZE - sizeof(dmsPageHeader);
    pageHeader._freeOffset = DMS_PAGE_SIZE;
    // copy header to each page
    for (int i = 0; i < DMS_FILE_SEGMENT_SIZE; i+=DMS_PAGE_SIZE ) {
        memcpy(data+i, (char*)&pageHeader, sizeof(dmsPageHeader));
    }

    // free space handling
    freeMapSize = _freeSpaceMap.size();
    // insert into free space map
    for (int i = 0; i < DMS_PAGES_PER_SEGMENT; i++) {
        _freeSpaceMap.insert(pair<unsigned int, PAGEID>
            (pageHeader._freeSpace, i+freeMapSize));
    }

    // push the segment into body list
    _body.push_back(data);
    _header->_size  += DMS_PAGES_PER_SEGMENT;
done:
    return rc;
error:
    goto done;
}

// Create a db header for a newly created file 
// Should get the lock before.
int dmsFile::_initHeader() {
    int rc = EDB_OK;

    // 1.extend the file for header
    rc = _extendFile(DMS_FILE_HEADER_SIZE);
    PD_RC_CHECK(rc, PDERROR, "Failed to extend file, rc = %d", rc);
    // 2. map the header
    rc = map(0, DMS_FILE_HEADER_SIZE, (void **)&_header);
    PD_RC_CHECK(rc, PDERROR, "Failed to map the file, rc = %d", rc);
    // 3. initialize the header property
    strcpy(_header->_magic, DMS_HEADER_MAGIC);
    _header->_size = 0;
    _header->_flag = DMS_HEADER_FLAG_NORMAL;
    _header->_version = DMS_HEADER_VERSION_CURRENT;
done:
    return rc;
error:
    goto done;
}


// find a page to store a record
// If there is no appropriate page, return DMS_INVALID_PAGEID
PAGEID dmsFile::_findPage(size_t requiredSize) {
    std::multimap<unsigned int, PAGEID>::iterator findIter;
    findIter = _freeSpaceMap.upper_bound(requiredSize);
    if (findIter != _freeSpaceMap.end()) {
        return findIter->second;
    }
    return DMS_INVALID_PAGEID;
}

// Should get the lock before
int dmsFile::_extendFile(int size) {
    int rc = EDB_OK;
    offsetType fileSize;

    // 1. get file size
    rc = _fileOp.getSize(&fileSize);
    PD_RC_CHECK(rc, PDERROR, 
        "Failed to get file size, rc = %d", rc);

    // 2. seek to the specified size
    _fileOp.seekToOffset(fileSize + size);
    PD_RC_CHECK(rc, PDERROR,
        "Failed to seek to offset");
done:
    return rc;
error:
    goto done;
}

int dmsFile::_searchSlot(char *page,
                        dmsRecordID &rid,
                        SLOTOFF &slot) {
    int rc = EDB_OK;
    dmsPageHeader *pageHeader = nullptr;
    if (!page) {
        rc = EDB_SYS;
        PD_LOG(PDERROR, "page is null");
        goto error;
    }
    // sanity check for rid
    if (rid._pageID < 0 || rid._slotID < 0) {
        rc = EDB_SYS;
        PD_LOG(PDERROR, "Invalid RID: %d.%d", 
            rid._pageID, rid._slotID);
        goto error;
    }
    pageHeader = (dmsPageHeader*)page;
    // sanity check for slotid
    if (rid._slotID > pageHeader->_numSlots) {
        rc = EDB_SYS;
        PD_LOG(PDERROR, "Slot is out of range, provided: %d, max :%d",
            rid._slotID, pageHeader->_numSlots);
        goto error;
    }
    slot = *(SLOTOFF*)(page + sizeof(dmsPageHeader) + rid._slotID*(sizeof(SLOTOFF)));
done:
    return rc;
error:
    goto done;
}

int dmsFile::_loadData() {
    int rc          = EDB_OK;
    int numPage     = 0;
    int numSegments = 0;
    dmsPageHeader *pageHeader = NULL;
    char *data      = NULL;
    SLOTID slotID   = 0;
    SLOTOFF slotOffset = 0;
    dmsRecordID recordID;
    bson::BSONObj bson;

    // check if _header is valid
    if (!_header) {
        rc = map(0, DMS_PAGE_SIZE, (void **)&_header);
        PD_RC_CHECK(rc, PDERROR, "Failed to map file header, rc = %d", rc);
    }
    numPage = _header->_size;
    if (numPage % DMS_PAGES_PER_SEGMENT != 0) {
        rc = EDB_SYS;
        PD_LOG(PDERROR, "Failed to load data, partial segments detected");
        goto error;
    } 
    numSegments = numPage / DMS_PAGES_PER_SEGMENT;
    // get the segments number
    if (numSegments > 0) {
        for (int i = 0; i < numSegments; i++) {
            // map each segment into memoery
            rc = map(DMS_FILE_HEADER_SIZE+DMS_FILE_SEGMENT_SIZE * i,
                    DMS_FILE_SEGMENT_SIZE, (void **)&data);
            PD_RC_CHECK(rc, PDERROR, "Failed to map segment %d, rc = %d",
                    i, rc);
            _body.push_back(data);
            // initialize each page into freeSpaceMap
            for (int j = 0; j < DMS_PAGES_PER_SEGMENT; j++) {
                pageHeader = (dmsPageHeader *)(data + j * DMS_PAGE_SIZE);
                _freeSpaceMap.insert(std::pair<unsigned int, PAGEID>(
                    pageHeader->_freeSpace, j));
                slotID = (SLOTID) pageHeader->_numSlots;
                recordID._pageID = (PAGEID)j;
                // for each record in the page, let's insert into index
                // for (unsigned int k = 0; k < slotID; k++) {
                //     slotOffset = *(SLOTOFF*)(data + j*DMS_PAGE_SIZE +
                //                 sizeof(dmsPageHeader) + k*sizeof(SLOTID));
                //     if (slotOffset == DMS_SLOT_EMPTY) {
                //         continue; // has been removed
                //     }
                //     // put it into index
                // } 
            }
        }
    }
done:
    return rc;
error:
    goto done;
}

// reorganize the page for freespace
void dmsFile::_recoverSpace(char *page) {
    char *pLeft         = NULL;
    char *pRight        = NULL;
    SLOTOFF slot        = 0;
    int recordSize      = 0;
    bool isRecover      = false;
    dmsRecord *recordHeader = NULL;
    dmsPageHeader *pageHeader = NULL;

    pLeft = page + sizeof(dmsPageHeader);
    pRight = page + DMS_PAGE_SIZE;

    pageHeader = (dmsPageHeader *)page;
    for (unsigned int i = 0; i < pageHeader->_numSlots; i++) {
        slot = *((SLOTOFF*)(pLeft + sizeof(SLOTOFF) *i));
        if (DMS_SLOT_EMPTY != slot) {
            recordHeader = (dmsRecord *)(page + slot);
            recordSize = recordHeader->_size;
            pRight -= recordSize;
            if (isRecover) {
                memmove(pRight, page + slot, recordSize);
                // update the slot -> dataoffset
                *((SLOTOFF*)(pLeft + sizeof(SLOTOFF) * i)) = (SLOTOFF)(pRight - page);
            }
        } else {
            isRecover = true;
        }
    }
    pageHeader->_freeOffset = pRight - page;
}

