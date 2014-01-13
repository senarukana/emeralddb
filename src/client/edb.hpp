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
#ifndef _EDB_HPP__
#define _EDB_HPP__

#include "core.hpp"
#include "ossSocket.hpp"
#include "commandFactory.hpp"

const int CMD_BUFFER_SIZE = 512;
class Edb {
public:
   Edb(){}
   ~Edb(){};
   void start();
protected:
   void prompt();
private:
   void _split(const std::string &text, char delim, std::string &command, std::vector< std::string> &optionVec) ;
   int _readInput();
private:
   ossSocket      _socket;
   CommandFactory _cmdFactory;
   char           _cmdBuffer[CMD_BUFFER_SIZE];
};

#endif