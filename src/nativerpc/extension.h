/**
 *  Native RPC C++ Language Extensions
 *
 *      Any
 *      AnyMethod
 *      toAnyMethod
 *      ClassType
 *      Typing
 *      terminateWithTrace
 *      getTempFileName
 *      execProcess
 *      native_array
 *      leftTrim
 *      rightTrim
 *      findStringIC
 *      toLowerStr
 *      getSubString
 *      parseInt
 *      getSocketHost
 *      getTime
 *      findIndex
 *      replaceAll
 *      getHeaderMap
 *      makeRequest
 */
#pragma once
#pragma pack(1)
#include <iostream>
#include <map>
#include <nlohmann/json.hpp>

#ifdef NATIVERPC_EXPORTS
#define DLL_EXPORT __declspec(dllexport)
#else
#define DLL_EXPORT __declspec(dllimport)
#endif

namespace nativerpc {

class DLL_EXPORT Any {};

typedef void (Any::*AnyMethod)();

template <class T, class R, class P> AnyMethod toAnyMethod(R (T::*pointer)(P)) { return *reinterpret_cast<AnyMethod *>(&pointer); }

class DLL_EXPORT ClassType {
  public:
    std::string className;
    bool isComplex{false};
    int dataSize{0};

    ClassType();
    ClassType(std::string name, bool complex, int size);
};

class DLL_EXPORT Typing {
  public:
    template <class T> static std::string name() {
        std::string result = typeid(T).name();
        size_t idx = 0;
        std::string from = "class ";
        while ((idx = result.find(from, idx)) != std::string::npos) {
            result.replace(idx, from.length(), "");
        }
        if (!_typeCreators[result]) {
            _typeCreators[result] = &Typing::typeCreator<T>;
        }
        return result;
    }

    static std::shared_ptr<Any> create(std::string name) { return _typeCreators[name](); }

    template <class T> static std::shared_ptr<Any> typeCreator() {
        auto result = std::make_shared<T>();
        return *(std::shared_ptr<Any> *)&result;
    }

  public:
    static std::map<std::string, std::shared_ptr<Any> (*)()> _typeCreators;
};

DLL_EXPORT void terminateWithTrace();
DLL_EXPORT std::string getTempFileName(std::string);
DLL_EXPORT std::string execProcess(std::string command, std::string cwd);

DLL_EXPORT void leftTrim(std::string &s);
DLL_EXPORT void rightTrim(std::string &s);
DLL_EXPORT int findStringIC(const std::string &text, const std::string &needle, int start, bool allowMissing);
DLL_EXPORT std::string toLowerStr(const std::string &str);
DLL_EXPORT std::string getSubString(const std::string &text, int start, int end, bool trim);
DLL_EXPORT int parseInt(std::string value);
DLL_EXPORT std::tuple<std::string, int> getSocketHost(int socket);
DLL_EXPORT int64_t getTime();
DLL_EXPORT int findIndex(std::vector<uint8_t> &data, const char *needle);
DLL_EXPORT std::string replaceAll(std::string str, const std::string &from, const std::string &to);
DLL_EXPORT std::vector<std::string> splitString(const std::string &str, std::string delimiter, int maxCount = 0);
DLL_EXPORT std::string joinString(std::vector<std::string> parts, std::string delimiter, int start = 0, int last = -1);
DLL_EXPORT std::map<std::string, std::string> getHeaderMap(std::string headers, std::vector<std::string> names);

DLL_EXPORT std::tuple<std::string, std::string, nlohmann::json> makeRequest(int socket, std::string req);

} // namespace nativerpc
