/*******************************************************************************
Copyright (C) 2013 SequoiaDB Software Inc.

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU Affero General Public License, version 3,
as published by the Free Software Foundation.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
GNU Affero General Public License for more details.

You should have received a copy of the GNU Affero General Public License
along with this program. If not, see <http://www.gnu.org/license/>.
*******************************************************************************/
#include "core.hpp"
#include "ossFileOp.hpp"
#include "ossLatch.hpp"
#include "pd.hpp"

const static char *PDLEVELSTRING[] =
{
    "SEVERE",
    "ERROR",
    "EVENT",
    "WARNING",
    "INFO",
    "TRACE"
};

const char *getPDLevel(PDLEVEL level) {
    if ((unsigned int)level > (unsigned int)PDTRACE) {
        return "Unknown Level";
    }
    return PDLEVELSTRING[(unsigned int)level];
}

const static char *PD_LOG_HEADER_FORMAT="%04d-%02d-%02d-%02d.%02d.%02d.%06d\
               \
    Level:%s" OSS_NEWLINE "PID:%-41dTID:%d" OSS_NEWLINE "Function:%-36sLine:%d"\
    OSS_NEWLINE "File:%s" OSS_NEWLINE"Message: " OSS_NEWLINE"%s" OSS_NEWLINE OSS_NEWLINE;


PDLEVEL _curPDLevel = PDTRACE ;
PDLEVEL _curPDFileLevel = PDTRACE;


char _pdDiagLogPath[OSS_MAX_PATHSIZE+1] = {0} ;

ossXLatch _pdLogMutex;
ossFileOp _pdLogFile;

// open log file
static int _pdLogFileReopen() {
    int rc = EDB_OK;

    _pdLogFile.Close();
    rc = _pdLogFile.Open(_pdDiagLogPath);
    if (rc) {
        printf("Failed to open log file, errno = %d" OSS_NEWLINE, rc);
        goto error;
    }
    _pdLogFile.seekToEnd();
done:
    return rc;
error:
    rc = errno;
    goto done;
}

static int _pdLogFileWrite(char *pData) {
    int rc = EDB_OK;
    size_t dataSize = strlen(pData);

    _pdLogMutex.get();
    if (!_pdLogFile.isValid()) {
        // open the file
        rc = _pdLogFileReopen();
        if (rc) {
            printf("Failed to open log file, errno = %d" OSS_NEWLINE, rc);
            goto error;
        }
    }
    // write into the file
    rc = _pdLogFile.Write(pData, dataSize);
    if (rc) {
        printf("Failed to write into log file, errno = %d" OSS_NEWLINE, rc);
        goto error;
    }
done:
    _pdLogMutex.release();
    return rc;
error:
    rc = errno;
    goto done;
}

void pdLog(PDLEVEL level, const char *func, const char *file, unsigned int line, const char *format, ...) {
    int rc = EDB_OK;
    if (level >= _curPDLevel) {
      return;
    }
    va_list ap;
    char userInfo[PD_LOG_STRINGMAX] ; // for user defined message
    char sysInfo[PD_LOG_STRINGMAX] ;  // for log header
    struct tm otm;
    struct timeval tv;
    struct timezone tz;
    time_t tt;

    gettimeofday(&tv, &tz);
    tt = tv.tv_sec;
    localtime_r(&tt, &otm);

    // create user information
    va_start(ap, format);
    vsnprintf(userInfo, PD_LOG_STRINGMAX, format, ap);
    va_end(ap);

    snprintf(sysInfo, PD_LOG_STRINGMAX, PD_LOG_HEADER_FORMAT,
        otm.tm_year+1900,
        otm.tm_mon+1,
        otm.tm_mday,
        otm.tm_hour,
        otm.tm_min,
        otm.tm_sec,
        tv.tv_usec,
        PDLEVELSTRING[level],
        getpid(),
        syscall(SYS_gettid),
        func,
        line,
        file,
        userInfo);
    printf("%s" OSS_NEWLINE, sysInfo);
    if (_pdDiagLogPath[0] != '\0') {
        if (level <= _curPDFileLevel) {
            rc = _pdLogFileWrite(sysInfo);
            if (rc){
                printf ("Failed to write into log file, errno = %d" OSS_NEWLINE, rc ) ;
                printf ("%s" OSS_NEWLINE, sysInfo) ;
                abort();
            }
        }
    }
    return;
} 