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
#include "pd.hpp"

#define PMD_TCPLISTENER_RETRY 5
#define OSS_MAX_SERVICENAME NI_MAXSERV

int pmdTcpListenerEntryPoint() {
    int rc = EDB_OK;
    int port = 48127;
    ossSocket socket(port);
    rc = socket.init();
    PD_RC_CHECK(rc, PDCRITICAL, "Failed to init socket server, rc = %d", rc);
    rc = socket.bind_listen();
    PD_RC_CHECK(rc, PDCRITICAL, "Failed to bind socket server, rc = %d", rc);

    // master loop for tcp listener
    for(;;) {
        int s; 
        rc = socket.accept(&s, NULL, NULL);
        if (EDB_TIMEOUT == rc) {
            rc = EDB_OK;
            continue;
        }
        char buffer[1024];
        int size;
        ossSocket sock1(&s);
        socket.disableNagle();
        do {
            rc = socket.recv((char*)&size, 4);
            if (rc && rc != EDB_TIMEOUT) {
                PD_RC_CHECK(rc, PDTRACE, "Failed to receive buffer size, rc = %d", rc);
            }
        } while(EDB_TIMEOUT == rc);
        do {
            rc = sock1.recv(&buffer[0], size- sizeof(int));
            if (rc && rc != EDB_TIMEOUT) {
                PD_RC_CHECK(rc, PDTRACE, "Failed to receive buffer, rc = %d", rc);
            }
        } while(EDB_TIMEOUT == rc);
        printf("%s\n", buffer);
        sock1.close();
    }
done:
    return rc;
error:
    goto done;
}