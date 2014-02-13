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
#include "pmdCommand.hpp"
#include "pmdEDUMgr.hpp"
#include "msg.hpp"
#include "pd.hpp"

#include "bson.h"

using namespace std;
using namespace bson;


void pmdCommand::init() {
    _commandMap[OP_INSERT] = pmdInsertCommand;
    _commandMap[OP_DELETE] = pmdDeleteCommand;
    _commandMap[OP_QUERY] = pmdQueryCommand;
}

pmdCommandFunc pmdCommand::getCommand(int opCode) {
    std::map<int, pmdCommandFunc>::iterator it = _commandMap.find(opCode);
    if (it == _commandMap.end()) {
        return NULL;
    } else {
        return (*it).second;
    }
}

int pmdInsertCommand(char *pReceiveBuffer,
                        int packetSize,
                        pmdEDUCB *cb,
                        BSONObj* obj) {
    int rc = EDB_OK;
    int recordNum;
    const char *pInsertToBuffer = NULL;
    rtn *rtnMgr = pmdGetKRCB()->getRtnMgr();
    PD_LOG(PDEVENT, "Insert request received");
    rc = msgExtractInsert(pReceiveBuffer, recordNum, &pInsertToBuffer);
    if (rc) {
        PD_LOG(PDERROR, "Failed to read insert packet");
        rc = EDB_INVALIDARG;
        goto error;
    }
    try {
        BSONObj insertor(pInsertToBuffer);
        PD_LOG(PDEVENT, "Insert: insertor: %s",
            insertor.toString().c_str());
        // make suere _id is included
        BSONObjIterator it(insertor);
        BSONElement ele = *it;
        const char *tmp = ele.fieldName();
        rc = strcmp(tmp, gKeyFieldName);
        if (rc) {
            PD_LOG(PDERROR,
                    "First element in inserted record is not _id");
            rc = EDB_NO_ID;
            goto error;
        }
        //insert record
        rc = rtnMgr->rtnInsert(insertor);
        // if (!rc) {

        // }

    } catch(exception &e) {
        PD_LOG(PDERROR,
                "Failed to create insertor for insert: %s", e.what());
        rc = EDB_INVALIDARG;
        goto error;
    }
done:
    return rc;    
error:
    goto done;
}

int pmdDeleteCommand(char *pReceiveBuffer,
                        int packetSize,
                        pmdEDUCB *cb,
                        BSONObj* obj) {
    int rc = EDB_OK;
    rtn *rtnMgr = pmdGetKRCB()->getRtnMgr();
    BSONObj recordID;

    PD_LOG(PDTRACE, "Delete request received");

    rc = msgExtractDelete(pReceiveBuffer, recordID);
    if (rc) {
        PD_LOG(PDERROR, "Failed to read delete packet");
        rc = EDB_INVALIDARG;
        goto error;
    }
    PD_LOG(PDTRACE, "Delete condition: %s", recordID.toString().c_str());
    rc = rtnMgr->rtnRemove(recordID);
done:
    return rc;    
error:
    goto done;
}

int pmdQueryCommand(char *pReceiveBuffer,
                        int packetSize,
                        pmdEDUCB *cb,
                        BSONObj* obj) {
    int rc = EDB_OK;
    rtn *rtnMgr = pmdGetKRCB()->getRtnMgr();
    BSONObj recordID;
    BSONObj findObj;

    PD_LOG(PDTRACE, "Query request received");

    if (obj == nullptr) {
        rc = EDB_SYS;
        PD_LOG(PDERROR, "fatal error, query obj is null");
        goto error;
    }
    rc = msgExtractQuery(pReceiveBuffer, recordID);
    if (rc) {
        PD_LOG(PDERROR, "Failed to read query packet");
        rc = EDB_INVALIDARG;
        goto error;
    }
    PD_LOG(PDTRACE, "Query condition: %s", recordID.toString().c_str());
    rc = rtnMgr->rtnFind(recordID,findObj);
    *obj = findObj;
done:
    return rc;    
error:
    goto done;
}