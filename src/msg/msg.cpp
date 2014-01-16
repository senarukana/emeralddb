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
#include "msg.hpp"
#include "core.hpp"
#include "pd.hpp"

using namespace bson;
static int msgCheckBuffer(char **ppBuffer, int *pBufferSize, int length) {
   int rc = EDB_OK;
   if (length > *pBufferSize) {
      char *pOldBuf = *ppBuffer;
      if (length < 0) {
         PD_LOG(PDERROR, "invalid length: %d", length);
         rc = EDB_INVALIDARG;
         goto error;
      }
      PD_LOG(PDTRACE, "Realloc buffer size %d", *pBufferSize);
      *ppBuffer = (char *)realloc(*ppBuffer, sizeof(char)*length);
      if (!*ppBuffer) {
         PD_LOG(PDERROR, "Failed to allocate %d bytes buffer", length);
         rc = EDB_OOM;
         *ppBuffer = pOldBuf;
         goto error;
      }
      *pBufferSize = length;
   }
done:
   return rc;
error:
   goto done;
}

// int msgBuildInsert(char **ppBuffer, int *pBufferSize, std::vector<BSONObj> vecBson) {
//    int rc = EDB_OK;
   
// }

int msgBuildInsert(char **ppBuffer, int *pBufferSize, BSONObj &obj) {
   int rc = EDB_OK;
   int size = sizeof(MsgInsert) + obj.objsize();
   MsgInsert *pInsert = NULL;

   rc = msgCheckBuffer(ppBuffer, pBufferSize, size);
   PD_RC_CHECK(rc, PDERROR, "Failed to realloc buffer for %d bytes, rc = %d",
               size , rc);

   // buffer is allocated, let's assign variables
   pInsert = (MsgInsert*)(*ppBuffer);
   pInsert->header.msgLen = size;
   pInsert->header.opCode = OP_INSERT;
   pInsert->numInsert = 1;
   memcpy(&pInsert->data[0], obj.objdata(), obj.objsize());
done:
   return rc;
error:
   goto done; 
}

int msgBuildQuery(char **ppBuffer, int *pBufferSize, bson::BSONObj &key) {
   int rc = EDB_OK;
   int size = sizeof(MsgQuery) + key.objsize();
   MsgQuery *pQuery = NULL;

   rc = msgCheckBuffer(ppBuffer, pBufferSize, size);
   PD_RC_CHECK(rc, PDERROR, "Failed to realloc buffer for %d bytes, rc = %d",
               size , rc);

   pQuery = (MsgQuery*)(*ppBuffer);
   pQuery->header.msgLen = size;
   pQuery->header.opCode = OP_QUERY;
   memcpy(&pQuery->key[0], key.objdata(), key.objsize());
done:
   return rc;
error:
   goto done;
}

int msgBuildDelete(char **ppBuffer, int *pBufferSize, bson::BSONObj &key) {
   int rc = EDB_OK;
   int size = sizeof(MsgDelete) + key.objsize();
   MsgDelete *pDelete = NULL;

   rc = msgCheckBuffer(ppBuffer, pBufferSize, size);
   PD_RC_CHECK(rc, PDERROR, "Failed to realloc buffer for %d bytes, rc = %d",
               size , rc);

   pDelete = (MsgDelete*)(*ppBuffer);
   pDelete->header.msgLen = size;
   pDelete->header.opCode = OP_DELETE;
   memcpy(&pDelete->key[0], key.objdata(), key.objsize());
done:
   return rc;
error:
   goto done;
}

