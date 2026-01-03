/**
 *  Native RPC Main
 *
 *      Serializer
 *          Serializer
 *          findType
 *          getFields
 *          getMethods
 *          getSize
 *          toJson
 *          fromJson
 *          createInstance
 *          destroyInstance
 *          createOrDestroy
 *
 *      Server
 *          Server
 *          listen
 *          startServer
 *          serverCall
 *          connectClient
 *          getMetadata
 *          closeClient
 *
 *      Client
 *          Client
 *          connect
 *          initSocket
 *          setupInstance
 *          clientCall
 *          close
 *          getProxyMethod
 *          proxyCall
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
#include "extension.h"
#include "main.h"
#include <cstdlib>
#include <exception>
#include <fstream>
#include <iostream>
#include <nlohmann/json.hpp>
#include <sstream>
#include <stacktrace>
#include <string.h>
#include <string>

namespace nativerpc {

Serializer::Serializer() {
    // Read settings
    verifyPython();
    auto projectPath = getProjectPath();

    // Read module list
    for (auto item : getMessageFiles(projectPath)) {
        assert(std::filesystem::exists(item));
    }

    // Read class metadata
    for (auto item : getMessageFiles(projectPath)) {
        for (auto item2 : parseSchemaList(item)) {
            _schemaList.push_back(item2);
        }
    }

    // Register types
    for (auto methodInfo : _schemaList) {
        if (methodInfo.methodName.size()) {
            if (COMMON_TYPES.find(methodInfo.methodRequest) == COMMON_TYPES.end() && _fieldList.find(methodInfo.methodRequest) == _fieldList.end()) {
                _fieldList[methodInfo.methodRequest] = getFields(methodInfo.methodRequest);
            }
            if (COMMON_TYPES.find(methodInfo.methodResponse) == COMMON_TYPES.end() && _fieldList.find(methodInfo.methodResponse) == _fieldList.end()) {
                _fieldList[methodInfo.methodResponse] = getFields(methodInfo.methodResponse);
            }
        } else {
            if (COMMON_TYPES.find(methodInfo.fieldType) == COMMON_TYPES.end() && _fieldList.find(methodInfo.fieldType) == _fieldList.end()) {
                _fieldList[methodInfo.fieldType] = getFields(methodInfo.fieldType);
            }
        }
    }
}

ClassType Serializer::findType(std::string name) {
    if (COMMON_TYPES.find(name) != COMMON_TYPES.end()) {
        return COMMON_TYPES[name];
    }
    return ClassType(name, true, 0);
}

std::vector<FieldInfo> Serializer::getFields(std::string className) {
    std::vector<FieldInfo> result;
    if (COMMON_TYPES.find(className) != COMMON_TYPES.end()) {
        throw std::exception(("Type mismatch: " + className).c_str());
    }
    auto classType = findType(className);
    if (classType.isComplex) {
        for (auto item : _schemaList) {
            if (item.className != className || !item.fieldName.size()) {
                continue;
            }
            result.push_back(FieldInfo(className, classType, item.fieldName, findType(item.fieldType), item.idNumber));

            assert(findType(item.fieldType).className.size());
        }
        if (!result.size()) {
            throw std::exception(("No fields found: " + className).c_str());
        }
    }
    return result;
}

std::vector<MethodInfo> Serializer::getMethods(std::string className, Any *classInstance) {
    std::vector<MethodInfo> result;
    auto methodIndex = -1;

    for (auto item : _schemaList) {
        if (item.className != className || !item.methodName.size()) {
            continue;
        }
        methodIndex++;
        result.push_back(MethodInfo(className, classInstance, item.methodName, methodIndex, findType(item.methodRequest), findType(item.methodResponse), item.idNumber));
        assert(findType(item.methodRequest).className.size());
        assert(findType(item.methodResponse).className.size());
    }
    assert(result.size());
    return result;
}

std::vector<MethodInfo> Serializer::getMethods(std::string className, Any *classInstance, std::map<std::string, AnyMethod> methods) {
    std::vector<MethodInfo> result;

    for (auto item : _schemaList) {
        if (item.className != className || !item.methodName.size()) {
            continue;
        }
        assert(methods.find(item.methodName) != methods.end());
        result.push_back(MethodInfo(className, classInstance, item.methodName, methods[item.methodName], findType(item.methodRequest), findType(item.methodResponse), item.idNumber));
        assert(findType(item.methodRequest).className.size());
        assert(findType(item.methodResponse).className.size());
    }
    assert(result.size());
    return result;
}

int Serializer::getSize(std::string typeName) {
    assert(_fieldList.size());
    if (typeName == "dict") {
        return sizeof(nlohmann::json);
    } else if (COMMON_TYPES.find(typeName) != COMMON_TYPES.end()) {
        if (typeName == "int") {
            return sizeof(int);
        } else if (typeName == "float") {
            return sizeof(float);
        } else if (typeName == "str") {
            return sizeof(std::string);
        } else if (typeName == "bool") {
            return sizeof(bool);
        } else {
            assert(false);
        }
    } else {
        assert(_fieldList.find(typeName) != _fieldList.end());
        int total = 0;
        for (auto &item : _fieldList[typeName]) {
            total += getSize(item.fieldType.className);
        }
        return total;
    }
}

nlohmann::json Serializer::toJson(std::string typeName, const Any *obj) {
    assert(_fieldList.size());
    assert(!typeName.starts_with("class "));
    nlohmann::json result;
    if (typeName == "dict") {
        result = nlohmann::json({});
        assert(_fieldList.find(typeName) == _fieldList.end());
        auto obj2 = reinterpret_cast<const nlohmann::json *>(obj);
        for (auto &kv : obj2->items()) {
            result[kv.key()] = kv.value();
        }
    } else if (COMMON_TYPES.find(typeName) != COMMON_TYPES.end()) {
        if (typeName == "int") {
            result = *(int *)obj;
        } else if (typeName == "float") {
            result = *(float *)obj;
        } else if (typeName == "str") {
            result = *(std::string *)obj;
        } else if (typeName == "bool") {
            result = *(bool *)obj;
        } else {
            assert(false);
        }
    } else {
        result = nlohmann::json({});
        auto data = reinterpret_cast<const uint8_t *>(obj);
        assert(_fieldList.find(typeName) != _fieldList.end());
        for (auto &item : _fieldList[typeName]) {
            result[item.fieldName] = toJson(item.fieldType.className, (const Any *)data);
            data += item.fieldType.dataSize;
        }
    }
    return result;
}

nlohmann::json Serializer::toJson(std::string typeName, std::vector<uint8_t> &obj) {
    assert(_fieldList.size());
    assert(obj.size() == getSize(typeName));
    return toJson(typeName, reinterpret_cast<Any *>(&obj[0]));
}

void Serializer::fromJson(std::string typeName, nlohmann::json data, Any *obj) {
    assert(_fieldList.size());
    if (typeName == "dict") {
        assert(_fieldList.find(typeName) == _fieldList.end());
        nlohmann::json &target = *reinterpret_cast<nlohmann::json *>(obj);
        for (auto &kv : data.items()) {
            target[kv.key()] = kv.value();
        }
    } else if (COMMON_TYPES.find(typeName) != COMMON_TYPES.end()) {
        assert(_fieldList.find(typeName) == _fieldList.end());
        if (typeName == "int") {
            *(int *)obj = data.get<int>();
        } else if (typeName == "float") {
            *(float *)obj = data.get<float>();
        } else if (typeName == "str") {
            *(std::string *)obj = data.get<std::string>();
        } else if (typeName == "bool") {
            *(bool *)obj = data.get<bool>();
        } else {
            assert(false);
        }
    } else {
        assert(_fieldList.find(typeName) != _fieldList.end());
        uint8_t *target = reinterpret_cast<uint8_t *>(obj);
        for (auto &item : _fieldList[typeName]) {
            if (!data.contains(item.fieldName)) {
                continue;
            }
            fromJson(item.fieldType.className, data[item.fieldName], (Any *)target);
            target += item.fieldType.dataSize;
        }
    }
}

void Serializer::fromJson(std::string typeName, nlohmann::json data, std::vector<uint8_t> &obj) {
    assert(_fieldList.size());
    assert(obj.size() == getSize(typeName));
    fromJson(typeName, data, reinterpret_cast<Any *>(&obj[0]));
}

void Serializer::createInstance(std::string className, std::vector<uint8_t> &data) {
    assert(_fieldList.size());
    int total = getSize(className);
    int powerOfTwo = 16;
    while (powerOfTwo < total) {
        powerOfTwo *= 2;
    }
    data.reserve(powerOfTwo);
    data.resize(total);
    createOrDestroy(true, className, &data[0]);
}

void Serializer::createInstance(std::string className, std::vector<uint8_t> &data, std::string secondaryClass) {
    assert(_fieldList.size());
    int total = getSize(className);
    int total2 = max(total, getSize(secondaryClass));
    int powerOfTwo = 16;
    while (powerOfTwo < total2) {
        powerOfTwo *= 2;
    }
    data.reserve(powerOfTwo);
    data.resize(total);
    createOrDestroy(true, className, &data[0]);
}

void Serializer::destroyInstance(std::string className, std::vector<uint8_t> &data) {
    assert(_fieldList.size());
    int total = getSize(className);
    assert(data.size() == total);
    createOrDestroy(false, className, &data[0]);
    data.resize(0);
}

void Serializer::destroyInstance(std::string className, Any *obj) {
    assert(_fieldList.size());
    int total = getSize(className);
    createOrDestroy(false, className, (uint8_t *)obj);
}

void Serializer::createOrDestroy(bool create, std::string typeName, uint8_t *data) {
    if (typeName == "dict") {
        if (create) {
            new (reinterpret_cast<nlohmann::json *>(data)) nlohmann::json({});
        } else {
            reinterpret_cast<nlohmann::json *>(data)->~basic_json();
        }
    } else if (COMMON_TYPES.find(typeName) != COMMON_TYPES.end()) {
        assert(_fieldList.find(typeName) == _fieldList.end());
        if (typeName == "int") {
            *reinterpret_cast<int *>(data) = 0;
        } else if (typeName == "float") {
            *reinterpret_cast<float *>(data) = 0;
        } else if (typeName == "str") {
            if (create) {
                new (reinterpret_cast<std::string *>(data)) std::string();
            } else {
                reinterpret_cast<std::string *>(data)->~basic_string();
            }
        } else if (typeName == "bool") {
            *reinterpret_cast<bool *>(data) = false;
        } else {
            assert(false);
        }
    } else {
        assert(_fieldList.find(typeName) != _fieldList.end());
        uint8_t *target = data;
        for (auto &item : _fieldList[typeName]) {
            createOrDestroy(create, item.fieldType.className, target);
            target += item.fieldType.dataSize;
        }
    }
}

Server::Server(nlohmann::json options) {
    _className = options["service"][0].get<std::string>();
    auto serviceName = options["service"][1].get<std::string>();
    _classInstance = Typing::create(serviceName);
    _host = options["host"][0].get<std::string>();
    _port = options["host"][1].get<int>();
    _serializer = std::make_shared<Serializer>();
    _mainSocket = 0;
    _newConnectionId = 0;

    // Add custom metadata
    std::map<std::string, AnyMethod> methods = {
        {"connectClient", toAnyMethod(&Server::connectClient)},
        {"getMetadata", toAnyMethod(&Server::getMetadata)},
        {"closeClient", toAnyMethod(&Server::closeClient)},
    };
    _serializer->_schemaList.push_back(SchemaInfo("", "Metadata", "connectClient", "dict", "dict", -1));
    _serializer->_schemaList.push_back(SchemaInfo("", "Metadata", "getMetadata", "dict", "dict", -1));
    _serializer->_schemaList.push_back(SchemaInfo("", "Metadata", "closeClient", "dict", "dict", -1));

    // Register methods
    for (auto item : _serializer->getMethods("Metadata", (Any *)this, methods)) {
        _methodList[item.className + "." + item.methodName] = item;
    }
    for (auto item : _serializer->getMethods(_className, _classInstance.get())) {
        _methodList[item.className + "." + item.methodName] = item;
    }
}

void Server::listen() { startServer(); }

void Server::startServer() {
    WSADATA wsaData = {0};
    auto res = WSAStartup(MAKEWORD(2, 2), &wsaData);
    if (res != NO_ERROR) {
        throw std::runtime_error(std::string() + "Failed to setup WSA, code=" + std::to_string(WSAGetLastError()));
    }

    _mainSocket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (!_mainSocket) {
        throw std::runtime_error(std::string() + "Failed to create socket, code=" + std::to_string(WSAGetLastError()));
    }

    int opt = 1000;
    if (setsockopt(_mainSocket, SOL_SOCKET, SO_RCVTIMEO, reinterpret_cast<char *>(&opt), sizeof(opt))) {
        throw std::runtime_error(std::string() + "Failed to set receive timeout, code=" + std::to_string(WSAGetLastError()));
    }

    // TODO: SO_REUSEPORT on linux
    // TODO: SO_KEEPALIVE

    // int opt = 1;
    // if (setsockopt(_mainSocket, SOL_SOCKET, SO_REUSEADDR, reinterpret_cast<char *>(&opt), sizeof(opt))) {
    //     throw std::runtime_error(std::string() + "Internal socket issue, code=" + std::to_string(WSAGetLastError()));
    // }

    struct sockaddr_in address = {AF_INET, htons(_port), INADDR_ANY};

    // TODO: thisHost = gethostbyname(""); ip = inet_ntoa(*(struct in_addr *) *thisHost->h_addr_list); service.sin_addr.s_addr = inet_addr(ip);

    if (bind(_mainSocket, (struct sockaddr *)&address, sizeof(address))) {
        throw std::runtime_error(std::string() + "Failed to bind socket, code=" + std::to_string(WSAGetLastError()));
    }

    if (::listen(_mainSocket, SOMAXCONN)) {
        throw std::runtime_error(std::string() + "Failed to listen socket, code=" + std::to_string(WSAGetLastError()));
    }

    std::cout << "Server running at http://" << _host << ":" << _port << "/" << std::endl;

    _activeConnections.push_back(std::make_shared<Connection>(0, _mainSocket, std::make_pair(_host, _port), getTime(), getTime(), getProjectName(), "/Server/startServer"));
    std::string url;
    std::string headers;
    nlohmann::json payload;
    uint8_t inputBuffer[10000] = {0};
    std::ostringstream writeBuffer;
    fd_set readHandles;
    struct timeval timeout = {0, 500000};

    while (true) {
        // Cleanup
        for (int index = 0; index < _closedConnections.size(); index++) {
            auto connection = _closedConnections[index];
            if (getTime() - connection->wtime > 5000) {
                assert(connection->closed);
                assert(connection->socket == 0);
                _closedConnections.erase(_closedConnections.begin() + index);
                index--;
            }
        }

        // Accept or read
        auto connections = _activeConnections;
        assert(connections.size() < 64);
        readHandles.fd_count = connections.size();
        for (int j = 0; j < connections.size(); j++) {
            readHandles.fd_array[j] = connections[j]->socket;
        }
        int seleted = select(0, &readHandles, NULL, NULL, &timeout);
        if (seleted == SOCKET_ERROR) {
            throw std::runtime_error(std::string() + "Failed to call select, code=" + std::to_string(WSAGetLastError()));
        }

        for (int index = 0; index < connections.size(); index++) {
            assert(index >= 0);
            auto connection = connections[index];
            auto sock = connection->socket;
            if (!__WSAFDIsSet(sock, &readHandles)) {
                continue;
            }
            // Add client
            if (connection->connectionId == 0) {
                auto newSock = accept(_mainSocket, NULL, NULL);

                if (newSock == INVALID_SOCKET) {
                    std::cerr << "Accept failed: " << WSAGetLastError() << std::endl;
                } else {
                    _newConnectionId++;
                    auto connection = std::make_shared<Connection>(_newConnectionId, newSock, getSocketHost(newSock), getTime(), getTime(), "unknown", "");
                    _activeConnections.push_back(connection);
                    std::cout << "Adding client: " << _activeConnections.size() << ", " << _closedConnections.size() << ", " << (unsigned int)newSock << std::endl;
                }
            } 
            // Read
            else {
                // Read chunk
                // See also SO_RCVTIMEO 
                auto received = recv(sock, (char *)inputBuffer, sizeof(inputBuffer), 0);
                if (received > 0) {
                    connection->readBuffer.insert(connection->readBuffer.end(), inputBuffer, inputBuffer + received);
                }
                auto middleIndex = received > 0 ? findIndex(connection->readBuffer, "\r\n\r\n") : -1;
                // std::cout<<"---WAITING "<< connection->connectionId << ", "<< middleIndex << ", " << connection->readBuffer.size() << std::endl;

                // Parse headers and payload
                url = "";
                headers = "";
                payload = nullptr;
                if (received > 0 && middleIndex != -1) {
                    headers = std::string((char *)connection->readBuffer.data(), middleIndex);
                    auto headers2 = getHeaderMap(headers, {"Content-Length", "Project-Id", "Sender-Id"});
                    auto msgLen = parseInt(headers2["Content-Length"]);
                    assert(msgLen >= 0);
                    if (middleIndex + 4 + msgLen <= connection->readBuffer.size()) {
                        std::string newLine = "\r\n";
                        auto firstLine = std::search(headers.begin(), headers.end(), newLine.begin(), newLine.end()) - headers.begin();
                        auto parts = splitString(headers.substr(0, firstLine), " ", 3);
                        if (parts.size() != 3) {
                            throw std::runtime_error("Failed to parse request path");
                        }
                        url = parts[1];
                        if (parts[0] != "POST") {
                            throw std::runtime_error("Only accepting POST");
                        }
                        payload = nlohmann::json::parse(std::string((char *)connection->readBuffer.data() + middleIndex + 4, msgLen));
                        connection->readBuffer.erase(connection->readBuffer.begin(), connection->readBuffer.begin() + middleIndex + 4 + msgLen);

                        connection->wtime = getTime();
                        connection->messageCount += 1;
                        connection->senderId = headers2["Sender-Id"];
                        connection->callId = url;

                        if (headers2["Project-Id"].size()) {
                            connection->projectId = headers2["Project-Id"]; // normally populated in connectClient
                        }
                    }
                }

                // Process error
                if (received < 0) {
                    connection->closed = true;
                    connection->socket = 0;
                    connection->errorCode = WSAGetLastError();
                    _closedConnections.push_back(connection);
                    _activeConnections.erase(std::find(_activeConnections.begin(), _activeConnections.end(), connection));
                    closesocket(sock);
                    std::cout << "Failing client: " << _activeConnections.size() << ", " << _closedConnections.size() << std::endl;
                }
                // Handle closing
                else if (received == 0) {
                    connection->closed = true;
                    connection->socket = 0;
                    connection->errorCode = 0;
                    closesocket(sock);
                    _closedConnections.push_back(connection);
                    _activeConnections.erase(std::find(_activeConnections.begin(), _activeConnections.end(), connection));
                    std::cout << "Removing client: " << _activeConnections.size() << ", " << _closedConnections.size() << std::endl;
                }
                // Process message
                else if (payload != nullptr) {
                    _currentConnection = connection;

                    std::string resp;
                    std::string error;

                    // Server call
                    try {
                        resp = serverCall(url, payload).dump();
                    } catch (const std::exception &ex) {
                        error = ex.what();
                        resp = nlohmann::json({{"error", error}}).dump();
                        assert(error.size());
                    }

                    // Send response
                    if (error.size() == 0) {
                        writeBuffer.str("");
                        writeBuffer.clear();
                        writeBuffer << "HTTP/1.1 200 OK\r\n"
                                    << "Connection: keep-alive\r\n"
                                    << "Content-Length: " << resp.size() << "\r\n"
                                    << "Content-type: application/json\r\n\r\n"
                                    << resp;

                        auto buffer = writeBuffer.str();
                        auto written = send(sock, buffer.c_str(), buffer.size(), 0);
                        if (written != buffer.size()) {
                            throw std::runtime_error(std::string() + "Failed to write socket, code=" + std::to_string(WSAGetLastError()));
                        }
                    }
                    // Process error
                    else {
                        writeBuffer.str("");
                        writeBuffer.clear();
                        assert(error.find("\n") == std::string::npos);
                        writeBuffer << "HTTP/1.1 504 Remote error: " << error << "\r\n"
                                    << "Content-Length: " << resp.size() << "\r\n"
                                    << "Content-type: application/problem+json\r\n\r\n"
                                    << resp;

                        auto buffer = writeBuffer.str();
                        send(sock, buffer.c_str(), buffer.size(), 0);
                        connection->closed = true;
                        connection->socket = 0;
                        connection->errorCode = 50001;
                        closesocket(sock);
                        _closedConnections.push_back(connection);
                        _activeConnections.erase(std::find(_activeConnections.begin(), _activeConnections.end(), connection));
                        std::cout << "Errored client: " << _activeConnections.size() << ", " << _closedConnections.size() << ", " << error << std::endl;
                    }

                    _currentConnection.reset();
                }
            }
        }
    }
}

nlohmann::json Server::serverCall(std::string url, nlohmann::json payload) {
    // std::cout<<"---INCOMING " << url  << std::endl;
    auto parts2 = splitString(url, "/");
    if (parts2.size() != 3 || parts2[0] != "") {
        throw std::runtime_error("Failed to parse route");
    }
    if (_methodList.find(parts2[1] + "." + parts2[2]) == _methodList.end()) {
        throw std::runtime_error("Failed to route");
    }
    auto met = _methodList[parts2[1] + "." + parts2[2]];
    assert(met.methodPointer ? met.methodIndex == -1 : met.methodIndex >= 0);
    std::vector<uint8_t> param;
    _serializer->createInstance(met.methodRequest.className, param, met.methodResponse.className);
    _serializer->fromJson(met.methodRequest.className, payload, param);
    auto method = met.methodPointer ? met.methodPointer : (*reinterpret_cast<AnyMethod **>(met.classInstance))[met.methodIndex];
    nlohmann::json result;

    if (param.capacity() == 16) {
        auto method2 = *reinterpret_cast<std::array<uint8_t, 16> (Any::**)(std::array<uint8_t, 16>)>(&method);
        auto data = ((met.classInstance)->*method2)(*(std::array<uint8_t, 16> *)param.data());
        result = _serializer->destroyInstanceGet(met.methodResponse.className, data);
    } else if (param.capacity() == 32) {
        auto method2 = *reinterpret_cast<std::array<uint8_t, 32> (Any::**)(std::array<uint8_t, 32>)>(&method);
        auto data = ((met.classInstance)->*method2)(*(std::array<uint8_t, 32> *)param.data());
        result = _serializer->destroyInstanceGet(met.methodResponse.className, data);
    } else if (param.capacity() == 64) {
        auto method2 = *reinterpret_cast<std::array<uint8_t, 64> (Any::**)(std::array<uint8_t, 64>)>(&method);
        auto data = ((met.classInstance)->*method2)(*(std::array<uint8_t, 64> *)param.data());
        result = _serializer->destroyInstanceGet(met.methodResponse.className, data);
    } else if (param.capacity() == 128) {
        auto method2 = *reinterpret_cast<std::array<uint8_t, 128> (Any::**)(std::array<uint8_t, 128>)>(&method);
        auto data = ((met.classInstance)->*method2)(*(std::array<uint8_t, 128> *)param.data());
        result = _serializer->destroyInstanceGet(met.methodResponse.className, data);
    } else if (param.capacity() == 256) {
        auto method2 = *reinterpret_cast<std::array<uint8_t, 256> (Any::**)(std::array<uint8_t, 256>)>(&method);
        auto data = ((met.classInstance)->*method2)(*(std::array<uint8_t, 256> *)param.data());
        result = _serializer->destroyInstanceGet(met.methodResponse.className, data);
    } else if (param.capacity() == 512) {
        auto method2 = *reinterpret_cast<std::array<uint8_t, 512> (Any::**)(std::array<uint8_t, 512>)>(&method);
        auto data = ((met.classInstance)->*method2)(*(std::array<uint8_t, 512> *)param.data());
        result = _serializer->destroyInstanceGet(met.methodResponse.className, data);
    } else if (param.capacity() == 1024) {
        auto method2 = *reinterpret_cast<std::array<uint8_t, 1024> (Any::**)(std::array<uint8_t, 1024>)>(&method);
        auto data = ((met.classInstance)->*method2)(*(std::array<uint8_t, 1024> *)param.data());
        result = _serializer->destroyInstanceGet(met.methodResponse.className, data);
    } else {
        throw std::runtime_error(std::string() + "Too large payload: " + std::to_string(param.capacity()));
    }

    // std::cout << "---CALL " << parts2[1] << "." << parts2[2] << ": " << met.methodRequest.className << ", " << met.methodResponse.className << ", " << param.size() << ", " << _serializer->getSize(met.methodResponse.className) << std::endl;
    return result;
}

nlohmann::json Server::connectClient(nlohmann::json param) {
    auto connection = _currentConnection;
    assert(connection);
    assert(connection->socket);
    assert(connection->connectionId);

    connection->processId = param["clientId"].get<std::string>();
    connection->parentId = param["parentId"].get<std::string>();
    connection->shellId = param["shellId"].get<std::string>();
    connection->entryPoint = param["entryPoint"].get<std::string>();
    connection->projectId = param["projectId"].get<std::string>();

    return nlohmann::json({
        {"projectId", getProjectName()},
        {"connected", true},
        {"port", _port},
        {"connectionId", connection->connectionId},
    });
}

nlohmann::json Server::getMetadata(nlohmann::json param) {
    auto connection = _currentConnection;
    if (connection->projectId == "nativerpc") {
        // silent
    } else {
        std::cout << "Responding to metadata: " << connection->connectionId << ", " << connection->projectId << std::endl;
    }

    // Cleanup
    for (int index = 0; index < _closedConnections.size(); index++) {
        auto connection = _closedConnections[index];
        if (getTime() - connection->wtime > 5000) {
            assert(connection->closed);
            assert(connection->socket == 0);
            _closedConnections.erase(_closedConnections.begin() + index);
            index--;
        }
    }

    // Clients
    nlohmann::json clientInfos = nlohmann::json::array();
    std::vector<std::shared_ptr<Connection>> buffers;
    buffers.insert(buffers.end(), _activeConnections.begin(), _activeConnections.end());
    buffers.insert(buffers.end(), _closedConnections.begin(), _closedConnections.end());
    for (auto connection : buffers) {
        if (connection->projectId == "nativerpc") {
            continue;
        }
        clientInfos.push_back({
            {"connectionId", connection->connectionId},
            {"address", connection->address},
            {"readSize", connection->readBuffer.size()},
            {"active", !connection->closed},
            {"closed", connection->closed},
            {"stime", connection->stime / 1000.0},
            {"wtime", connection->wtime / 1000.0},
            {"projectId", connection->projectId},
            {"messageCount", connection->messageCount},
            {"senderId", connection->senderId},
            {"callId", connection->callId},
            {"processId", connection->processId},
            {"shellId", connection->shellId},
        });
    }

    // Schema
    nlohmann::json schemaInfos = nlohmann::json::array();
    for (auto item : _serializer->_schemaList) {
        schemaInfos.push_back({
            {"projectName", item.projectName},
            {"className", item.className},
            {"fieldName", item.fieldName},
            {"fieldType", item.fieldType},
            {"methodName", item.methodName},
            {"methodRequest", item.methodRequest},
            {"methodResponse", item.methodResponse},
            {"idNumber", item.idNumber},
        });
    }
    // clientInfos.sort((aa, bb) => aa.connectionId > bb.connectionId ? 1 : aa.connectionId < bb.connectionId ? -1 : 0);

    return {
        {"projectId", getProjectName()}, {"port", _port}, {"entryPoint", getEntryPoint()}, {"clientCounts", nlohmann::json::array({_activeConnections.size(), _closedConnections.size(), clientInfos.size()})}, {"clientInfos", clientInfos}, {"schemaList", schemaInfos},
    };
}

nlohmann::json Server::closeClient(nlohmann::json param) {
    auto connection = _currentConnection;
    assert(connection->processId == param["clientId"].get<std::string>());
    assert(connection->projectId == param["projectId"].get<std::string>());

    return {
        {"projectId", getProjectName()},
        {"connected", false},
        {"port", _port},
        {"connectionId", connection->connectionId},
    };
}

Client::Client(nlohmann::json options) {
    _className = options["service"].get<std::string>();
    _host = options["host"][0].get<std::string>();
    _port = options["host"][1].get<int>();
    _serializer = std::make_shared<Serializer>();
    _mainSocket = 0;
    _self = this;
    _connectionId = -1;
    initSocket();

    // Add custom metadata
    std::map<std::string, AnyMethod> methods = {
        {"connectClient", toAnyMethod(&Server::connectClient)},
        {"getMetadata", toAnyMethod(&Server::getMetadata)},
        {"closeClient", toAnyMethod(&Server::closeClient)},
    };
    _serializer->_schemaList.push_back(SchemaInfo("", "Metadata", "connectClient", "dict", "dict", -1));
    _serializer->_schemaList.push_back(SchemaInfo("", "Metadata", "getMetadata", "dict", "dict", -1));
    _serializer->_schemaList.push_back(SchemaInfo("", "Metadata", "closeClient", "dict", "dict", -1));

    // Register methods
    // assert(std::search(_serializer->_schemaList.begin(), _serializer->_schemaList.end(), [](const SchemaInfo& x){return x.className == "Metadata" && x.methodName.size();}) != _serializer->_schemaList.end());
    // assert(std::search(_serializer->_schemaList.begin(), _serializer->_schemaList.end(), [this](const SchemaInfo& x){return x.className == _className && x.methodName.size();}) != _serializer->_schemaList.end());
    setupInstance();
}

void Client::initSocket() {
    WSADATA wsaData = {0};
    auto res = WSAStartup(MAKEWORD(2, 2), &wsaData);
    if (res != NO_ERROR) {
        throw std::runtime_error(std::string() + "Failed to setup WSA, code=" + std::to_string(WSAGetLastError()));
    }

    _mainSocket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (!_mainSocket) {
        throw std::runtime_error(std::string() + "Failed to create socket, code=" + std::to_string(WSAGetLastError()));
    }

    struct sockaddr_in address = {AF_INET, htons(_port), INADDR_ANY};

    if (inet_pton(AF_INET, "127.0.0.1", &address.sin_addr) <= 0) {
        throw std::runtime_error(std::string() + "Failed to set host, code=" + std::to_string(WSAGetLastError()));
    }

    if (::connect(_mainSocket, (struct sockaddr *)&address, sizeof(address))) {
        throw std::runtime_error(std::string() + "Failed to connect socket, port=" + std::to_string(_port) + ", code=" + std::to_string(WSAGetLastError()));
    }

    int opt = 1000;
    if (setsockopt(_mainSocket, SOL_SOCKET, SO_RCVTIMEO, reinterpret_cast<char *>(&opt), sizeof(opt))) {
        throw std::runtime_error(std::string() + "Failed to set receive timeout, code=" + std::to_string(WSAGetLastError()));
    }

    auto req2 = nlohmann::json({
        {"projectId", getProjectName()},
        {"clientId", std::to_string(getpid())},
        {"parentId", std::to_string(getpid())},
        {"shellId", getShellId()},
        {"entryPoint", getEntryPoint()},
    }).dump();
    std::ostringstream req;
    req << "POST /" << "Metadata" << "/" << "connectClient" << " HTTP/1.1\r\n"
        << "Host: " << _host << ":" << _port << "\r\n"
        << "Accept: */*\r\n"
        << "Connection: keep-alive\r\n"
        << "Server-Id: connect\r\n"
        << "User-Agent: curl/8.16.0\r\n"
        << "Content-Length: " << req2.size() << "\r\n"
        << "Content-type: application/problem+json\r\n\r\n"
        << req2;
    auto resp = makeRequest(_mainSocket, req.str());
    if (std::get<0>(resp) != "200") {
        throw std::runtime_error("Connection error: " + std::get<1>(resp));
    }
    
    _connectionId = std::get<2>(resp)["connectionId"].get<int>();
}

