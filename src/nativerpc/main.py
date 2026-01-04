##
#   Native RPC Main
#
#       Serializer
#           __init__
#           findType
#           getFields
#           getMethods
#           getSize
#           toJson
#           fromJson
#           createInstance
#           destroyInstance
#           createOrDestroy
#
#       Server
#           __init__
#           listen
#           startServer
#           serverCall
#           connectClient
#           getMetadata
#           closeClient
#
#       Client
#           __init__
#           connect
#           initSocket
#           setupInstance
#           clientCall
#           close
##
import __main__
import json
import os
import importlib
import socket
import requests
import select
import time
import traceback
import requests.adapters
from .common import (
    CONFIG_NAME, COMMON_TYPES,
    SchemaInfo, FieldInfo, MethodInfo, SERVICE, HOST, Options, Connection, Service,
    verifyPython,
    getProjectName, getProjectPath, getMessageFiles, parseSchemaList, getShellId,
)
from .extension import (
    getHeaderMap
)
# from typing import TypeAlias, TypeVar, Generic, Type
# T = TypeVar("T")


class Serializer:
    modules: list
    schemaList: list[SchemaInfo]
    fieldList: dict[str, list[FieldInfo]]
    verbose: bool

    def __init__(self):
        self.modules = []
        self.schemaList = []
        self.fieldList = {}
        self.verbose = False

        # Read settings
        package_dir = os.path.join(os.path.dirname(__main__.__file__), '..')
        while os.path.basename(package_dir) and not os.path.exists(os.path.join(package_dir, CONFIG_NAME)):
            package_dir = os.path.dirname(package_dir)
        assert package_dir
        settings = None
        with open(os.path.join(package_dir, CONFIG_NAME), "rt") as file:
            settings = json.load(file)

        # Read module list
        for item in getMessageFiles(getProjectPath()):
            schemaName = os.path.basename(item).replace(".py", "")
            self.modules.append(importlib.import_module(schemaName, item))

        # Read class metadata
        for item in getMessageFiles(getProjectPath()):
            schemaList = parseSchemaList(item)
            for item2 in schemaList:
                self.schemaList.append(SchemaInfo(
                    projectName="",
                    className=item2["className"],
                    fieldName=item2.get("fieldName"),
                    fieldType=item2.get("fieldType"),
                    methodName=item2.get("methodName"),
                    methodRequest=item2.get("methodRequest"),
                    methodResponse=item2.get("methodResponse"),
                    idNumber=item2["idNumber"],
                ))
            assert len(schemaList) > 0

        # Register types
        for item in self.schemaList:
            if item.methodName:
                if item.methodRequest not in self.fieldList:
                    self.fieldList[item.methodRequest] = self.getFields(item.methodRequest)
                if item.methodResponse not in self.fieldList:
                    self.fieldList[item.methodResponse] = self.getFields(item.methodResponse)
            else:
                if item.fieldType not in self.fieldList:
                    self.fieldList[item.fieldType] = self.getFields(item.fieldType)

    def findType(self, name, requireFound):
        if name in COMMON_TYPES:
            return COMMON_TYPES[name]

        for item in self.modules:
            for value in item.__dict__.values():
                if isinstance(value, type) and value.__name__ == name:
                    return value
        if requireFound:
            assert False, f"Failed to find type: {name}"
        return None

    def getFields(self, name):
        if name in COMMON_TYPES:
            return []
        assert [x for x in self.schemaList if x.className == name and x.fieldName]
        classType = self.findType(name, True)
        inst = classType()
        result = []
        assert classType.__name__ == name
        for fieldInfo in self.schemaList:
            if fieldInfo.className != name or not fieldInfo.fieldName:
                continue
            assert hasattr(inst, fieldInfo.fieldName)
            result.append(FieldInfo(
                className=name,
                classType=classType,
                fieldName=fieldInfo.fieldName,
                fieldType=fieldInfo.fieldType,
                idNumber=fieldInfo.idNumber,
            ))

        assert len(result) > 0
        return result

    def getMethods(self, classType, classInstance, className):
        assert classType == Server or issubclass(classType, self.findType(classType.__bases__[0].__name__, True)), f"Unknown subclass: {classType.__bases__[0].__name__}, {classType}"
        result = []
        assert [x for x in self.schemaList if x.className == className and x.methodName]
        for methodInfo in self.schemaList:
            if methodInfo.className != className or not methodInfo.methodName:
                continue
            assert getattr(classInstance, methodInfo.methodName)
            result.append(MethodInfo(
                className=className,
                classType=classType,
                methodName=methodInfo.methodName,
                methodCall=getattr(classInstance, methodInfo.methodName),
                methodParams=[methodInfo.methodRequest, methodInfo.methodResponse],
                idNumber=methodInfo.idNumber,
            ))

        assert len(result) > 0
        return result

    def getSize(self, name):
        if name in COMMON_TYPES:
            result = (
                8 if name == "dict" else
                8 if name == "str" else
                8 if name == "list" else
                4 if name == "int" else
                4 if name == "float" else
                1 if name == "bool" else
                0
            )
            return result
        result = 0
        assert name in self.fieldList
        for item in self.fieldList[name]:
            result += self.getSize(item.fieldType)

        return result

    def toJson(self, typeName, obj=None):
        if obj is None:
            obj = typeName
            typeName = obj.__class__.__name__
        if not isinstance(typeName, str):
            typeName = obj.__class__.__name__

        assert typeName in self.fieldList, f"Unknown type name: {typeName}"
        result = None
        if typeName == 'dict':
            result = {}
            for key, value in obj.items():
                result[key] = value
            assert len(self.fieldList[typeName]) == 0
        elif typeName in COMMON_TYPES:
            result = obj
        else:
            result = {}
            for item in self.fieldList[typeName]:
                assert hasattr(obj, item.fieldName)
                result[item.fieldName] = self.toJson(
                    item.fieldType,
                    getattr(
                        obj,
                        item.fieldName)
                )
            assert len(obj.__dict__) == len(self.fieldList[typeName])
        return result

    def fromJson(self, typeName, data):
        if not isinstance(typeName, str):
            typeName = typeName.__name__
        assert typeName in self.fieldList
        result = None
        if typeName == 'dict':
            assert isinstance(data, dict)
            result = {}
            for key, value in data.items():
                result[key] = value
            assert len(self.fieldList[typeName]) == 0
        elif typeName in COMMON_TYPES:
            result = data
        else:
            assert isinstance(data, dict)
            assert len(self.fieldList[typeName]) > 0
            result = self.fieldList[typeName][0].classType()
            for item in self.fieldList[typeName]:
                if item.fieldName not in data:
                    continue
                setattr(
                    result,
                    item.fieldName,
                    self.fromJson(
                        item.fieldType,
                        data[item.fieldName]
                    )
                )
        return result

    def createInstance(self):
        raise RuntimeError("Not implemented")

    def destroyInstance(self):
        raise RuntimeError("Not implemented")

    def createOrDestroy(self):
        raise RuntimeError("Not implemented")


