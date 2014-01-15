
#ifndef PMD_COMMAND_HPP__
#define PMD_COMMAND_HPP__

#include "pmdEDU.hpp"
#include "bson.h"

typedef int (*pmdCommandFunc)(char *pReceiveBuffer,
                        int packetSize,
                        char **ppResultBuffer,
                        int *pResultBufferSize,
                        bool *disconnect,
                        pmdEDUCB *cb,
                        bson::BSONObj* obj);

class pmdCommand {
private:
    std::map<int, pmdCommandFunc> _commandMap;

public:
    void init();
    pmdCommandFunc getCommand(int opCode);
};

int pmdInsertCommand(char *pReceiveBuffer,
                        int packetSize,
                        char **ppResultBuffer,
                        int *pResultBufferSize,
                        bool *disconnect,
                        pmdEDUCB *cb,
                        bson::BSONObj* obj);

int pmdDeleteCommand(char *pReceiveBuffer,
                        int packetSize,
                        char **ppResultBuffer,
                        int *pResultBufferSize,
                        bool *disconnect,
                        pmdEDUCB *cb,
                        bson::BSONObj* obj);

#endif