void Client::setupInstance() {
    _proxyInstance = std::make_shared<ServiceHolder>();

    auto count = 0;
    for (auto item : _serializer->_schemaList) {
        if (item.className != _className || !item.methodName.size()) {
            continue;
        }
        count++;
    }
    _proxyInstance->_virtualTable = new AnyMethod[count + 1];

    auto methodIndex = -1;
    for (auto item : _serializer->_schemaList) {
        if (item.className != _className || !item.methodName.size()) {
            continue;
        }
        methodIndex++;
        // TODO: for (auto item : getMethods(_schemaList, _className, _classInstance.get())) {
        _proxyInstance->_methodList.push_back(MethodInfo(item.className, 0, item.methodName, nullptr, _serializer->findType(item.methodRequest), _serializer->findType(item.methodResponse), -1));
        auto total = max(_serializer->getSize(item.methodRequest), _serializer->getSize(item.methodResponse));
        auto powerOfTwo = 16;
        auto sizeOffset = 0;
        while (powerOfTwo < total) {
            powerOfTwo *= 2;
            sizeOffset++;
        }
        _proxyInstance->_virtualTable[methodIndex] = getProxyMethod(sizeOffset, methodIndex);
    }

    _proxyInstance->_virtualTable[count] = nullptr;
    _proxyInstance->_parent = this;
}