class Server:
    classType: type
    classInstance: any
    host: str
    port: int
    serializer: Serializer
    methodList: dict[str, MethodInfo]
    mainSocket: any
    clientSockets: list
    activeConnections: list[Connection]
    closedConnections: list[Connection]
    currentConnection: Connection
    newConnectionId: int
    verbose: bool

    def __init__(self, options: Options):
        self.className = options[SERVICE].__bases__[0].__name__
        self.classType = options[SERVICE]
        self.classInstance = options[SERVICE]()
        self.host = options[HOST][0]
        self.port = options[HOST][1]
        self.serializer = Serializer()
        self.methodList = {}
        self.mainSocket = None
        self.clientSockets = []
        self.activeConnections = []
        self.closedConnections = []
        self.currentConnection = None
        self.newConnectionId = 0
        self.verbose = False
        verifyPython()

        # Add custom metadata
        self.serializer.schemaList.extend([
            SchemaInfo(
                projectName=getProjectName(),
                className="Metadata",
                methodName="connectClient",
                methodRequest="dict",
                methodResponse="dict",
                idNumber=-1,
            ),
            SchemaInfo(
                projectName=getProjectName(),
                className="Metadata",
                methodName="getMetadata",
                methodRequest="dict",
                methodResponse="dict",
                idNumber=-1,
            ),
            SchemaInfo(
                projectName=getProjectName(),
                className="Metadata",
                methodName="closeClient",
                methodRequest="dict",
                methodResponse="dict",
                idNumber=-1,
            ),
        ])

        # Register methods
        for item in self.serializer.getMethods(__class__, self, "Metadata"):
            self.methodList[f"{item.className}.{item.methodName}"] = item
        for item in self.serializer.getMethods(self.classType, self.classInstance, self.className):
            self.methodList[f"{item.className}.{item.methodName}"] = item

    def listen(self):
        self.startServer()

    def startServer(self):
        # Server socket
        self.mainSocket = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        self.mainSocket.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
        self.mainSocket.bind((self.host, self.port))
        self.mainSocket.listen()
        print(f"Server running at http://{self.host}:{self.port}")

        # Client sockets
        self.clientSockets = [self.mainSocket]
        self.activeConnections.append(Connection(
            connectionId=0,
            socket=self.mainSocket,
            address=(self.host, self.port),
            stime=time.time(),
            wtime=time.time(),
            projectId=getProjectName(),
            callId="/Server/startServer",
        ))

        # Accept and read
        while self.mainSocket:
            # Cleanup
            for client in self.closedConnections[:]:
                if time.time() - client.wtime > 5:
                    self.closedConnections.remove(client)

            # Accept or read
            readable, writable, errored = select.select(self.clientSockets, [], [], 0.5)

            # Accept or read
            for sock in readable:
                sockIdx = self.clientSockets.index(sock)
                connection = self.activeConnections[sockIdx]
                data = connection.readBuffer

                # Add client
                if sockIdx == 0:
                    assert sock == self.mainSocket
                    new_sock, address = self.mainSocket.accept()
                    self.newConnectionId += 1
                    self.clientSockets.append(new_sock)
                    self.activeConnections.append(Connection(
                        connectionId=self.newConnectionId,
                        socket=new_sock,
                        address=address,
                        stime=time.time(),
                        wtime=time.time(),
                        projectId="unknown",
                    ))
                    if self.verbose:
                        print(
                            f'Adding client: {len(self.activeConnections)}, '
                            f'{len(self.clientSockets)}, {len(self.closedConnections)}'
                        )
                
                # Read
                else:
                    # Read chunk
                    assert sock == connection.socket
                    closeEvent = 0
                    newData = sock.recv(1024)
                    if newData:
                        data += newData
                    else:
                        closeEvent = 1

                    # Parse headers and payload
                    headers = None
                    url = None
                    middle = None
                    contentLen = None
                    payload = None
                    if closeEvent > 0:
                        pass
                    elif b"\r\n\r\n" in data:
                        middle = data.index(b'\r\n\r\n') + len(b'\r\n\r\n')
                        headers = getHeaderMap(
                            data[0: middle].decode(),
                            ["Content-Length", "Project-Id", "Sender-Id"]
                        )
                        parts = [x.strip() for x in data[0: data.index(b'\n')].decode().split(' ') if x.strip()]
                        assert len(parts) == 3
                        url = parts[1]
                        contentLen = int(headers["Content-Length"])
                        if middle + contentLen <= len(data):
                            connection.readBuffer = data[middle+contentLen:]
                            payload = json.loads(data[middle: middle+contentLen].decode())
                            connection.wtime = time.time()
                            connection.messageCount += 1
                            connection.senderId = headers["Sender-Id"]
                            connection.callId = url
                            if headers["Project-Id"]:
                                connection.projectId = headers["Project-Id"]     # normally populated in connectClient
                        else:
                            connection.readBuffer = data
                    else:
                        connection.readBuffer = data

                    # Process message
                    buf = None
                    if closeEvent > 0:
                        pass
                    elif payload is not None:
                        # Server call
                        try:
                            self.currentConnection = connection
                            buf = self.serverCall(url, payload)
                            
                        # Process error
                        except Exception as ex:
                            if self.verbose:
                                print(
                                    f"ERROR: Failed in call: "
                                    f"{connection.connectionId}, {connection.callId}, "
                                    f"{url}, {ex}"
                                )
                                traceback.print_exc()
                            closeEvent = 2
                            respBuf = json.dumps({
                                "type": "https://nativerpc.com/errors/not-found",
                                "title": "Internal error",
                                "detail": str(ex),
                                "instance": url,
                                "status": 504,
                            }).encode("utf-8")
                            buf = (
                                f"HTTP/1.1 504 Remote error\r\n"
                                f"Content-Length: {len(respBuf)}\r\n"
                                f"Content-type: application/problem+json\r\n\r\n"
                            ).encode() + respBuf
                        finally:
                            self.currentConnection = None

                    # Send response
                    if buf is None or closeEvent == 1:
                        pass
                    else:
                        try:
                            sock.sendall(buf)
                        except Exception as ex:
                            if self.verbose:
                                print(
                                    f"ERROR: Failed in send: "
                                    f"{connection.connectionId}, {connection.callId}"
                                    f"{url}, {ex}"
                                )
                                traceback.print_exc()
                            closeEvent = 3

                    # Ensure closed
                    if closeEvent > 0:
                        self.clientSockets.remove(sock)
                        self.activeConnections.remove(connection)
                        self.closedConnections.append(connection)
                        connection.closed = True
                        connection.socket = None
                        connection.wtime = time.time()
                        closed = False
                        try:
                            sock.close()
                            closed = True
                        except Exception:
                            pass
                        if self.verbose:
                            print(
                                f"Removing client: "
                                f"{closeEvent}, {closed}, "
                                f"{len(self.activeConnections)}, {len(self.clientSockets)}, {len(self.closedConnections)}"
                            )

        self.mainSocket.close()

    def serverCall(self, url, payload):
        parts = [x for x in url.split('/') if x]
        if len(parts) != 2:
            raise RuntimeError(f"Failed to route: {parts}")
        if f"{parts[0]}.{parts[1]}" not in self.methodList:
            raise RuntimeError(f"Failed to route: {parts}")
        if isinstance(payload, str):
            payload = json.loads(payload)
        met = self.methodList[f"{parts[0]}.{parts[1]}"]
        param = self.serializer.fromJson(met.methodParams[0], payload)
        assert met.methodParams[1] in COMMON_TYPES or met.methodParams[
            1] in self.serializer.fieldList, f"Missing type: {met.methodParams[1]}"
        assert met.methodParams[1] in COMMON_TYPES or len(
            self.serializer.fieldList[met.methodParams[1]]) > 0, f"Missing field items: {met.methodParams[1]}"
        respType = COMMON_TYPES[met.methodParams[1]
                                ] if met.methodParams[1] in COMMON_TYPES else self.serializer.fieldList[met.methodParams[1]][0].classType
        resp = met.methodCall(param)
        assert isinstance(resp, respType)
        respJson = self.serializer.toJson(met.methodParams[1], resp)
        # print('---CALL', parts, param, resp)
        respBuf = json.dumps(respJson).encode("utf-8")
        return (
            f"HTTP/1.1 200 OK\r\n"
            f"Content-Length: {len(respBuf)}\r\n"
            f"Content-type: application/json\r\n\r\n"
        ).encode() + respBuf

    def connectClient(self, param: dict):
        connection = self.currentConnection

        connection.processId = param["clientId"]
        connection.parentId = param["parentId"]
        connection.shellId = param["shellId"]
        connection.entryPoint = param["entryPoint"]
        connection.projectId = param["projectId"]

        return {
            "projectId": getProjectName(),
            "connected": True,
            "port": self.port,
            "connectionId": connection.connectionId,
        }

    def getMetadata(self, param: dict):
        connection = self.currentConnection
        if connection.projectId == "nativerpc":
            pass
        else:
            if self.verbose:
                print(
                    f'Responding to metadata: {connection.connectionId}, {connection.projectId}')

        # Cleanup
        for client in self.closedConnections[:]:
            if time.time() - client.wtime > 5:
                self.closedConnections.remove(client)

        # Clients
        clientInfos = []
        for client in self.activeConnections + self.closedConnections:
            if client.projectId == "nativerpc":
                continue
            clientInfos.append({
                "connectionId": client.connectionId,
                "address": client.address,
                "readSize": len(client.readBuffer),
                "active": not client.closed,
                "closed": client.closed,
                "stime": client.stime,
                "wtime": client.wtime,
                "projectId": client.projectId,
                "messageCount": client.messageCount,
                "senderId": client.senderId,
                "callId": client.callId,
                "processId": client.processId,
                "shellId": client.shellId,
            })

        clientInfos.sort(key=lambda item: (1 if not item["active"] else 0, item["connectionId"]))

        return {
            "projectId": getProjectName(),
            "port": self.port,
            "entryPoint": __main__.__file__,
            "clientCounts": [len(self.activeConnections), len(self.closedConnections), len(clientInfos)],
            "clientInfos": clientInfos,
            "schemaList": [[x.__dict__ for x in self.serializer.schemaList]],
        }

    def closeClient(self, param: dict):
        connection = self.currentConnection
        assert connection.processId == param[
            "clientId"], f'Mismatch in close: {connection.connectionId}, {connection.processId}, {param["clientId"]}'
        assert connection.projectId == param["projectId"], "Mismatch in project id"

        return {
            "projectId": getProjectName(),
            "connected": False,
            "port": self.port,
            "connectionId": connection.connectionId,
        }


