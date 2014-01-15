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
#include "pmd.hpp"
#include "pmdEDUMgr.hpp"
#include "pmdEDU.hpp"
#include "ossSocket.hpp"
#include "pmdCommand.hpp"
#include "msg.hpp"
#include "pd.hpp"

#include "bson.h"


#define ossRoundUpToMultipleX(x,y) (((x)+((y)-1))-(((x)+((y)-1))%(y)))
#define PMD_AGENT_RECV_BUFFER_SZ    4096
#define EDB_PAGE_SIZE               4096

using namespace bson;

static int pmdProcessAgentRequest(char *pRecvBuffer,
                                    int packetSize,
                                    char **ppResultBuffer,
                                    int *pResultBufferSize,
                                    bool *disconnect,
                                    pmdEDUCB *cb) {
    int rc = EDB_OK;
    pmdCommand *commandMap = pmdGetKRCB()->getCommandMap();
    pmdCommandFunc commandFunc;
    BSONObj retObj;

    // extract message
    MsgHeader *header = (MsgHeader*)pRecvBuffer;
    int opCode = header->opCode;

    commandFunc = commandMap->getCommand(opCode);
    if (commandFunc == NULL) {
        rc = EDB_INVALIDARG;
        goto error;
    }
    rc = commandFunc(pRecvBuffer, packetSize, ppResultBuffer, pResultBufferSize,
             disconnect, cb, &retObj);
    if (rc) {
        goto error;
    }
done:
    // build reply
    if (!*disconnect) {
        switch(opCode) {
        case OP_SNAPSHOT:
        case OP_QUERY:
            rc = msgBuildReply(ppResultBuffer,
                        pResultBufferSize,
                        rc, &retObj);
        default:
            rc = msgBuildReply(ppResultBuffer,
                        pResultBufferSize,
                        rc, NULL);
        }
        return rc;
    }
error:
    switch(rc) {
    case EDB_INVALIDARG: 
        PD_LOG ( PDERROR,
        "Invalid argument is received") ;
        break;
    case EDB_IXM_ID_NOT_EXIST :
        PD_LOG ( PDERROR,
               "Record does not exist" ) ;
        break;
    default :
        PD_LOG (PDERROR,
               "System error, rc = %d", rc ) ;
        break;
    }
    goto done;
}

int pmdAgentEntryPoint(pmdEDUCB *cb, void *arg) {
    int rc = EDB_OK;
    char *pRecvBuffer = NULL;
    char *pResultBuffer = NULL;
    int packetLen;
    int socketfd;
    int newSize;
    EDUID id = cb->getID();
    pmdEDUMgr *mgr = cb->getEDUMgr();
    bool disconnect = false;
    int recvBufferSize = PMD_AGENT_RECV_BUFFER_SZ;
    int resultBufferSize = PMD_AGENT_RECV_BUFFER_SZ;

    // receive socket from argument
    socketfd = *((int*)&arg);
    ossSocket socket(&socketfd);
    socket.disableNagle();

    // allocate memory for receive buffer
    if((pRecvBuffer = (char *)malloc(sizeof(char) * recvBufferSize)) == NULL) {
        rc = EDB_OOM;
        PD_LOG(PDERROR, "malloc pRecvBuffer error");
        goto done;
    }

    while(!disconnect) {
        //receive next packet
        rc = pmdRecv(pRecvBuffer, sizeof(int), &socket, cb);
        if (rc) {
            if (rc == EDB_APP_FORCED) {
                disconnect = true;
                continue;
            } else {
                goto error;
            }
        }
        packetLen = *(int*)(pRecvBuffer);
        PD_LOG(PDTRACE, "Received packet size =%d", packetLen);
        if (packetLen < (int)sizeof(int)) {
            rc = EDB_INVALIDARG;
            goto error;
        }
        // check if current receive buffer size is large enough for the package
        if (recvBufferSize < packetLen +1 ) {
            PD_LOG(PDTRACE, 
                "Received packet size is big, buffer size is %d, packetLen is %d", recvBufferSize, packetLen);
            newSize = ossRoundUpToMultipleX(packetLen + 1, EDB_PAGE_SIZE);
            if ((pRecvBuffer = (char *)realloc(pRecvBuffer, newSize)) == NULL) {
                rc = EDB_OOM;
                PD_LOG(PDERROR, "realloc pRecvBuffer error");
                goto error;
            }
            recvBufferSize = newSize;
        }
        // recv body
        rc = pmdRecv(&pRecvBuffer[sizeof(int)], packetLen-sizeof(int), &socket, cb);
        if (rc) {
            if (rc == EDB_APP_FORCED) {
                disconnect = true;
                continue;
            } else {
                goto error;
            }
        }
        pRecvBuffer[packetLen] = '\0';
        // now it's time to handle request, set the edu to running status
        if ((rc = mgr->activateEDU(id)) != EDB_OK) {
            goto error;
        }

        rc = pmdProcessAgentRequest(pRecvBuffer,
                                    packetLen,
                                    &pResultBuffer,
                                    &resultBufferSize,
                                    &disconnect,
                                    cb);
        if (rc) {
            PD_LOG(PDERROR, "Error processing Agent request, rc = %d", rc);
            goto error;
        }

        if (!disconnect) {
            rc = pmdSend(pResultBuffer, *(int*)resultBufferSize, &socket, cb);
            if (rc) {
                if (rc == EDB_APP_FORCED) {

                } else {
                    goto error;
                }
            }
        }
        // change the status to running
        if ((rc = mgr->waitEDU(id)) != EDB_OK) {
            goto error;
        }
    }
done:
    if (pRecvBuffer) {
        free(pRecvBuffer);
    }
    if (pResultBuffer) {
        free(pResultBuffer);
    }
    socket.close();
    return rc;
error:
    switch ( rc ) {
        case EDB_SYS :
            PD_LOG (PDCRITICAL,
              "EDU id %d cannot be found", id ) ;
            break ;
        case EDB_INVALIDARG :
            PD_LOG (PDCRITICAL,
                  "Invalid argument receieved by agent") ;
            break ;
        case EDB_OOM :
            PD_LOG (PDCRITICAL,
                  "Failed to allocate memory by agent") ;
            break ;
        case EDB_NETWORK :
            PD_LOG (PDCRITICAL,
                  "Network error occured") ;
            break ;
        case EDB_NETWORK_CLOSE :
            PD_LOG (PDTRACE,
                  "Remote connection closed" ) ;
            rc = EDB_OK ;
            break ;
        default :
            PD_LOG (PDCRITICAL,
                  "Internal error") ;
    }
    goto done;
}