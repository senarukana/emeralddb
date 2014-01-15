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
#define EDB_COMMAND_NOT_FOUND          -101
#define EDB_ARGUMENT_TOO_LONG          -102
#define EDB_INVALID_COMMAND            -103

#define EDB_QUERY_INVALID_ARGUMENT     -104
#define EDB_INSERT_INVALID_ARGUMENT    -105
#define EDB_DELETE_INVALID_ARGUMENT    -106
#define EDB_CONNECT_INVALID_ARGUMENT   -107


#define EDB_INVALID_RECORD             -201
#define EDB_RECV_DATA_LENGTH_ERROR     -202
#define EDB_MSG_BUILD_FAILED           -203

#define EDB_SOCKET_NOT_CONNECT         -301
#define EDB_SOCKET_INIT_FAILED         -302
#define EDB_SOCKET_CONNECT_FAILED      -303
#define EDB_SOCKET_REMOTE_CLOSED       -304
#define EDB_SOCKET_SEND_FAILED         -305

#define EDB_INTERNAL_ERROR             -401

void getError(int code);