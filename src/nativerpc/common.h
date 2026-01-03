/**
 *  Native RPC Common
 *
 *      CONFIG_NAME
 *      COMMON_TYPES
 *
 *      SchemaInfo
 *      FieldInfo
 *      MethodInfo
 *      Options
 *      Connection
 *      Service
 *
 *      verifyPython
 *      getProjectName
 *      getProjectPath
 *      getEntryPoint
 *      getMessageFiles
 *      parseSchemaList
 *      getShellId
 */

#pragma once
#pragma pack(1)
#include "extension.h"
#include <assert.h>
#include <iostream>
#include <map>
#include <nlohmann/json.hpp>
#include <thread>

#ifdef NATIVERPC_EXPORTS
#define DLL_EXPORT __declspec(dllexport)
#else
#define DLL_EXPORT __declspec(dllimport)
#endif

namespace nativerpc {

extern std::string DLL_EXPORT CONFIG_NAME;
extern std::map<std::string, ClassType> DLL_EXPORT COMMON_TYPES;

class DLL_EXPORT SchemaInfo {
  public:
    std::string projectName;
    std::string className;
    std::string fieldName;
    std::string fieldType;
    std::string methodName;
    std::string methodRequest;
    std::string methodResponse;
    int idNumber;

    SchemaInfo();
    SchemaInfo(std::string projectName, std::string className, std::string fieldName, std::string fieldType, int idNumber);
    SchemaInfo(std::string projectName, std::string className, std::string methodName, std::string methodRequest, std::string methodResponse, int idNumber);
};

class DLL_EXPORT FieldInfo {
  public:
    std::string className;
    ClassType classType;
    std::string fieldName;
    ClassType fieldType;
    int idNumber;

    FieldInfo(std::string className, ClassType classType, std::string fieldName, ClassType fieldType, int idNumber);
};

class DLL_EXPORT MethodInfo {
  public:
    std::string className;
    Any *classInstance;
    std::string methodName;
    int methodIndex;
    AnyMethod methodPointer;
    ClassType methodRequest;
    ClassType methodResponse;
    int idNumber;

    MethodInfo();
    MethodInfo(std::string className, Any *classInstance, std::string methodName, int methodIndex, ClassType methodRequest, ClassType methodResponse, int idNumber);
    MethodInfo(std::string className, Any *classInstance, std::string methodName, AnyMethod methodPointer, ClassType methodRequest, ClassType methodResponse, int idNumber);
};

class DLL_EXPORT Options {
  public:
    Options(nlohmann::json options);

  private:
    nlohmann::json _options;
};

class DLL_EXPORT Connection {
  public:
    int connectionId{0};
    int socket{0};
    std::tuple<std::string, int> address;
    int64_t stime{0};
    int64_t wtime{0};
    std::vector<uint8_t> readBuffer;
    std::string projectId;
    bool closed{false};
    std::string error;
    int errorCode{0};
    int messageCount{0};
    std::string senderId;
    std::string callId;
    std::string processId;
    std::string parentId;
    std::string shellId;
    std::string entryPoint;

    Connection(int connectionId, int socket, std::tuple<std::string, int> address, int64_t stime, int64_t wtime, std::string projectId, std::string callId);
};

class DLL_EXPORT ServiceHolder {
  public:
    ServiceHolder();
    ~ServiceHolder();
  
  public:
    AnyMethod *_virtualTable;
    class Client *_parent;
    std::vector<MethodInfo> _methodList;
};

template <class T> class Service : public T {
  public:
    void close() {
        assert(_parent);
        assert(_parent == _parent->_self);
        _parent->close();
    }
    nlohmann::json getMetadata(nlohmann::json param) {
        assert(_parent);
        assert(_parent == _parent->_self);
        std::cout<<"5555 "<<param<<std::endl;
        return _parent->clientCall("Metadata", "getMetadata", param);
    }
  public:
    class Client *_parent;
};

DLL_EXPORT void verifyPython();
DLL_EXPORT std::string getProjectName();
DLL_EXPORT std::string getProjectPath();
DLL_EXPORT std::string getEntryPoint();
DLL_EXPORT std::vector<std::string> getMessageFiles(std::string projectPath);
DLL_EXPORT std::vector<SchemaInfo> parseSchemaList(std::string file);
DLL_EXPORT std::string getShellId();

} // namespace nativerpc
