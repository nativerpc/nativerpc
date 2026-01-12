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
 *      ServiceHolder
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
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifdef _WIN32
#include <windows.h>
#endif
#include <Ws2tcpip.h>
#include <winsock2.h>

#pragma pack(1)
#include "common.h"
#include <cstdlib>
#include <exception>
#include <fstream>
#include <iostream>
#include <nlohmann/json.hpp>
#include <nlohmann/json_fwd.hpp>
#include <sstream>
#include <stacktrace>
#include <string>

namespace nativerpc {

std::string CONFIG_NAME = "workspace.json";

std::map<std::string, ClassType> COMMON_TYPES = {
    {"int", ClassType("int", false, sizeof(int))},    
    {"float", ClassType("float", false, sizeof(float))},   
    {"str", ClassType("str", false, sizeof(std::string))},
    {"bool", ClassType("bool", false, sizeof(bool))}, 
    {"dict", ClassType("dict", false, sizeof(nlohmann::json))},
    {"list", ClassType("list", false, sizeof(nlohmann::json))},
};

SchemaInfo::SchemaInfo() {
    this->idNumber = -1;
}

SchemaInfo::SchemaInfo(std::string projectName, std::string className, std::string fieldName, std::string fieldType, int idNumber) {
    this->projectName = projectName;
    this->className = className;
    this->fieldName = fieldName;
    this->fieldType = fieldType;
    this->idNumber = idNumber;
}

SchemaInfo::SchemaInfo(std::string projectName, std::string className, std::string methodName, std::string methodRequest, std::string methodResponse, int idNumber) {
    this->projectName = projectName;
    this->className = className;
    this->methodName = methodName;
    this->methodRequest = methodRequest;
    this->methodResponse = methodResponse;
    this->idNumber = idNumber;
}

FieldInfo::FieldInfo(std::string className, ClassType classType, std::string fieldName, ClassType fieldType, int idNumber) {
    this->className = className;
    this->classType = classType;
    this->fieldName = fieldName;
    this->fieldType = fieldType;
    this->idNumber = idNumber;
}

MethodInfo::MethodInfo() {
    this->className = "";
    this->classInstance = 0;
    this->methodName = "";
    this->methodIndex = -1;
    this->methodPointer = nullptr;
    this->methodRequest = ClassType("", true, 0);
    this->methodResponse = ClassType("", true, 0);
    this->idNumber = -1;
}

MethodInfo::MethodInfo(std::string className, Any *classInstance, std::string methodName, int methodIndex, ClassType methodRequest, ClassType methodResponse, int idNumber) {
    this->className = className;
    this->classInstance = classInstance;
    this->methodName = methodName;
    this->methodIndex = methodIndex;
    this->methodPointer = nullptr;
    this->methodRequest = methodRequest;
    this->methodResponse = methodResponse;
    this->idNumber = idNumber;
}

MethodInfo::MethodInfo(std::string className, Any *classInstance, std::string methodName, AnyMethod methodPointer, ClassType methodRequest, ClassType methodResponse, int idNumber) {
    this->className = className;
    this->classInstance = classInstance;
    this->methodName = methodName;
    this->methodIndex = -1;
    this->methodPointer = methodPointer;
    this->methodRequest = methodRequest;
    this->methodResponse = methodResponse;
    this->idNumber = idNumber;
}

Options::Options(nlohmann::json options) {
    _options = options;
}

Connection::Connection(int connectionId, int socket, std::tuple<std::string, int> address, int64_t stime, int64_t wtime, std::string projectId, std::string callId) {
    this->connectionId = connectionId;
    this->socket = socket;
    this->address = address;
    this->stime = stime;
    this->wtime = wtime;
    this->projectId = projectId;
    this->callId = callId;
}

ServiceHolder::ServiceHolder() { 
    _virtualTable = nullptr;
    _methodList.clear();
    _parent = nullptr;
}

ServiceHolder::~ServiceHolder() {
    if (_virtualTable) {
        delete[] _virtualTable;
        _virtualTable = nullptr;
    }
    _methodList.clear();
    _parent = nullptr;
}

void verifyPython() {
    std::ostringstream cmd;
    cmd << "python --version";
    std::string resp;
    try {
        resp = execProcess(cmd.str(), ".");
    } catch (...) {
        throw std::exception("Missing python executable");
    }
    resp = replaceAll(resp, "Python ", "");
    auto parts = splitString(resp, ".");
    if (parts.size() != 3) {
        throw std::exception("Internal python error");
    }
    auto major = parseInt(parts[0]);
    if (major != 3) {
        throw std::exception("Internal python error");
    }
}

std::string getProjectName() { return std::filesystem::path(getProjectPath()).filename().string(); }

std::string getProjectPath() {
    std::error_code ec;
    std::filesystem::path path = std::filesystem::canonical("/proc/self/exe", ec);

#ifdef _WIN32
    if (ec.value()) {
        char exePath[MAX_PATH + 1];
        unsigned int len = GetModuleFileNameA(GetModuleHandleA(0x0), exePath, MAX_PATH);
        if (len == 0) {
            ec.assign(1, std::generic_category());
        } else {
            ec.assign(0, std::generic_category());
            path = std::string(exePath, len);
        }
    }
#endif
    assert(!ec.value());

    std::string parsed;
    auto it = path.parent_path();
    while (true) {
        if (std::filesystem::exists(it / "workspace.json")) {
            break;
        } else if (std::filesystem::exists(it / "CMakeLists.txt")) {
            std::ifstream file(it / "CMakeLists.txt");
            std::ostringstream resp;
            std::string line;
            while (std::getline(file, line)) {
                resp << line << std::endl;
                leftTrim(line);
                if (line.starts_with("project(")) {
                    int idx = line.find_last_of(")");
                    assert(idx != -1);
                    parsed = line.substr(strlen("project("), idx);
                    break;
                }
            }
            file.close();
            if (parsed.size()) {
                break;
            }
        }
        it = it.parent_path();
    }

    return it.string();
}

std::string getEntryPoint() {
    std::error_code ec;
    std::filesystem::path path = std::filesystem::canonical("/proc/self/exe", ec);

#ifdef _WIN32
    if (ec.value()) {
        char exePath[MAX_PATH + 1];
        unsigned int len = GetModuleFileNameA(GetModuleHandleA(0x0), exePath, MAX_PATH);
        if (len == 0) {
            ec.assign(1, std::generic_category());
        } else {
            ec.assign(0, std::generic_category());
            path = std::string(exePath, len);
        }
    }
#endif
    assert(!ec.value());
    return path.string();
}

std::vector<std::string> getMessageFiles(std::string projectPath) {
    std::vector<std::string> result;
    auto path = std::filesystem::path(projectPath) / "src" / "common.h";
    result.push_back(path.string());
    assert(std::filesystem::exists(result.back()));
    return result;
}

std::vector<SchemaInfo> parseSchemaList(std::string file) {
    assert(std::filesystem::exists(file));
    std::ostringstream cmd;
    std::filesystem::path p = __FILE__;
    assert(std::filesystem::exists((p.parent_path().parent_path().parent_path() / "src/nativerpc/parser.py").string()));
    std::string cwd = p.parent_path().parent_path().parent_path().string();
    cmd << "python src/nativerpc/parser.py \"" << file << "\"";
    std::string command_output = execProcess(cmd.str(), cwd);
    std::vector<SchemaInfo> result;
    for (auto item : nlohmann::json::parse(command_output)) {
        if (item.contains("methodName") && item["methodName"].get<std::string>().size()) {
            result.push_back(SchemaInfo(std::string(""), item["className"].get<std::string>(), item["methodName"].get<std::string>(), item["methodRequest"].get<std::string>(), item["methodResponse"].get<std::string>(), -1));
        } else {
            result.push_back(SchemaInfo("", item["className"].get<std::string>(), item["fieldName"].get<std::string>(), item["fieldType"].get<std::string>(), -1));
        }
    }
    return result;
}

std::string getShellId() {
    std::ostringstream cmd;
    cmd << "python -c \"import psutil; import os; print(':'.join([str(x.pid) for "
           "x in psutil.Process(os.getppid()).parent().parent().parents()]))\"";
    return execProcess(cmd.str(), ".");
}

} // namespace nativerpc