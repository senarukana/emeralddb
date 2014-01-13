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
#ifndef PMD_EDU_EVENT_HPP__
#define PMD_EDU_EVENT_HPP__

#include "core.hpp"
enum pmdEDUEventTypes {
    PMD_EDU_EVENT_NONE = 0,
    PMD_EDU_EVENT_TERM,
    PMD_EDU_EVENT_RESUME,
    PMD_EDU_EVENT_ACTIVE,
    PMD_EDU_EVENT_DEACTIVE,
    PMD_EDU_EVENT_MSG,
    PMD_EDU_EVENT_TIMEOUT,
    PMD_EDU_EVENT_LOCKWAKEUP
};

class pmdEDUEvent {
public:
    pmdEDUEvent():
    _eventType(PMD_EDU_EVENT_NONE),
    _release(false),
    _Data(NULL)
    {}

    pmdEDUEvent(pmdEDUEventTypes type):
    _eventType(type),
    _release(false),
    _Data(NULL)
    {}

    pmdEDUEvent(pmdEDUEventTypes type, bool release, void *data):
    _eventType(type),
    _release(_release),
    _Data(data)
    {}

    void reset() {
        _eventType = PMD_EDU_EVENT_NONE ;
        _release = false ;
        _Data = NULL ;    
    }

    pmdEDUEventTypes    _eventType;
    bool                _release;
    void                *_Data;
};

#endif