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
 *          constructor
 *          listen
 *          startServer
 *          createOrGetConnection
 *          serverCall
 *          connectClient
 *          getMetadata
 *          closeClient
 * 
 *      Client
 *          constructor
 *          connect
 *          initSocket
 *          setupInstance
 *          clientCall
 *          close
 * 
 */
import * as fs from "node:fs";
import * as path from "node:path";
import * as http from 'http';
import * as net from 'net';
import { strict as assert } from "node:assert";
import {
    CONFIG_NAME,
    COMMON_TYPES,
    verifyPython, getProjectName, getProjectPath, getMessageFiles,
    SchemaInfo, FieldInfo, MethodInfo, type Options, Connection, Service,
    parseSchemaList,
    getShellId,
} from './common';
import { 
    type ClassType,
    getHeaderMap,
    createPayload,
    SocketReceiver,
} from "./extension";

export class Serializer {
    modules: any[];
    schemaList: SchemaInfo[];
    fieldList: Record<string, FieldInfo[]>;
    
    constructor() {
        this.modules = [require.main];
        this.schemaList = [];
        this.fieldList = {};

        // Read settings
        verifyPython();
        let packageDir = getProjectPath();
        while (packageDir && !fs.existsSync(path.join(packageDir, CONFIG_NAME))) {
            packageDir = path.dirname(packageDir);
        }
        assert(packageDir);
        const settings = JSON.parse(fs.readFileSync(
            path.join(packageDir, CONFIG_NAME),
            { encoding: 'utf-8' }
        ));
        
        // Read module list
        for (const item of getMessageFiles(getProjectPath())) {
            assert(fs.existsSync(item));
            require(item);
        }

        // Read class metadata
        for (const item of getMessageFiles(getProjectPath())) {
            for (const item2 of parseSchemaList(item)) {
                this.schemaList.push(new SchemaInfo({
                    projectName: "",
                    className: item2.className,
                    fieldName: item2.fieldName,
                    fieldType: item2.fieldType,
                    methodName: item2.methodName,
                    methodRequest: item2.methodRequest,
                    methodResponse: item2.methodResponse,
                    idNumber: item2.idNumber,
                }));
            }
        }

        // Register types
        for (const item of this.schemaList) {
            if (item.methodName) {
                if (!this.fieldList[item.methodRequest]) {
                    this.fieldList[item.methodRequest] = this.getFields(item.methodRequest);
                }
                if (!this.fieldList[item.methodResponse]) {
                    this.fieldList[item.methodResponse] = this.getFields(item.methodResponse);
                }
            } else {
                if (!this.fieldList[item.fieldType]) {
                    this.fieldList[item.fieldType] = this.getFields(item.fieldType);
                }
            }
        }
    }

    findType(name, requireFound, module=null): ClassType<any> {
        if (!module) {
            module = this.modules[0];
        }
        if (COMMON_TYPES[name]) {
            return COMMON_TYPES[name];
        }
        if (module.exports[name]) {
            return module.exports[name];
        }
        for (const item of module.children) {
            if (item.id === module.id) {
                continue;
            }
            const res = this.findType(name, false, item);
            if (res) {
                return res;
            }
        }
        if (requireFound) {
            assert(false, `Failed to find type: ${name}`);
        }
        return null;
    }

    getFields(name) {
        if (COMMON_TYPES[name]) {
            return [];
        }
        assert(!COMMON_TYPES[name]);
        if (!this.schemaList.some(item => item.className === name && item.fieldName)) {
            throw new Error(`Unknown type name: ${name}`);
        }
        const classType = this.findType(name, true);
        const inst = new classType({});
        const result = []
        for (const item of this.schemaList) {
            if (item.className != name || !item.fieldName) {
                continue;
            }
            assert(inst[item.fieldName] !== undefined);
            result.push(new FieldInfo({
                className: name,
                classType: classType,
                fieldName: item.fieldName,
                fieldType: item.fieldType,
                idNumber: item.idNumber,
            }));
        }
        return result;
    }

