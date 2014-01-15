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
#include "ossSocket.hpp"
#include "pmd.hpp"
#include "pmdEDUMgr.hpp"
#include "pd.hpp"

#define PMD_TCPLISTENER_RETRY 5
#define OSS_MAX_SERVICENAME NI_MAXSERV

int pmdTcpListenerEntryPoint(pmdEDUCB *cb, void *arg) {
    int rc = EDB_OK;
    pmdEDUMgr *mgr = cb->getEDUMgr();
    EDUID     id = cb->getID();
    unsigned int retry = 0;
    EDUID agentEDU = PMD_INVALID_EDUID;

    while (retry <= PMD_TCPLISTENER_RETRY && !EDB_IS_DB_DOWN) {
        retry++;
        ossSocket socket(pmdGetKRCB()->getSvcName());
        rc = socket.init();
        PD_RC_CHECK(rc, PDERROR, "Failed to initilize socket rc = %d", rc);
        rc = socket.bind_listen();
        PD_RC_CHECK(rc,PDERROR, "Failed to bing socket server, rc = %d", rc);
        PD_LOG(PDEVENT, "Listening on port %d\n", socket.getLocalPort());

        if ((rc = mgr->activateEDU(id)) != EDB_OK) {
            goto error;
        }
        // master loop for tcp listener
        while (!EDB_IS_DB_DOWN) {
            int c;
            rc = socket.accept(&c, NULL, NULL);
            // if we don't get anything for a perioud of time, lets loop
            if (rc == EDB_TIMEOUT) {
                rc = EDB_OK;
                continue;
            }
            // if we receive error due to database down, we finish
            if (rc && EDB_IS_DB_DOWN) {
                rc = EDB_OK;
                goto done;
            } else if (rc) {
                PD_LOG(PDERROR, "Failed to accept socket in TcpListener");
                sleep(1);
                PD_LOG(PDEVENT, "Restarting socket to listen");
                break;
            }

            // assign the socket to the arg
            void *pData = NULL;
            *((int*)&pData) = c;
            // move the status to running
            rc = mgr->startEDU(EDU_TYPE_AGENT, pData, &agentEDU);
            if (rc) {
                if (rc == EDB_QUIESCED) {
                    // we can't start EDU due to quiesced
                    PD_LOG(PDWARNING, "Reject new connection due to quiesced database");
                } else {
                    PD_LOG(PDERROR, "Failed to start EDU agent");
                }
                // close remote connection if we can't create new thread
                ossSocket newSock(&c);
                newSock.close();
                continue;
            } 
        }
        // move the status to wait
        if ((rc = mgr->waitEDU(id)) != EDB_OK) {
            goto error;
        }
    }
done:
    return rc;
error:
    switch(rc) {
        case EDB_SYS:
            PD_LOG(PDCRITICAL, "System error occured");
            break;
        default:
            PD_LOG(PDCRITICAL, "Internal error");
    }
    goto done;
}