class Client:
    classType: type
    host: str
    port: int
    serializer: Serializer
    mainSocket: requests.Session
    connectionId: int
    proxyInstance: any
    verbose: bool

    def __init__(self, options: Options):
        self.className = options[SERVICE].__name__
        self.classType = options[SERVICE]
        self.host = options[HOST][0]
        self.port = options[HOST][1]
        self.serializer = Serializer()
        self.mainSocket = None
        self.connectionId = 0
        self.proxyInstance = Service(self)
        self.verbose = False

        # Add custom metadata
        self.serializer.schemaList.extend([
            SchemaInfo(
                projectName=getProjectName(),
                className="Metadata",
                methodName="connectClient",
                methodRequest="dict",
                methodResponse="dict",
                idNumber=-1,
            ),
            SchemaInfo(
                projectName=getProjectName(),
                className="Metadata",
                methodName="getMetadata",
                methodRequest="dict",
                methodResponse="dict",
                idNumber=-1,
            ),
            SchemaInfo(
                projectName=getProjectName(),
                className="Metadata",
                methodName="closeClient",
                methodRequest="dict",
                methodResponse="dict",
                idNumber=-1,
            ),
        ])

        # Register methods
        assert [x for x in self.serializer.schemaList if x.className == "Metadata" and x.methodName]
        assert [x for x in self.serializer.schemaList if x.className == self.className and x.methodName]
        self.setupInstance()

    def connect(self):
        self.initSocket()
        return self.proxyInstance

    def initSocket(self):
        payload = {
            "projectId": getProjectName(),
            "clientId": os.getpid(),
            "parentId": os.getppid(),
            "shellId": getShellId(),
            "entryPoint": __main__.__file__,
        }

        self.mainSocket = requests.Session()
        self.main_adapter = requests.adapters.HTTPAdapter(
            max_retries=1, pool_connections=1, pool_maxsize=1, pool_block=True)
        self.mainSocket.mount('http://', self.main_adapter)
        self.mainSocket.mount('https://', self.main_adapter)

        req = requests.Request(
            'POST',
            f"http://{self.host}:{self.port}/Metadata/connectClient",
            json=payload,
            headers={
                "Connection": "keep-alive",
                "Sender-Id": "connect"
            }
        )
        resp = self.mainSocket.send(
            request=req.prepare(),
            timeout=1,
            stream=False,
        )
        resp.raise_for_status()
        assert resp.status_code == 200
        resp2 = resp.json()
        self.connectionId = resp2["connectionId"]

    def setupInstance(self):
        assert [x for x in self.serializer.schemaList if x.className == self.className and x.methodName]
        for methodInfo in self.serializer.schemaList:
            if methodInfo.className != self.className or not methodInfo.methodName:
                continue
            this = self
            method = methodInfo.methodName
            request = methodInfo.methodRequest
            response = methodInfo.methodResponse
            setattr(
                self.proxyInstance,
                methodInfo.methodName,
                lambda param, this=this, method=method, request=request, response=response:
                this.clientCall(param, this.className, method, request, response)
            )

        for methodInfo in self.serializer.schemaList:
            if methodInfo.className != "Metadata" or not methodInfo.methodName:
                continue
            this = self
            method = methodInfo.methodName
            request = methodInfo.methodRequest
            response = methodInfo.methodResponse
            setattr(
                self.proxyInstance,
                methodInfo.methodName,
                lambda param, this=this, method=method, request=request, response=response:
                this.clientCall(param, "Metadata", method, request, response)
            )

    def clientCall(self, param, className, methodName, reqName, resName):
        assert self.mainSocket
        reqJson = self.serializer.toJson(reqName, param)
        req = requests.Request(
            'POST',
            f"http://{self.host}:{self.port}/{className}/{methodName}",
            json=reqJson,
            headers={
                "Sender-Id": "call"
            }
        )
        resp = self.mainSocket.send(
            request=req.prepare(),
            timeout=1,
            stream=False
        )
        # Throw client errors
        if resp.status_code != 200:
            details = "Internal error"
            try:
                details = json.loads(resp.text)["detail"]
            except Exception:
                pass
            raise RuntimeError(
                f"Client error: {resp.reason}: {details}, code={resp.status_code}"
            )
        resp2 = resp.json()
        resp3 = self.serializer.fromJson(resName, resp2)
        return resp3

    def close(self):
        payload = {
            "projectId": getProjectName(),
            "clientId": os.getpid(),
            "parentId": os.getppid(),
            "shellId": getShellId(),
            "entryPoint": __main__.__file__,
            "connectionId": self.connectionId,
        }
        req = requests.Request(
            'POST',
            f"http://{self.host}:{self.port}/Metadata/closeClient",
            json=payload,
            headers={
                "Sender-Id": "close"
            }
        )
        try:
            resp = self.mainSocket.send(
                request=req.prepare(),
                timeout=1,
                stream=False,
            )
            resp.raise_for_status()
            assert resp.status_code == 200
        except Exception:
            if self.verbose:
                print('WARNING: Failing to close cleanly')
        self.mainSocket.close()
        self.mainSocket = None
