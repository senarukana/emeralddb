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
#include "edb.hpp"
#include "commandFactory.hpp"
#include "pd.hpp"
#include "error.hpp"
#include <iostream>
#include <sstream>

const char  SPACE       = ' ';
const char  TAB         = '\t';
const char  BACK_SLANT  = '\\';
const char  NEW_LINE    = '\n';
const char *PROMPT      = "emeralddb >";

int gQuit = 0;

void Edb::start() {
   std::cout<<"Welcome to EmeraldDB Shell!"<<std::endl;
   std::cout<<"edb help for help. Ctrl+c or quit to exit"<<std::endl;
   while (gQuit == 0) {
      prompt();
   }
}

void Edb::prompt() {
   std::string command;
   std::vector<std::string> optionVec;
   ICommand *pCmd = NULL;
   int rc = EDB_OK;

   rc = _readInput();
   std::string input = _cmdBuffer;

   _split(input, SPACE, command, optionVec);
   pCmd = _cmdFactory.getCommandProcessor(command);
   if (pCmd == NULL) {
      getError(EDB_INVALID_COMMAND);
   } else {
      rc = pCmd->execute(_socket, optionVec);
      getError(rc);
   }
}

void Edb::_split(const std::string &text, char delim, std::string &command, std::vector< std::string> &optionVec) {
   int i;
   string option;
   int len = text.size();
   bool first = true;
   int start = 0;

   for (i = 0; i < len; i++) {
      if (text[i] == delim) {
         if (first) {
            command = text.substr(0, i);
            first = false;
         } else {
            option = text.substr(start, i - start);
            optionVec.push_back(option);
         }
         start = i + 1;
      }
   } 
   // last one
   option = text.substr(start, len - start);
   optionVec.push_back(option);
   return;
}

// The input will store in _cmdBuffer
// if input is too long, return EDB_INVALID_COMMAND
// else return EDB_OK
int Edb::_readInput() {
   int i = 0;
   char c;
   memset(_cmdBuffer, 0 , CMD_BUFFER_SIZE);
   std::cout<<PROMPT;
   while ((c = getchar()) != NEW_LINE && i < CMD_BUFFER_SIZE) {
      switch(c) {
         case BACK_SLANT: 
            if ((c = getchar()) != NEW_LINE) {
               return EDB_INVALID_COMMAND;
            }
            continue;
         case TAB:   //convert tab to space
            _cmdBuffer[i++] = SPACE;
            break;
         default:
            _cmdBuffer[i++] = c;
      }
   }
   if (i >= CMD_BUFFER_SIZE) {
      return EDB_ARGUMENT_TOO_LONG;
   }
   _cmdBuffer[i] = '\0';
   std::cout<<i<<_cmdBuffer<<std::endl;
   return EDB_OK;
}

int main(int argc, char **argv) {
   Edb edb;
   edb.start();
   return 0;
}