/**
 *  Native RPC Main
 *
 *      Serializer
 *          constructor
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
 *          loadSchema
 *
 *      Client
 *          Client
 *          connect
 *          initSocket
 *          setupInstance
 *          clientCall
 *          close
 *          loadSchema
 *          getProxyMethod
 *          proxyCall
 */
#include "common.h"

namespace nativerpc {

class DLL_EXPORT Serializer {
  public:
    Serializer();
    ClassType findType(std::string name);
    std::vector<FieldInfo> getFields(std::string name);
    std::vector<MethodInfo> getMethods(std::string className, Any *classInstance);
    std::vector<MethodInfo> getMethods(std::string className, Any *classInstance, std::map<std::string, AnyMethod> methods);

    int getSize(std::string typeName);
    void getSize(std::string typeName, std::vector<uint8_t> &buffer);

    nlohmann::json toJson(std::string typeName, const Any *obj);
    nlohmann::json toJson(std::string typeName, std::vector<uint8_t> &obj);
    template <int SIZE> nlohmann::json toJson(std::string typeName, const std::array<uint8_t, SIZE> &data) {
        assert(getSize(typeName) <= SIZE);
        return toJson(typeName, (Any *)data.data());
    }

    template <class T> nlohmann::json toJson(const T &obj) {
        std::string name = typeid(T).name();
        if (name.starts_with("class ")) {
            name = name.substr(strlen("class "));
        }
        if (name.starts_with("class nlohmann::json_")) {
            name = "dict";
        }
        if (name == "bool") {
            name = "bool";
        }
        return toJson(name, reinterpret_cast<const Any *>(&obj));
    }

    template <class T> T fromJson(nlohmann::json data) {
        std::string name = typeid(T).name();
        if (name.starts_with("class ")) {
            name = name.substr(strlen("class "));
        }
        T result;
        fromJson(name, data, reinterpret_cast<Any *>(&result));
        return result;
    }
    void fromJson(std::string typeName, nlohmann::json data, Any *obj);

    void fromJson(std::string typeName, nlohmann::json data, std::vector<uint8_t> &obj);

    void createInstance(std::string className, std::vector<uint8_t> &data);
    template <int SIZE> void createInstance(std::string className, std::array<uint8_t, SIZE> &data) {
        int total = getSize(className);
        assert(total <= data.size());
        createOrDestroy(true, className, &data[0]);
    }
    void createInstance(std::string className, std::vector<uint8_t> &data, std::string secondaryClass);
    void destroyInstance(std::string className, std::vector<uint8_t> &data);
    void destroyInstance(std::string className, Any *obj);

    template <int SIZE> nlohmann::json destroyInstanceGet(std::string className, const std::array<uint8_t, SIZE> &data) {
        int total = getSize(className);
        assert(total <= data.size());
        auto result = toJson(className, reinterpret_cast<const Any *>(&data));
        createOrDestroy(false, className, reinterpret_cast<uint8_t *>(const_cast<std::array<uint8_t, SIZE> *>(&data)));
        return result;
    }

    void createOrDestroy(bool create, std::string typeName, uint8_t *data);

  public:
    std::vector<std::string> _modules;
    std::vector<SchemaInfo> _schemaList;
    std::map<std::string, std::vector<FieldInfo>> _fieldList;
    bool _verbose{false};
};

class DLL_EXPORT Server {
  public:
    Server(nlohmann::json options);
    void listen();
    void startServer();
    nlohmann::json serverCall(std::string url, nlohmann::json payload);
    nlohmann::json connectClient(nlohmann::json param);
    nlohmann::json getMetadata(nlohmann::json param);
    nlohmann::json closeClient(nlohmann::json param);
    void loadSchema();

  private:
    std::shared_ptr<Any> _classInstance;
    std::string _className;
    std::string _host;
    int _port;
    std::shared_ptr<Serializer> _serializer;
    std::map<std::string, MethodInfo> _methodList;
    int _mainSocket;
    std::vector<std::shared_ptr<Connection>> _activeConnections;
    std::vector<std::shared_ptr<Connection>> _closedConnections;
    std::shared_ptr<Connection> _currentConnection;
    int _newConnectionId;
    bool _verbose{false};
};

class DLL_EXPORT Client {
  public:
    Client(nlohmann::json options);
    template <class T> std::shared_ptr<Service<T>> connect() { 
      return *(std::shared_ptr<Service<T>> *)&_proxyInstance; 
    }
    void initSocket();
    void setupInstance();
    nlohmann::json clientCall(std::string className, std::string methodName, nlohmann::json data);
    void close();
    void loadSchema();

    AnyMethod getProxyMethod(int sizeOffset, int methodIndex);

    template <int SIZE, int ID> std::array<uint8_t, SIZE> proxyCall(std::array<uint8_t, SIZE> param) {
        auto service = reinterpret_cast<ServiceHolder*>(this);
        auto client = service->_parent;
        auto serializer = client->_serializer;
        auto paramJson = serializer->destroyInstanceGet(service->_methodList[ID].methodRequest.className, param);
        auto respJson = client->clientCall(service->_methodList[ID].className, service->_methodList[ID].methodName, paramJson);
        std::array<uint8_t, SIZE> result;
        serializer->createInstance(service->_methodList[ID].methodResponse.className, result);
        serializer->fromJson(service->_methodList[ID].methodResponse.className, respJson, (Any*)&result);
        return result;
    }

  public:
    std::string _className;
    std::string _host;
    int _port;
    std::shared_ptr<Serializer> _serializer;
    int _mainSocket;
    std::shared_ptr<ServiceHolder> _proxyInstance;
    Client* _self;
    int _connectionId;
    bool _verbose{false};
};

} // namespace nativerpc