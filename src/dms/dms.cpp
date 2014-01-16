
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
    int rc                  = EDB_OK;
    PAGEID pageID           = 0;
    char *page              = nullptr;
    dmsPageHeader *pageHeader   = nullptr;
    int recordSize          = 0;
    SLOTOFF offsetData      = 0;
    dmsRecord recordHeader;

    recordSize = record.objsize(); 
    // when we attempt to insert record, first we have to verify it include _id field
    // sanity check for recordSize
    if ((unsigned int)recordSize > DMS_MAX_RECORD) {
        rc = EDB_INVALIDARG;
        PD_LOG(PDERROR, "record too big, size is %d", recordSize);
    }

    /******************** CRITICAL SECTION ******************/
retry:
    _mutex.get();
    // 1. find a page to insert the data
    pageID = _findPage(recordSize);
    if (pageID == DMS_INVALID_PAGEID) {
        _mutex.release();

        if (_extendMutex.try_get()) {
            // extend the file
            rc = _extendSegment();
            if (rc) {
                _extendMutex.release();
                PD_LOG(PDERROR, "extend segment error, rc = %d", rc);
                goto error;
            }
        } else {
            // if we can't get the extendmutex, that means someone else is trying to extend
            // so let's wait until getting the mutex, and release it and try again;
            _extendMutex.get();
        }
        _extendMutex.release();
        goto retry;
    }
    
    // 2. get the data of the page
    page = pageToOffset(pageID);
    // if something wrong, let's return error
    if (!page) {
        rc = EDB_SYS;
        PD_LOG(PDERROR, "Failed to find page");
        goto error_releasemutex;
    }

    // set the pageHeader
    pageHeader = (dmsPageHeader*)page;
    // sanity check for pageHeader
    if (memcmp(pageHeader->_magic, DMS_PAGE_MAGIC, DMS_PAGE_MAGIC_LEN) != 0) {
        rc = EDB_SYS;
        PD_LOG(PDERROR, "Invalid page pageHeader");
        goto error_releasemutex;
    }
    // 3. check if we need to reorganize the page
    if (pageHeader->_freeSpace > (pageHeader->_freeOffset - pageHeader->_slotOffset) &&
        // if there is no free space excluding holes
        (pageHeader->_slotOffset + recordSize + sizeof(dmsRecord) + sizeof(SLOTID)) > pageHeader->_freeOffset) {
        // recover empty hole from page
        _recoverSpace(page);
    }
    // sanity check for freespace
    if ((pageHeader->_freeSpace < recordSize + sizeof(dmsRecord) + sizeof(SLOTID)) ||
        (pageHeader->_freeOffset - pageHeader->_slotOffset <
            recordSize + sizeof(dmsRecord) + sizeof(SLOTID))) {
        rc = EDB_SYS;
        PD_LOG(PDERROR, "There is no free space for insert");
        goto error_releasemutex;
    }
    // 4. update the page
    offsetData = pageHeader->_freeOffset - sizeof(dmsRecord) - recordSize;
    recordHeader._size = recordSize;
    recordHeader._flag = DMS_RECORD_FLAG_NORMAL;
    // append a slot
    *(SLOTOFF*)(page + pageHeader->_slotOffset) = offsetData;
    // copy the record pageHeader
    memcpy(page + offsetData, (void *)&recordHeader, sizeof(dmsRecord));
    // copy the record data
    memcpy(page + offsetData + sizeof(dmsRecord), record.objdata(), recordSize);
    outRecord = BSONObj(page + offsetData + sizeof(dmsRecord));
    rid._pageID = pageID;
    rid._slotID = pageHeader->_numSlots;
    // 5. update the metadata in page and db
    // modify metadata in page
    pageHeader->_numSlots++;
    pageHeader->_slotOffset += sizeof(SLOTID);
    pageHeader->_freeOffset = offsetData;
    // modify database metadata
    _updateFreeSpace(pageHeader, -(recordSize+sizeof(SLOTID)+sizeof(dmsRecord)), pageID);
    // release lock for database
    _mutex.release();
    /******************** END OF CRITICAL SECTION ******************/
done:
    return rc;
error_releasemutex:
    _mutex.release();
    goto done;
error:
    goto done;     
}

int dmsFile::remove(dmsRecordID &rid) {
    int rc                      = EDB_OK;
    char *page                  = NULL;
    dmsPageHeader *pageHeader   = nullptr;
    dmsRecord *recordHeader     = nullptr;
    SLOTOFF slot                = 0;

    page = pageToOffset(rid._pageID);
    if (!page) {
        rc = EDB_SYS;
        PD_LOG(PDERROR, "Failed to find the page for %u:%u",
            rid._pageID, rid._slotID);
        goto done;
    }
    /*****************CRITICAL SECTION*****************/
    _mutex.get();
    rc = _searchSlot(page, rid, slot);
    if (rc) {
        PD_LOG(PDERROR, "Failed to search slot, rc = %d", rc);
        goto error;
    }
    // check if data has already been removed
    if (slot == DMS_SLOT_EMPTY) {
        rc = EDB_SYS;
        PD_LOG(PDERROR, "The record is dropped");
        goto error;
    }

    // set the pageHeader
    pageHeader = (dmsPageHeader*)page;
    // set the slot to empty
    *(SLOTOFF*)(page + sizeof(dmsPageHeader) +
                rid._slotID * sizeof(SLOTOFF)) = DMS_SLOT_EMPTY;
    // set the record header
    recordHeader = (dmsRecord*)(page + slot);
    recordHeader->_flag = DMS_RECORD_FLAG_DROPPED;
    // update database metadata
    _updateFreeSpace(pageHeader, recordHeader->_size, rid._pageID);
    _mutex.release();
    /*****************END OF CRITICAL SECTION*****************/
done:
    return rc;
error:
    _mutex.release();
    goto done;
}