    getMethods(classType, classInstance, className) {
        const result = [];
        assert(this.schemaList.some(item => item.className === className && item.methodName));
        for (const methodInfo of this.schemaList) {
            if (methodInfo.className != className || !methodInfo.methodName) {
                continue
            }
            assert(classInstance[methodInfo.methodName]);
            result.push(new MethodInfo({
                className: className,
                classType: classType,
                classInstance: classInstance,
                methodName: methodInfo.methodName,
                methodCall: classInstance[methodInfo.methodName],
                methodRequest: methodInfo.methodRequest, 
                methodResponse: methodInfo.methodResponse,
                idNumber: methodInfo.idNumber,
            }))
        }
        return result;
    }

    getSize(name) {
        if (COMMON_TYPES[name]) {
            return (
                name == "dict" ? 8 :
                name == "str" ? 8 :
                name == "list" ? 8 :
                name == "int" ? 4 :
                name == "float" ? 4 :
                name == "bool" ? 1 :
                0
            );
        }
        assert(this.fieldList[name]);
        let result = 0;
        for (const item of this.fieldList[name]) {
            result += this.getSize(item.fieldType);
        }
        return result;
    }

    toJson(typeName, obj) {
        assert(this.fieldList[typeName]);
        let result = null;
        if (typeName === 'dict') {
            result = {};
            for (const [key, value] of Object.entries(obj)) {
                result[key] = value;
            }
            assert(this.fieldList[typeName].length === 0);
        } else if (COMMON_TYPES[typeName]) {
            result = obj;
        } else {
            result = {};
            for (const item of this.fieldList[typeName]) {
                assert(item.fieldName in obj);
                result[item.fieldName] = this.toJson(item.fieldType, obj[item.fieldName]);
            }
            assert(Object.keys(obj).length === this.fieldList[typeName].length);
        }
        return result;
    }

    fromJson(typeName, data) {
        if (!COMMON_TYPES[typeName] && !this.fieldList[typeName]) {
            throw new Error(`Unknown type: ${typeName}`);
        }
        let result = null;
        if (typeName === 'dict') {
            result = {};
            for (const [key, value] of Object.entries(data)) {
                result[key] = value;
            }
            assert(this.fieldList[typeName].length === 0);
        } else if (COMMON_TYPES[typeName]) {
            result = data;
        } else {
            assert(Object.getPrototypeOf(data).constructor.name === 'Object');
            result = new this.fieldList[typeName][0].classType({});
            for (const item of this.fieldList[typeName]) {
                if (!(item.fieldName in data)) {
                    continue;
                }
                result[item.fieldName] = this.fromJson(item.fieldType, data[item.fieldName]);
            }
        }
        return result
    }

    createInstance() {
        throw new Error("Not implemented");
    }

    destroyInstance() {
        throw new Error("Not implemented");
    }

    createOrDestroy() {
        throw new Error("Not implemented");
    }
}

export class Server {
    classType: any;
    classInstance: any;
    className: string;
    host: string;
    port: number;
    serializer: Serializer;
    methodList: Record<string, MethodInfo>;
    httpServer: any;
    activeConnections: Connection[];
    closedConnections: Connection[];
    currentConnection: any;
    newConnectionId: number;

    constructor(options: Options) {
        this.classType = options.service;
        this.classInstance = new options.service();
        this.className = Object.getPrototypeOf(this.classType).name;
        this.host = options.host[0];
        this.port = options.host[1];
        this.serializer = new Serializer();
        this.methodList = {};
        this.httpServer = null;
        this.activeConnections = [];
        this.closedConnections = [];
        this.currentConnection = null;
        this.newConnectionId = 0;

        // Add custom metadata
        this.serializer.schemaList.push(
            new SchemaInfo({
                projectName: "",
                className: "Metadata",
                methodName: "connectClient",
                methodRequest: "dict",
                methodResponse: "dict",
                idNumber: -1,
            })
        );
        this.serializer.schemaList.push(
            new SchemaInfo({
                projectName: "",
                className: "Metadata",
                methodName: "getMetadata",
                methodRequest: "dict",
                methodResponse: "dict",
                idNumber: -1,
            })
        );
        this.serializer.schemaList.push(
            new SchemaInfo({
                projectName: "",
                className: "Metadata",
                methodName: "closeClient",
                methodRequest: "dict",
                methodResponse: "dict",
                idNumber: -1,
            })
        );

        // Register methods
        for (const item of this.serializer.getMethods(this.constructor, this, "Metadata")) {
            this.methodList[`${item.className}.${item.methodName}`] = item;
        }
        for (const item of this.serializer.getMethods(this.classType, this.classInstance, this.className)) {
            this.methodList[`${item.className}.${item.methodName}`] = item;
        }
    }

