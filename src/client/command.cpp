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
#include "msg.hpp"
#include "command.hpp"
#include "commandFactory.hpp"
#include "error.hpp"
#include "pd.hpp"

COMMAND_BEGIN
COMMAND_ADD(COMMAND_INSERT, InsertCommand);
COMMAND_ADD(COMMAND_CONNECT, ConnectCommand);
COMMAND_ADD(COMMAND_DELETE, DeleteCommand);
COMMAND_ADD(COMMAND_QUERY, QueryCommand);
COMMAND_END


extern int gQuit;

int ICommand::execute(ossSocket & socket, std::vector<std::string> & argVec )
{
   return EDB_OK;
}

int ICommand::recvReply(ossSocket &socket) {
    int length = 0;
    int rc = EDB_OK;
    memset(_recvBuf, 0, RECV_BUF_SIZE);
    if (!socket.isConnected()) {
        return EDB_SOCKET_NOT_CONNECT;
    }
    for (;;) {
        // first read the length of the data
        rc = socket.recv(_recvBuf, sizeof(int));
        if (rc == EDB_OK) {
            break;
        } else if (rc == EDB_TIMEOUT) {
            continue;
        } else if (rc == EDB_NETWORK_CLOSE) {
            return EDB_SOCKET_REMOTE_CLOSED;
        } else {
            PD_LOG(PDERROR, "recv reply error, rc = %d", rc);
            return EDB_INTERNAL_ERROR;
        }
    }

    length = *(int *)_recvBuf;
    if (length > RECV_BUF_SIZE) {
        return EDB_RECV_DATA_LENGTH_ERROR;
    }

    for (;;) {
        rc = socket.recv(&_recvBuf[sizeof(int)], length - sizeof(int));
         if (rc == EDB_OK) {
            break;
        } else if (rc == EDB_TIMEOUT) {
            continue;
        } else if (rc == EDB_NETWORK_CLOSE) {
            return EDB_SOCKET_REMOTE_CLOSED;
        } else {
            PD_LOG(PDERROR, "recv reply error, rc = %d", rc);
            return EDB_INTERNAL_ERROR;
        }
    }
    return EDB_OK;
}

int ICommand::sendMsg(ossSocket &socket, OnMsgBuild onMsgBuild) {
    int rc = EDB_OK;
    bson::BSONObj bsonData;
    try {
        bsonData = bson::fromjson(_jsonString);
    } catch (std::exception &e) {
        return EDB_INVALID_RECORD;
    }
    memset(_sendBuf,0,SEND_BUF_SIZE);
    int size = SEND_BUF_SIZE;
    char *pSendBuf = _sendBuf;
    rc = onMsgBuild(&pSendBuf, &size, bsonData);
    if (rc) {
        return EDB_MSG_BUILD_FAILED;
    }

    rc = socket.send(pSendBuf, *(int *)pSendBuf);
    if (rc) {
        return EDB_SOCKET_SEND_FAILED;
    }
    return EDB_OK;
}

/******************************InsertCommand**********************************************/

int InsertCommand::handleReply() {
    MsgReply *msg = (MsgReply*)_recvBuf;
    return msg->returnCode;
}

int InsertCommand::execute(ossSocket &socket, std::vector<std::string> &argVec) {
    int rc = EDB_OK;
    if (argVec.size() != 1) {
        return EDB_INSERT_INVALID_ARGUMENT;
    }
    _jsonString = argVec[0];
    if (!socket.isConnected()) {
        return EDB_SOCKET_NOT_CONNECT;
    }
    if ((rc = sendMsg(socket, msgBuildInsert)) < 0) {
        return rc;
    }

    if ((rc = recvReply(socket)) < 0) {
        return rc;
    }

    if ((rc = handleReply()) < 0) {
        return rc;
    }
    return rc;
}

/******************************DeleteCommand**********************************************/

int DeleteCommand::handleReply() {
    MsgReply *msg = (MsgReply*)_recvBuf;
    return msg->returnCode;
}

int DeleteCommand::execute(ossSocket &socket, std::vector<std::string> &argVec) {
    int rc = EDB_OK;
    if (argVec.size() != 1) {
        return EDB_DELETE_INVALID_ARGUMENT;
    }
    _jsonString = argVec[0];
    if (!socket.isConnected()) {
        return EDB_SOCKET_NOT_CONNECT;
    }
    if ((rc = sendMsg(socket, msgBuildDelete)) < 0) {
        return rc;
    }

    if ((rc = recvReply(socket)) < 0) {
        return rc;
    }

    if ((rc = handleReply()) < 0) {
        return rc;
    }
    return rc;
}

/******************************DeleteCommand**********************************************/

int QueryCommand::handleReply() {
    MsgReply *msg = (MsgReply*)_recvBuf;
    int returnCode = msg->returnCode;
    if (returnCode == EDB_OK) {
        bson::BSONObj bsonData = bson::BSONObj(&(msg->data[0]));
        std::cout<<bsonData.toString()<<std::endl;
    }
    return msg->returnCode;
}

int QueryCommand::execute(ossSocket &socket, std::vector<std::string> &argVec) {
    int rc = EDB_OK;
    if (argVec.size() != 1) {
        return EDB_DELETE_INVALID_ARGUMENT;
    }
    _jsonString = argVec[0];
    if (!socket.isConnected()) {
        return EDB_SOCKET_NOT_CONNECT;
    }
    if ((rc = sendMsg(socket, msgBuildDelete)) < 0) {
        return rc;
    }

    if ((rc = recvReply(socket)) < 0) {
        return rc;
    }

    if ((rc = handleReply()) < 0) {
        return rc;
    }
    return rc;
}

/******************************ConnectCommand****************************************/
int ConnectCommand::execute(ossSocket &socket, std::vector<std::string> &argVec) {
    int rc = EDB_OK;
    if (argVec.size() != 2) {
        return EDB_CONNECT_INVALID_ARGUMENT;
    }
    _address = argVec[0];
    _port = atoi(argVec[1].c_str());
    // in case it's already open
    socket.close();
    socket.setAddress(_address.c_str(), _port);
    rc = socket.init();
    if (rc) {
        return EDB_SOCKET_INIT_FAILED;
    }
    rc = socket.connect();
    if (rc) {
        return EDB_SOCKET_CONNECT_FAILED;
    }
    socket.disableNagle();
    return rc;
}