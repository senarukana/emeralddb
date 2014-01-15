
#ifndef DMS_RECORD_HPP__
#define DMS_RECORD_HPP__

typedef unsigned int PAGEID;
typedef unsigned int SLOTID;
// each record i represented by RID, which can be broken in to page id and slotid

struct dmsRecordID {
    PAGEID _pageId;
    SLOTID _slotId;
};

#endif