    listen() {
        this.startServer();
    }

    startServer() {
        // Server socket
        this.httpServer = http.createServer(
            (req, res) => {
                // Get or create connection
                const { isNew, connection } = this.createOrGetConnection(req);

                // Handle disconnect
                if (isNew) {
                    connection.socket.on('close', () => {
                        if (!connection.closed) {
                            connection.closed = true;
                            connection.wtime = Date.now() / 1000;
                            this.activeConnections.splice(this.activeConnections.indexOf(connection), 1)
                            this.closedConnections.push(connection)
                            console.log(
                                `Removing client: 1, ${this.activeConnections.length}, ${this.closedConnections.length}, ${connection.error}`
                            )
                        }
                    })
                }

                // Read chunk
                req.on('data', (chunk) => {
                    connection.readBuffer += chunk.toString();
                });

                // Parse and process message
                req.on('end', () => {
                    // Parse headers and payload
                    const url = req.url;
                    const headers = getHeaderMap(
                        req.headers,
                        ["Content-Length", "Project-Id", "Sender-Id"]
                    )
                    assert(parseInt(headers['Content-Length']) == connection.readBuffer.length);
                    const payload = JSON.parse(connection.readBuffer);
                    connection.readBuffer = '';
                    assert(req.method == 'POST');
                    connection.wtime = Date.now() / 1000;
                    connection.messageCount += 1
                    connection.senderId = headers["Sender-Id"]
                    connection.callId = url
                    if (headers["Project-Id"]) {
                        connection.projectId = headers["Project-Id"];     // normally populated in connectClient
                    }

                    // Process message
                    this.currentConnection = connection;
                    this.serverCall(url, payload)
                        .then(resp => {
                            // Send response
                            this.currentConnection = null;
                            res.writeHead(200, {
                                'Content-Type': 'application/json',
                                'Connection': 'keep-alive',
                            });
                            res.shouldKeepAlive = true;
                            res.end(JSON.stringify(resp));
                        })
                        .catch(err => {
                            // Process error
                            this.currentConnection = null;
                            connection.error = err;
                            console.error('Request error:', err);
                            res.writeHead(504, { 'Content-Type': 'application/json' });
                            res.end(JSON.stringify({
                                error: `Server error: ${err}`
                            }));
                            connection.socket.destroy()
                        })
                });

                // Process error
                req.on('error', (err) => {
                    connection.error = err;
                    console.error('Request error:', err);
                    // res.writeHead(505, { 'Content-Type': 'application/json' });
                    // res.end(JSON.stringify({error: `Internal Server Error: ${err}`}));
                    // assert(res.headersSent);
                    connection.socket.destroy()
                });
                res.on('finish', () => {
                });
                res.on('close', () => {
                });
            }
        );
        this.httpServer.listen(
            this.port,
            this.host,
            undefined,
            () => {
                console.log(`Server running at http://${this.host}:${this.port}/`);
            }
        );

        // Client sockets
        this.activeConnections.push(new Connection({
            connectionId: 0,
            socket: null,
            address: [this.host, this.port],
            stime: Date.now() / 1000,
            wtime: Date.now() / 1000,
            projectId: getProjectName(),
            callId: "/Server/startServer",
        }))
    }

    createOrGetConnection(req) {
        let connection = this.activeConnections.find(item => item.socket === req.socket);
        if (connection) {
            return { isNew: false, connection };
        }        

        // Add client
        this.newConnectionId++;
        connection = new Connection({
            connectionId: this.newConnectionId,
            socket: req.socket,
            address: [req.socket.remoteAddress, req.socket.remotePort],
            stime: Date.now() / 1000,
            wtime: Date.now() / 1000,
            projectId: "unknown",
            callId: "",
        });
        this.activeConnections.push(connection);
        console.log(
            `Adding client: ${this.activeConnections.length}, ${this.closedConnections.length}`
        )
        return { isNew: true, connection };
    }