nlohmann::json Client::clientCall(std::string className, std::string methodName, nlohmann::json data) {
    auto req2 = data.dump();
    std::ostringstream req;
    req << "POST /" << className << "/" << methodName << " HTTP/1.1\r\n"
        << "Host: " << _host << ":" << _port << "\r\n"
        << "Accept: */*\r\n"
        << "User-Agent: curl/8.16.0\r\n"
        << "Content-Length: " << req2.size() << "\r\n"
        << "Content-type: application/json\r\n\r\n"
        << req2;
    auto resp = makeRequest(_mainSocket, req.str());
    if (std::get<0>(resp) != "200") {
        throw std::runtime_error("Client error: " + std::get<1>(resp));
    }
    return std::get<2>(resp);
}

void Client::close() {
    if (!_mainSocket) {
        return;
    }
    auto sock = _mainSocket;
    auto req2 = nlohmann::json({
        {"projectId", getProjectName()},
        {"clientId", std::to_string(getpid())},
        {"parentId", std::to_string(getpid())},
        {"shellId", getShellId()},
        {"entryPoint", getEntryPoint()},
        {"connectionId", _connectionId},
    }).dump();
    std::ostringstream req;
    req << "POST /" << "Metadata" << "/" << "closeClient" << " HTTP/1.1\r\n"
        << "Host: " << _host << ":" << _port << "\r\n"
        << "Accept: */*\r\n"
        << "Connection: keep-alive\r\n"
        << "Server-Id: close\r\n"
        << "User-Agent: curl/8.16.0\r\n"
        << "Content-Length: " << req2.size() << "\r\n"
        << "Content-type: application/problem+json\r\n\r\n"
        << req2;
    auto resp = makeRequest(_mainSocket, req.str());
    if (std::get<0>(resp) != "200") {
        throw std::runtime_error("Connection error: " + std::get<1>(resp));
    }
    _mainSocket = 0;
    closesocket(sock);
}

