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
#ifndef _COMMAND_HPP__
#define _COMMAND_HPP__

#include "core.hpp"
#include "ossSocket.hpp"
#include <bson/src/util/json.h>

#define COMMAND_QUIT       "quit"
#define COMMAND_QUERY      "query"
#define COMMAND_INSERT     "insert"
#define COMMAND_DELETE     "delete"
#define COMMAND_HELP       "help"
#define COMMAND_CONNECT    "connect"
#define COMMAND_TEST       "test"
#define COMMAND_SNAPSHOT   "snapshot"

#define RECV_BUF_SIZE      4096
#define SEND_BUF_SIZE      4096

class ICommand {
   typedef int (*OnMsgBuild) (char **ppBufer, int *pBufferSize, bson::BSONObj &obj);
public:
   virtual int execute(ossSocket &socket, std::vector<std::string> &argVec);
protected:
   int recvReply(ossSocket &socket);
   int sendMsg(ossSocket &socket, OnMsgBuild onMsgBuild);
   int sendMsg(ossSocket &socket, int opCode);
protected:
   virtual int handleReply() { return EDB_OK; }
protected:
   char _recvBuf[RECV_BUF_SIZE];
   char _sendBuf[SEND_BUF_SIZE];
   std::string _jsonString;
};

class InsertCommand : public ICommand {
public:
   int execute(ossSocket &socket, std::vector<std::string> &argVec);
protected:
   int handleReply();
};

class ConnectCommand : public ICommand {
public:
   int execute(ossSocket &socket, std::vector<std::string> &argVec);
private:
   std::string _address;
   int         _port;
};

class HelpCommand : public ICommand {
public:
   int execute(ossSocket &socket, std::vector<std::string> &argVec);
};

class QuitCommand : public ICommand {
public:
   int execute(ossSocket &socket, std::vector<std::string> &argVec);
};

#endif