/*******************************************************************************
   Copyright (C) 2013 SequoiaDB Software Inc.

   This program is free software: you can redistribute it and/or modify
   it under the terms of the GNU Affero General Public License, version 3,
   as published by the Free Software Foundation.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
   GNU Affero General Public License for more details.

   You shoudl have received a copy of the GNU Affero General Public License
   along with this program. If not, see <http://www.gnu.org/license/>.
*******************************************************************************/
#include "core.hpp"
#include "ossFileOp.hpp"

ossFileOp::ossFileOp() {
   _fileHandle = OSS_INVALID_HANDLE_FD_VALUE;
   _bIsStdout = false;
}

void ossFileOp::Close() {
   if (isValid() && (!_bIsStdout)) {
      oss_close(_fileHandle);
      _fileHandle = OSS_INVALID_HANDLE_FD_VALUE;
   }
}

int ossFileOp::Open(const char *pFilePath, unsigned int options) {
   int rc = 0;
   int mode = O_RDWR;

   if (options & OSS_FILE_OP_READ_ONLY) {
      mode = O_RDONLY;
   } else if (options & OSS_FILE_OP_WRITE_ONLY) {
      mode = O_WRONLY;
   }
   if (options & OSS_FILE_OP_OPEN_EXISTING){
   }
   else if(options & OSS_FILE_OP_OPEN_ALWAYS){
      mode |= O_CREAT ;
   }
   if (options & OSS_FILE_OP_OPEN_TRUNC){
      mode |= O_TRUNC ;
   }

   do {
      _fileHandle = oss_open(pFilePath, mode, 0644);
   } while((-1 == _fileHandle) && (EINTR == errno));

   if (_fileHandle <= OSS_INVALID_HANDLE_FD_VALUE) {
      rc = errno;
      goto error;
   }
done:
   return rc;
error:
   goto done;
}

int ossFileOp::Read(void *pBuffer, size_t size, int *const pBytesRead) {
   int rc = EDB_OK;
   int nRead;
   int nLeft = size;

   if (isValid()) {
      while (nLeft > 0) {
         nRead = oss_read(_fileHandle, pBuffer, nLeft);
         if (nRead < 0) {
            if (errno == EINTR) {
               continue;
            } else {
               goto error;
            }
         } else if (nRead == 0) {
            goto done;
         } else {
            pBuffer = (void *)((char *)nRead + nRead);
            nLeft -= nRead;
         }
      }
   } else {
      goto error;
   }
done:
   if (pBytesRead) {
      *pBytesRead = size - nLeft;
   }
   return rc;
error:
   rc = errno;
   goto done;
}

int ossFileOp::Write(void *pBuffer, size_t size) {
   int rc = 0;
   int nWrite;
   int nLeft = size;

   if (isValid()) {
      while (nLeft > 0) {
         nWrite = oss_write(_fileHandle, pBuffer, nLeft);
         if (nWrite < 0) {
            if (errno == EINTR) {
               continue;
            } else {
               goto error;
            }
         } else if (nWrite == 0) {
            goto error;
         } else {
            pBuffer = (void*)((char*)pBuffer + nWrite);
            nLeft -= nWrite;
         }
      }
   } else {
      goto error;
   }
done:
   return rc;
error:
   rc = errno;
   goto done;
}

int ossFileOp::fWrite(const char *format, ...) {
   int rc = 0;
   va_list ap;
   char buf[OSS_FILE_OP_FWRITE_BUF_SIZE] = {0};

   va_start(ap, format);
   vsnprintf(buf, sizeof(buf), format, ap);
   va_end(ap);

   rc = Write(buf, strlen(buf));
   return rc;
}

bool ossFileOp::isValid() {
   return (OSS_INVALID_HANDLE_FD_VALUE != _fileHandle);
}

void ossFileOp::openStdout() {
   setFileHandle(STDOUT_FILENO) ;
   _bIsStdout = true ;
}

offsetType ossFileOp::getCurrentOffset () const {
   return oss_lseek( _fileHandle, 0, SEEK_CUR ) ;
}

void ossFileOp::seekToEnd(void) {
   oss_lseek( _fileHandle, 0, SEEK_END ) ;
}

void ossFileOp::seekToOffset(offsetType offset) {
   if ((oss_off_t) -1 != offset){
      oss_lseek( _fileHandle, offset, SEEK_SET) ;
   }
}

void ossFileOp::setFileHandle(handleType handle) {
   if (_fileHandle != OSS_INVALID_HANDLE_FD_VALUE) {
      oss_close(int(_fileHandle));
   }
   _fileHandle = handle ;
}

int ossFileOp::getSize(offsetType *const pFileSize){
   int             rc        = 0 ;
   oss_struct_stat buf       = { 0 } ;

   if (oss_fstat( _fileHandle, &buf) < 0) {
      rc = errno;
      goto error;
   }
   *pFileSize = buf.st_size ;
done:
   return rc ;
error :
   *pFileSize = 0 ;
   goto done;
}