    async serverCall(url, payload) {
        const parts = url.split('/').filter(item => item);
        if (parts.length !== 2) {
            throw new Error(`Failed to route: ${parts}`)
        }
        if (!this.methodList[`${parts[0]}.${parts[1]}`]) {
            throw new Error(`Failed to route: ${parts}`)
        }
        const met = this.methodList[`${parts[0]}.${parts[1]}`];
        const param = this.serializer.fromJson(met.methodRequest, payload);
        const respType = COMMON_TYPES[met.methodResponse] ?? this.serializer.fieldList[met.methodResponse][0].classType;
        const resp = await met.methodCall.apply(met.classInstance, [param])
        assert(resp instanceof respType);
        const respJson = this.serializer.toJson(met.methodResponse, resp);
        // console.log('CALL', parts, Object.keys(param).length, Object.keys(respJson).length)
        return respJson;
    }

    connectClient(param: object) {
        const connection = this.currentConnection;
        assert(connection);
        assert(connection.socket);

        connection.processId = param["clientId"]
        connection.parentId = param["parentId"]
        connection.shellId = param["shellId"]
        connection.entryPoint = param["entryPoint"]
        connection.projectId = param["projectId"]

        return {
            "projectId": getProjectName(),
            "connected": true,
            "port": this.port,
            "connectionId": connection.connectionId,
        };
    }