AnyMethod Client::getProxyMethod(int sizeOffset, int methodIndex) {
    AnyMethod allMethods[] = {
        toAnyMethod(&Client::proxyCall<16, 0>),   toAnyMethod(&Client::proxyCall<16, 1>),   toAnyMethod(&Client::proxyCall<16, 2>),   toAnyMethod(&Client::proxyCall<16, 3>),   toAnyMethod(&Client::proxyCall<16, 4>),   toAnyMethod(&Client::proxyCall<16, 5>),
        toAnyMethod(&Client::proxyCall<16, 6>),   toAnyMethod(&Client::proxyCall<16, 7>),   toAnyMethod(&Client::proxyCall<16, 8>),   toAnyMethod(&Client::proxyCall<16, 9>),   toAnyMethod(&Client::proxyCall<32, 0>),   toAnyMethod(&Client::proxyCall<32, 1>),
        toAnyMethod(&Client::proxyCall<32, 2>),   toAnyMethod(&Client::proxyCall<32, 3>),   toAnyMethod(&Client::proxyCall<32, 4>),   toAnyMethod(&Client::proxyCall<32, 5>),   toAnyMethod(&Client::proxyCall<32, 6>),   toAnyMethod(&Client::proxyCall<32, 7>),
        toAnyMethod(&Client::proxyCall<32, 8>),   toAnyMethod(&Client::proxyCall<32, 9>),   toAnyMethod(&Client::proxyCall<64, 0>),   toAnyMethod(&Client::proxyCall<64, 1>),   toAnyMethod(&Client::proxyCall<64, 2>),   toAnyMethod(&Client::proxyCall<64, 3>),
        toAnyMethod(&Client::proxyCall<64, 4>),   toAnyMethod(&Client::proxyCall<64, 5>),   toAnyMethod(&Client::proxyCall<64, 6>),   toAnyMethod(&Client::proxyCall<64, 7>),   toAnyMethod(&Client::proxyCall<64, 8>),   toAnyMethod(&Client::proxyCall<64, 9>),
        toAnyMethod(&Client::proxyCall<128, 0>),  toAnyMethod(&Client::proxyCall<128, 1>),  toAnyMethod(&Client::proxyCall<128, 2>),  toAnyMethod(&Client::proxyCall<128, 3>),  toAnyMethod(&Client::proxyCall<128, 4>),  toAnyMethod(&Client::proxyCall<128, 5>),
        toAnyMethod(&Client::proxyCall<128, 6>),  toAnyMethod(&Client::proxyCall<128, 7>),  toAnyMethod(&Client::proxyCall<128, 8>),  toAnyMethod(&Client::proxyCall<128, 9>),  toAnyMethod(&Client::proxyCall<512, 0>),  toAnyMethod(&Client::proxyCall<512, 1>),
        toAnyMethod(&Client::proxyCall<512, 2>),  toAnyMethod(&Client::proxyCall<512, 3>),  toAnyMethod(&Client::proxyCall<512, 4>),  toAnyMethod(&Client::proxyCall<512, 5>),  toAnyMethod(&Client::proxyCall<512, 6>),  toAnyMethod(&Client::proxyCall<512, 7>),
        toAnyMethod(&Client::proxyCall<512, 8>),  toAnyMethod(&Client::proxyCall<512, 9>),  toAnyMethod(&Client::proxyCall<1024, 0>), toAnyMethod(&Client::proxyCall<1024, 1>), toAnyMethod(&Client::proxyCall<1024, 2>), toAnyMethod(&Client::proxyCall<1024, 3>),
        toAnyMethod(&Client::proxyCall<1024, 4>), toAnyMethod(&Client::proxyCall<1024, 5>), toAnyMethod(&Client::proxyCall<1024, 6>), toAnyMethod(&Client::proxyCall<1024, 7>), toAnyMethod(&Client::proxyCall<1024, 8>), toAnyMethod(&Client::proxyCall<1024, 9>),
    };
    assert(sizeOffset < 6);
    assert(methodIndex < 10);
    assert(sizeOffset * 10 + methodIndex < sizeof(allMethods) / sizeof(*allMethods));
    return allMethods[sizeOffset * 10 + methodIndex];
}

} // namespace nativerpc
