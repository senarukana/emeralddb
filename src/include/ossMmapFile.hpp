
#ifndef OSS_MMAP_FILE_HPP__
#define OSS_MMAP_FILE_HPP__

#include "core.hpp"
#include "ossLatch.hpp"
#include "ossFileOp.hpp"

class _ossMmapFile {
protected:
    class _ossMmapSegment {
    public:
        void *_ptr;
        unsigned int        _length;
        unsigned long long  _offset;
        _ossMmapSegment(void *ptr,
                        unsigned int length,
                        unsigned long long offset) {
            _ptr = ptr;
            _length = length;
            _offset = offset;
        }
    };
    typedef _ossMmapSegment ossMmapSegment;

    ossFileOp _fileOp;
    ossXLatch _mutex;
    bool _opened;
    std::vector<_ossMmapSegment> _segments;
    char _fileName[OSS_MAX_PATHSIZE];
public:
    typedef std::vector<_ossMmapSegment>::const_iterator CONST_ITR;

    inline CONST_ITR begin() {
        return _segments.begin();
    }

    inline CONST_ITR end() {
        return _segments.end();
    }
    inline unsigned int segmentSize() {
        return _segments.size();
    }
public:
    _ossMmapFile() {
        _opened = false;
        memset(_fileName, 0 , sizeof(_fileName));
    }
    ~_ossMmapFile() {
        close();
    }

    int open(const char *pFileName, unsigned int options);
    void close();
    int map(unsigned long long offset, unsigned int length, void **pAddress);
};
typedef class _ossMmapFile ossMmapFile;

#endif