    getMetadata(param: object) {
        const connection = this.currentConnection;
        if (connection.projectId == "nativerpc") {
            // silent
        } else {
            console.log(
                `Responding to metadata: ${connection.connectionId}, ${connection.projectId}`
            )
        }

        // Cleanup
        for (const client of this.closedConnections) {
            if (Date.now() / 1000 - client.wtime > 5) {
                this.closedConnections.splice(this.closedConnections.indexOf(client), 1)
            }
        }

        // Clients
        const clientInfos = [];
        for (const client of [...this.activeConnections, ...this.closedConnections]) {
            if (client.projectId == "nativerpc") {
                continue
            }
            clientInfos.push({
                "connectionId": client.connectionId,
                "address": client.address,
                "readSize": client.readBuffer.length,
                "active": !client.closed,
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
        }

        clientInfos.sort((aa, bb) => aa.connectionId > bb.connectionId ? 1 : aa.connectionId < bb.connectionId ? -1 : 0);

        return {
            projectId: getProjectName(),
            port: this.port,
            entryPoint: require.main.filename,
            clientCounts: [this.activeConnections.length, this.closedConnections.length, clientInfos.length],
            clientInfos: clientInfos,
            schemaList: [...this.serializer.schemaList],
        }
    }

    closeClient(param: object) {
        const connection = this.currentConnection;
        assert(connection.processId == param["clientId"], `Mismatch in close: ${connection.connectionId}, ${connection.processId}, ${param["clientId"]}`);
        assert(connection.projectId == param["projectId"], "Mismatch in project id");

        return {
            "projectId": getProjectName(),
            "connected": false,
            "port": this.port,
            "connectionId": connection.connectionId,
        }
    }
}

export class Client {
    className: string;
    classType: any;
    host: string;
    port: number;
    serializer: Serializer;
    mainSocket: any;
    mainReceiver: SocketReceiver;
    connectionId: number;
    proxyInstance: Service;

    constructor(options: Options) {
        this.className = options.service.name;
        this.classType = options.service;
        this.host = options.host[0];
        this.port = options.host[1];
        this.serializer = new Serializer();
        this.mainSocket = null;
        this.mainReceiver = null;
        this.connectionId = 0;
        this.proxyInstance = new Service(this);

        // Add custom metadata
        this.serializer.schemaList.push(
            new SchemaInfo({
                projectName: "",
                className: "Metadata",
                methodName: "connectClient",
                methodRequest: "dict",
                methodResponse: "dict",
                idNumber: -1,
            })
        );
        this.serializer.schemaList.push(
            new SchemaInfo({
                projectName: "",
                className: "Metadata",
                methodName: "getMetadata",
                methodRequest: "dict",
                methodResponse: "dict",
                idNumber: -1,
            })
        );
        this.serializer.schemaList.push(
            new SchemaInfo({
                projectName: "",
                className: "Metadata",
                methodName: "closeClient",
                methodRequest: "dict",
                methodResponse: "dict",
                idNumber: -1,
            })
        );

        // Register methods
        assert(this.serializer.schemaList.some(item => item.className === this.className && item.methodName));
        assert(this.serializer.schemaList.some(item => item.className === "Metadata" && item.methodName));
        this.setupInstance();
    }

    async connect<T>(): Promise<Service & T> {
        await this.initSocket();
        return this.proxyInstance as Service & T;
    }

    async initSocket() {
        const payload = {
            "projectId": getProjectName(),
            "clientId": process.pid,
            "parentId": process.ppid,
            "shellId": getShellId(),
            "entryPoint": require.main.filename,
        }
        this.mainReceiver = new SocketReceiver();
        this.mainSocket = net.createConnection(
            { 
                port: 9001,
                host: 'localhost',
            }, 
            () => {
                this.mainReceiver.connect(this.mainSocket)
            }
        );
        this.mainSocket.on('data', (data) => {
            this.mainReceiver.data(data);
        });
        this.mainSocket.on('end', () => {
            this.mainReceiver.end();
        });
        this.mainSocket.on('error', (err) => {
            this.mainReceiver.error(err);
        });
        await this.mainReceiver.waitConnected1();
        this.mainSocket.write(
            createPayload(
                'POST /Metadata/connectClient HTTP/1.1',
                `Host: ${this.host}`,
                'User-Agent: nativerpc/1.0.0',
                'Content-Type: application/json',
                'Content-Length: $payloadSize',
                'Connection: keep-alive',
                'Sender-Id: connect',
                payload
            )
        );
        const { status, headers, body, error } = await this.mainReceiver.wait(2);
        assert(status === 200);
        this.connectionId = body["connectionId"];
        this.mainReceiver.setConnected();
    }

    setupInstance() {
        for (const methodInfo of this.serializer.schemaList) {
            if (methodInfo.className !== this.className || !methodInfo.methodName) {
                continue;
            }
            this.proxyInstance[methodInfo.methodName] = async (param) => {
                return await this.clientCall(this.className, methodInfo.methodName, methodInfo.methodRequest, methodInfo.methodResponse, param);
            }
        }
        for (const methodInfo of this.serializer.schemaList) {
            if (methodInfo.className !== "Metadata" || !methodInfo.methodName) {
                continue;
            }
            this.proxyInstance[methodInfo.methodName] = async (param) => {
                return await this.clientCall('Metadata', methodInfo.methodName, methodInfo.methodRequest, methodInfo.methodResponse, param);
            }
        }
    }

    async clientCall(className, methodName, reqName, resName, param) {
        await this.mainReceiver.waitConnected();
        const reqJson = this.serializer.toJson(reqName, param);
        this.mainSocket.write(
            createPayload(
                `POST /${className}/${methodName} HTTP/1.1`,
                `Host: ${this.host}`,
                'User-Agent: nativerpc/1.0.0',
                'Content-Type: application/json',
                'Content-Length: $payloadSize',
                'Connection: keep-alive',
                'Sender-Id: call',
                reqJson
            )
        );

        const { status, headers, body, error } = await this.mainReceiver.wait(3);
        // Throw client errors
        if (status != 200) {
            const finalError = error ?? body?.error;
            throw new Error(`Client error: ${finalError}, code=${status}, call=${className}.${methodName}`)
        }
        return this.serializer.fromJson(resName, body);
    }

    async close() {
        await this.mainReceiver.waitConnected();
        if (this.mainSocket.closed) {
            return;
        }
        const payload = {
            "projectId": getProjectName(),
            "clientId": process.pid,
            "parentId": process.ppid,
            "shellId": getShellId(),
            "entryPoint": require.main.filename,
            "connectionId": this.connectionId,
        }
        this.mainSocket.write(
            createPayload(
                'POST /Metadata/closeClient HTTP/1.1',
                `Host: ${this.host}`,
                'User-Agent: nativerpc/1.0.0',
                'Content-Type: application/json',
                'Content-Length: $payloadSize',
                'Connection: keep-alive',
                'Sender-Id: close',
                payload
            )
        );
        const { status, headers, body, error } = await this.mainReceiver.wait(4);
        this.mainSocket.destroy();
        this.mainSocket = null;
    }
}