int msgBuildReply(char **ppBuffer, int *pBufferSize,
                  int returnCode, bson::BSONObj *objReturn) {
   int rc = EDB_OK;
   int size = sizeof(MsgReply);
   MsgReply *pReply = NULL;

   if(objReturn) {
      size += objReturn->objsize();
   }
   // PD_LOG(PDERROR, "pd buffer size %d, %d", *pBufferSize, size);
   rc = msgCheckBuffer(ppBuffer, pBufferSize, size);
   PD_RC_CHECK(rc, PDERROR, "Failed to realloc buffer for %d bytes, rc = %d",
               size , rc);
   // buffer is allocated, let's assign variables
   pReply = (MsgReply *)(*ppBuffer);
   pReply->header.msgLen = size;
   pReply->header.opCode = OP_REPLY;
   pReply->returnCode = returnCode;
   pReply->numReturn = (objReturn)?1:0;
   // PD_LOG(PDERROR, "ok");
   //bson object
   if (objReturn) {
      memcpy(&pReply->data[0], objReturn->objdata(), objReturn->objsize());
   }
done:
   return rc;
error:
   goto done;
}

int msgExtractReply(char *pBuffer, int &returnCode, int &numReturn,
                  const char **ppObjStart) {
   int rc = EDB_OK;
   MsgReply *pReply = (MsgReply*)pBuffer;
   // sanity check for header
   if (pReply->header.msgLen < (int)sizeof(MsgReply)) {
      PD_LOG(PDERROR, "Invalid length of reply message");
      rc = EDB_INVALIDARG;
      goto error;
   }

   returnCode = pReply->returnCode;
   numReturn = pReply->numReturn;
   if (numReturn == 0) {
      *ppObjStart = NULL;
   } else {
      *ppObjStart = &pReply->data[0];
   }
done:
   return rc;
error:
   goto done;
}

int msgExtractInsert(char *pBuffer, int &numInsert, const char **ppObjStart) {
   int rc = EDB_OK;
   MsgInsert *pInsert = (MsgInsert*)pBuffer;
   // sanity check for header
   if ( pInsert->header.msgLen < (int)sizeof(MsgInsert)){
      PD_LOG ( PDERROR, "Invalid length of insert message" ) ;
      rc = EDB_INVALIDARG ;
      goto error ;
   }
   // sanity check for opCode
   if ( pInsert->header.opCode != OP_INSERT ) {
      PD_LOG ( PDERROR, "non-insert code is received: %d, expected %d",
               pInsert->header.opCode, OP_INSERT ) ;
      rc = EDB_INVALIDARG ;
      goto error ;
   }
   numInsert = pInsert->numInsert;
   if (numInsert == 0) {
      *ppObjStart = NULL; 
   } else {
      *ppObjStart = &pInsert->data[0];
   }
done :
   return rc ;
error :
   goto done ;
}

int msgExtractDelete(char *pBuffer, bson::BSONObj &key) {
   int rc = EDB_OK;
   MsgDelete *pDelete = (MsgDelete*)pBuffer;
   // sanity check for header
   if ( pDelete->header.msgLen < (int)sizeof(MsgDelete)){
      PD_LOG ( PDERROR, "Invalid length of insert message" ) ;
      rc = EDB_INVALIDARG ;
      goto error ;
   }
   // sanity check for opCode
   if ( pDelete->header.opCode != OP_DELETE ) {
      PD_LOG ( PDERROR, "non-delete code is received: %d, expected %d",
               pDelete->header.opCode, OP_DELETE ) ;
      rc = EDB_INVALIDARG ;
      goto error ;
   }
   key = BSONObj(&pDelete->key[0]);
done :
   return rc ;
error :
   goto done ;
}

int msgExtractQuery(char *pBuffer, bson::BSONObj &key) {
   int rc = EDB_OK;
   MsgQuery *pQuery = (MsgQuery*)pBuffer;
   // sanity check for header
   if ( pQuery->header.msgLen < (int)sizeof(MsgQuery)){
      PD_LOG ( PDERROR, "Invalid length of insert message" ) ;
      rc = EDB_INVALIDARG ;
      goto error ;
   }
   // sanity check for opCode
   if ( pQuery->header.opCode != OP_QUERY ) {
      PD_LOG ( PDERROR, "non-query code is received: %d, expected %d",
               pQuery->header.opCode, OP_QUERY ) ;
      rc = EDB_INVALIDARG ;
      goto error ;
   }
   key = BSONObj(&pQuery->key[0]);
done :
   return rc ;
error :
   goto done ;
}

