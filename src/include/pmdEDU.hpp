
#ifndef PMDEDU_HPP__
#define PMDEDU_HPP__

#include "core.hpp"
#include "pmdEDUEvent.hpp"
#include "ossQueue.hpp"
#include "ossSocket.hpp"

#define PMD_INVALID_EUDID 0
#define PMD_IS_EDU_CREATING(x)          (PMD_EDU_CREATING == x )
#define PMD_IS_EDU_RUNNING(x)           (PMD_EDU_RUNNING == x)
#define PMD_IS_EDU_WAITTING(x)          (PMD_EDU_WAITTING == x)
#define PMD_IS_EDU_IDLE(x)              (PMD_EDU_IDLE == x)

#endif