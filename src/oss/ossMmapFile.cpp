
#include "core.hpp"
#include "ossFileOp.hpp"
#include "ossMmapFile.hpp"
#include "pd.hpp"

int _ossMmapFile::open(const char *pFileName, unsigned int options) {
    int rc = EDB_OK;
    _mutex.get();
    rc = _fileOp.Open(pFileName, options);
    if (rc == EDB_OK) {
        _opened = true;
    } else {
        PD_LOG(PDERROR, "Failed to open file, rc = %d", rc);
        goto error;
    }
    strncpy(_fileName, pFileName, sizeof(_fileName));
done:
    _mutex.release();
    return rc;
error:
    goto done;
}

void _ossMmapFile::close() {
    std::vector<ossMmapSegment>::iterator it;
    _mutex.get();
    for ( it = _segments.begin();it != _segments.end(); it++) {
        munmap((void *)(*it)._ptr, (*it)._length);
    }
    _segments.clear();
    if (_opened) {
        _fileOp.Close();
        _opened = false;
    }
    _mutex.release();
}

int _ossMmapFile::map(unsigned long long offset, unsigned int length, void **pAddress) {
    int rc = EDB_OK;
    ossMmapSegment seg{ NULL, 0, 0};
    unsigned long long fileSize;
    void *segment = NULL;

    if (length == 0) {
        goto done;
    }
    /******************CRITICAL SECTION ******************/
    _mutex.get();
    rc = _fileOp.getSize((off_t*)&fileSize);
    if (rc) {
        PD_LOG(PDERROR,
            "Failed to get file size, rc = %d", rc);
        goto error;
    }
    if (offset + length > fileSize) {
        PD_LOG(PDERROR,
            "Offset is greater than file size");
        rc = EDB_INVALIDARG;
        goto error;
    }

    segment = mmap(NULL, length, PROT_READ | PROT_WRITE,
                MAP_SHARED, _fileOp.getHandle(), offset);
    if (segment == MAP_FAILED) {
        PD_LOG(PDERROR,
                "Failed to map offset %ld length %d, errno = %d",
                offset, length, errno);
        if (errno == ENOMEM) {
            rc = EDB_OOM;
        } else if (errno == EACCES) {
            rc = EDB_PERM;
        } else {
            rc = EDB_SYS;
        }
        goto error;
    }
    seg._ptr = segment;
    seg._length = length;
    seg._offset = offset;
    _segments.push_back(seg);
    if(pAddress) {
        *pAddress = segment;
    }
done:
    _mutex.release();
    return rc;
error:
    goto done;
}