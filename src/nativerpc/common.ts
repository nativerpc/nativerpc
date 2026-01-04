/**
 *  Native RPC Common
 * 
 *      CONFIG_NAME
 *      COMMON_TYPES
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
 * 
 *      SchemaFileInfo
 *      QueryInfo
 */
import { strict as assert } from "node:assert";
import * as fs from "node:fs";
import * as path from "node:path";
import { spawnSync } from "node:child_process";
import { 
    type ClassType
} from "./extension";

export const CONFIG_NAME = 'workspace.json';
export const COMMON_TYPES = {
    'dict': Object,
    'str': String,
    'bool': Boolean,
    'int': Number,
    'float': Number,
    'list': Array,
};

export class SchemaInfo {
    projectName: string;
    className: string;
    fieldName: string;
    fieldType: string;
    methodName: string;
    methodRequest: string;
    methodResponse: string;
    idNumber: number;

    constructor(options) {
        this.projectName = options.projectName ?? '';
        this.className = options.className;
        this.fieldName = options.fieldName ?? "";
        this.fieldType = options.fieldType ?? "";
        this.methodName = options.methodName ?? "";
        this.methodRequest = options.methodRequest ?? "";
        this.methodResponse = options.methodResponse ?? "";
        this.idNumber = options.idNumber ?? -1;
        assert(this.className);
        assert(this.methodName || this.fieldName);
    }
}

export class FieldInfo {
    className: string;
    classType: ClassType<any>;
    fieldName: string;
    fieldType: string;
    idNumber: number;

    constructor(options) {
        this.className = options.className ?? '';
        this.classType = options.classType ?? null;
        this.fieldName = options.fieldName ?? '';
        this.fieldType = options.fieldType ?? '';
        this.idNumber = options.idNumber ?? -1;
        assert(this.className);
        assert(this.fieldName);
    }
}

export class MethodInfo {
    className: string;
    classType: ClassType<any>;
    classInstance: any;
    methodName: string;
    methodCall: any;
    methodRequest: string;
    methodResponse: string;
    idNumber: number;

    constructor(options) {
        this.className = options.className ?? '';
        this.classType = options.classType ?? null;
        this.classInstance = options.classInstance;
        this.methodName = options.methodName ?? '';
        this.methodCall = options.methodCall;
        this.methodRequest = options.methodRequest;
        this.methodResponse = options.methodResponse;
        this.idNumber = options.idNumber ?? -1;
        assert(this.classInstance);
        assert(this.methodCall);
        assert(this.methodRequest);
        assert(this.methodResponse);
    }
}

export type Options = {
    service: any;
    host: [string, number];
}

export class Connection {
    connectionId: number;
    socket: any;
    address: any;
    stime: number;
    wtime: number;
    readBuffer: any;
    projectId: string;
    closed: boolean;
    error: any;
    messageCount: number;
    senderId: string;
    callId: string;
    processId: string;
    parentId: string;
    shellId: string;
    entryPoint: string;

    constructor(options) {
        this.connectionId = options.connectionId ?? 0;
        this.socket = options.socket ?? null;
        this.address = options.address ?? [];
        this.stime = options.stime ?? 0;
        this.wtime = options.wtime ?? 0;
        this.readBuffer = '';
        this.projectId = options.projectId ?? 0;
        this.closed = options.closed ?? false;
        this.error = null;
        this.messageCount = options.messageCount ?? 0;
        this.senderId = options.senderId ?? '';
        this.callId = options.callId ?? '';
        this.processId = options.processId ?? '';
        this.parentId = options.parentId ?? '';
        this.shellId = options.shellId ?? '';
        this.entryPoint = options.entryPoint ?? '';
    }
}

export class Service {
    client: any;

    constructor(client) {
        this.client = client;
    }
    async getMetadata(param) {
        assert(false);
        return {};
    }
    async close() {
        await this.client.close();
    }
}

export function verifyPython() {
    const { status, stdout, stderr } = spawnSync(
        'python',
        ['--version']
    );
    if (status !== 0) {
        console.error('Failed to find python')
        process.exit(1)
    }
}

export function getProjectName() {
    let folder = path.dirname(require.main.path)
    while (true) {
        if (fs.existsSync(path.join(folder, 'package.json')) ||
            fs.existsSync(path.join(folder, 'pyproject.toml')) ||
            fs.existsSync(path.join(folder, 'tsconfig.json')) ||
            fs.existsSync(path.join(folder, '.git'))) {
            break;
        }
        if (fs.existsSync(path.join(path.dirname(folder), 'workspace.json'))) {
            break;
        }
        folder = path.dirname(folder)
    }
    return path.basename(folder);
}

export function getProjectPath() {
    let folder = path.dirname(require.main.path)
    while (true) {
        if (fs.existsSync(path.join(folder, 'package.json')) ||
            fs.existsSync(path.join(folder, 'pyproject.toml')) ||
            fs.existsSync(path.join(folder, 'tsconfig.json')) ||
            fs.existsSync(path.join(folder, '.git'))) {
            break;
        }
        if (fs.existsSync(path.join(path.dirname(folder), 'workspace.json'))) {
            break;
        }
        folder = path.dirname(folder);
    }
    return folder;
}

export function getEntryPoint() {
    return require.main.path;
}

export function getMessageFiles(projectPath) {
    const schemaName = "common";
    const result = [path.join(projectPath, "src", schemaName + ".ts")]
    assert(fs.existsSync(result[0]))
    return result
}

export function parseSchemaList(file) {
    const { status, stdout, stderr } = spawnSync(
        'python',
        [
            '-u',
            path.join(__dirname, 'parser.py'),
            file
        ]
    );
    // console.log(1111, stdout.toString())
    // console.log(2222, stderr.toString())
    assert(status === 0);
    return JSON.parse(stdout.toString());
}

export function getShellId() {
    const { status, stdout, stderr } = spawnSync(
        'python',
        [
            '-c',
            "import psutil; import os; print(':'.join([str(x.pid) for x in psutil.Process(os.getppid()).parent().parent().parents()]))",
        ]
    );
    assert(status === 0);
    return stdout.toString();
}

export class SchemaFileInfo {
    projectName: string;
    projFiles: string[];
    schemaFile: string;

    constructor({ projectName, projFiles, schemaFile }) {
        this.projectName = projectName;
        this.projFiles = projFiles;
        this.schemaFile = schemaFile;
    }
}

export class QueryInfo {
    port: number;
    promise: any;
    queryResult: any;

    constructor({ port, promise, queryResult }) {
        this.port = port;
        this.promise = promise;
        this.queryResult = queryResult;
    }
}
