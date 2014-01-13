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
#include <ctype.h>
#include "commandFactory.hpp"

void _lowercase(string &str) {
   for (auto &c: str) {
      c = tolower(c);
   }
}

CommandFactory::CommandFactory() {
   addCommand();
}

ICommand *CommandFactory::getCommandProcessor(string &cmd) {
   ICommand *pProcessor = NULL;
   _lowercase(cmd);
   COMMAND_MAP::iterator iter;
   iter = _cmdMap.find(cmd);
   if (iter != _cmdMap.end()) {
      pProcessor = iter->second;
   }
   return pProcessor;
}