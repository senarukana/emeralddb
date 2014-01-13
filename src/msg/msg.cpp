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
   if (rc) {
      PD_LOG(PDERROR, "Failed to realloc buffer for %d bytes, rc = %d",
               size, rc);
      goto error;
   }

   // buffer is allocated, let's assign variables
   pInsert = (MsgInsert*)(*pBufferSize);
   pInsert->header.msgLen = size;
   pInsert->header.opCode = OP_INSERT;
   pInsert->numInsert = 1;
   memcpy(pInsert->data, obj.objdata(), obj.objsize());
done:
   return rc;
error:
   goto done; 
}