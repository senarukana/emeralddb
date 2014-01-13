#include "core.hpp"
#include "error.hpp"


void getError(int code) {
    switch(code) {
        case EDB_OK:
            std::cout<<"+OK"<<std::endl;
        case EDB_IO:
            std::cout<<"io error is occurred"<<std::endl;
            break;
        case EDB_INVALID_COMMAND:
            std::cout<<"command not known"<<std::endl;
            break;
        case EDB_INVALIDARG:
            std::cout<<"invalid argument"<<std::endl;
            break;
        case EDB_PERM:
            std::cout << "edb_perm" << std::endl;
            break;
        case EDB_OOM:
            std::cout << "edb_oom" << std::endl;
            break;
        case EDB_SYS:
            std::cout << "system error is occurred." << std::endl;
            break;
        case EDB_NETWORK_CLOSE:
            std::cout << "net work is closed." << std::endl;
            break;
        case EDB_NO_ID:
            std::cout << "_id is needed" << std::endl;
            break;
        case EDB_QUERY_INVALID_ARGUMENT:
            std::cout << "invalid query argument" << std::endl;
            break;
        case EDB_INSERT_INVALID_ARGUMENT:
            std::cout <<  "invalid insert argument" << std::endl;
            break;
        case EDB_DELETE_INVALID_ARGUMENT:
            std::cout << "invalid delete argument" << std::endl;
            break;
        case EDB_INVALID_RECORD:
            std::cout << "invalid record string" << std::endl;
            break;
        case EDB_SOCKET_NOT_CONNECT:
            std::cout << "sock connection does not exist" << std::endl;
            break;
        case EDB_SOCKET_REMOTE_CLOSED:
            std::cout << "remote sock connection is closed" << std::endl;
            break;
        case EDB_MSG_BUILD_FAILED:
            std::cout << "msg build failed" << std::endl;
            break;
        case EDB_SOCKET_SEND_FAILED:
            std::cout << "sock send msg faild" << std::endl;
            break;
        case EDB_SOCKET_INIT_FAILED:
            std::cout << "sock init failed" << std::endl;
            break;
        case EDB_SOCKET_CONNECT_FAILED:
            std::cout << "sock connect remote server failed" << std::endl;
            break;
        default :
            break;
    }
}