int dmsFile::find(dmsRecordID &rid, BSONObj &result) {
    int rc          = EDB_OK;
    SLOTOFF slot    = 0;
    char *page      = NULL;
    dmsRecord *recordHeader = nullptr;

    page = pageToOffset(rid._pageID);
    if (!page) {
        rc = EDB_SYS;
        PD_LOG(PDERROR, " Failed to find the page");
        goto done;
    }
    /*****************CRITICAL SECTION*****************/
    _mutex.get_shared();
    rc = _searchSlot(page, rid, slot);
    if (rc) {
        rc = EDB_SYS;
        PD_LOG(PDERROR, "Failed to search slot, rc = %d", rc);
        goto error;
    }

    // if slot is empty, this should not happen
    if (slot == DMS_SLOT_EMPTY) {
        rc = EDB_SYS;
        PD_LOG(PDERROR, "The record has already dropped");
        goto error;
    }
    // get the recordHeader
    recordHeader = (dmsRecord*)(page + slot);
    // sanity check for record
    if (recordHeader->_flag == DMS_RECORD_FLAG_DROPPED) {
        rc = EDB_SYS;
        PD_LOG(PDERROR, "The record has already dropped");
        goto error;
    }
    result = BSONObj(page + slot + sizeof(dmsRecord)).copy();
done:
    _mutex.release_shared();
    return rc;
error:
    goto done;
    /*****************END OF CRITICAL SECTION*****************/
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

    // 1. let's get the size of file before extend
    rc = _fileOp.getSize(&offset);
    PD_RC_CHECK(rc, PDERROR, "Failed to get file size, rc = %d", rc);
    // 2. extend the file
    rc = _extendFile(DMS_FILE_SEGMENT_SIZE);
    PD_RC_CHECK(rc, PDERROR, "Failed to extend segment rc = %d", rc);

    // map from original end to new end
    rc = map(offset, DMS_FILE_SEGMENT_SIZE, (void **)&data);
    PD_RC_CHECK(rc, PDERROR, "Failed to map file, rc = %d", rc);

    // 3. create page header structure and we are going to copy to each page
    strcpy(pageHeader._magic, DMS_PAGE_MAGIC);
    pageHeader._size = DMS_PAGE_SIZE;
    pageHeader._flag = DMS_PAGE_FLAG_NORMAL;
    pageHeader._numSlots = 0;
    pageHeader._slotOffset = sizeof(dmsPageHeader);
    pageHeader._freeSpace = DMS_PAGE_SIZE - sizeof(dmsPageHeader);
    pageHeader._freeOffset = DMS_PAGE_SIZE;
    // 4. copy header to each page
    for (int i = 0; i < DMS_FILE_SEGMENT_SIZE; i+=DMS_PAGE_SIZE ) {
        memcpy(data+i, (char*)&pageHeader, sizeof(dmsPageHeader));
    }

    // 5. free space handling
    freeMapSize = _freeSpaceMap.size();
    // insert into free space map
    for (int i = 0; i < DMS_PAGES_PER_SEGMENT; i++) {
        _freeSpaceMap.insert(pair<unsigned int, PAGEID>
            (pageHeader._freeSpace, i+freeMapSize));
    }

    // 6. push the segment into body list
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
    char temp[DMS_EXTEND_SIZE] = {0};
    memset(temp, 0, DMS_EXTEND_SIZE);
    if (size % DMS_EXTEND_SIZE != 0) {
        rc = EDB_SYS;
        PD_LOG(PDERROR, "Invalid extend size, must be mulitple of %d", DMS_EXTEND_SIZE);
        goto error;
    }
    // write file
    for (int i = 0; i < size; i+= DMS_EXTEND_SIZE) {
        _fileOp.seekToEnd();
        rc = _fileOp.Write(temp, DMS_EXTEND_SIZE);
        PD_RC_CHECK(rc, PDERROR, "Failed to write to file, rc = %d", rc);
    }

    // offsetType fileSize;

    // // 1. get file size
    // rc = _fileOp.getSize(&fileSize);
    // PD_RC_CHECK(rc, PDERROR, 
    //     "Failed to get file size, rc = %d", rc);

    // // 2. seek to the specified size
    // rc = _fileOp.seekToOffset(fileSize + size);
    // if (rc == -1) {
    //     PD_LOG(PDERROR,
    //         "Failed to seek to offset, rc = %d", rc);
    // }
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
                    pageHeader->_freeSpace, j + (DMS_PAGES_PER_SEGMENT) * i));
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

