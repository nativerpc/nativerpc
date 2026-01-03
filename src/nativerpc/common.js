"use strict";
var __awaiter = (this && this.__awaiter) || function (thisArg, _arguments, P, generator) {
    function adopt(value) { return value instanceof P ? value : new P(function (resolve) { resolve(value); }); }
    return new (P || (P = Promise))(function (resolve, reject) {
        function fulfilled(value) { try { step(generator.next(value)); } catch (e) { reject(e); } }
        function rejected(value) { try { step(generator["throw"](value)); } catch (e) { reject(e); } }
        function step(result) { result.done ? resolve(result.value) : adopt(result.value).then(fulfilled, rejected); }
        step((generator = generator.apply(thisArg, _arguments || [])).next());
    });
};
var __generator = (this && this.__generator) || function (thisArg, body) {
    var _ = { label: 0, sent: function() { if (t[0] & 1) throw t[1]; return t[1]; }, trys: [], ops: [] }, f, y, t, g = Object.create((typeof Iterator === "function" ? Iterator : Object).prototype);
    return g.next = verb(0), g["throw"] = verb(1), g["return"] = verb(2), typeof Symbol === "function" && (g[Symbol.iterator] = function() { return this; }), g;
    function verb(n) { return function (v) { return step([n, v]); }; }
    function step(op) {
        if (f) throw new TypeError("Generator is already executing.");
        while (g && (g = 0, op[0] && (_ = 0)), _) try {
            if (f = 1, y && (t = op[0] & 2 ? y["return"] : op[0] ? y["throw"] || ((t = y["return"]) && t.call(y), 0) : y.next) && !(t = t.call(y, op[1])).done) return t;
            if (y = 0, t) op = [op[0] & 2, t.value];
            switch (op[0]) {
                case 0: case 1: t = op; break;
                case 4: _.label++; return { value: op[1], done: false };
                case 5: _.label++; y = op[1]; op = [0]; continue;
                case 7: op = _.ops.pop(); _.trys.pop(); continue;
                default:
                    if (!(t = _.trys, t = t.length > 0 && t[t.length - 1]) && (op[0] === 6 || op[0] === 2)) { _ = 0; continue; }
                    if (op[0] === 3 && (!t || (op[1] > t[0] && op[1] < t[3]))) { _.label = op[1]; break; }
                    if (op[0] === 6 && _.label < t[1]) { _.label = t[1]; t = op; break; }
                    if (t && _.label < t[2]) { _.label = t[2]; _.ops.push(op); break; }
                    if (t[2]) _.ops.pop();
                    _.trys.pop(); continue;
            }
            op = body.call(thisArg, _);
        } catch (e) { op = [6, e]; y = 0; } finally { f = t = 0; }
        if (op[0] & 5) throw op[1]; return { value: op[0] ? op[1] : void 0, done: true };
    }
};
Object.defineProperty(exports, "__esModule", { value: true });
exports.QueryInfo = exports.SchemaFileInfo = exports.Service = exports.Connection = exports.MethodInfo = exports.FieldInfo = exports.SchemaInfo = exports.COMMON_TYPES = exports.CONFIG_NAME = void 0;
exports.verifyPython = verifyPython;
exports.getProjectName = getProjectName;
exports.getProjectPath = getProjectPath;
exports.getEntryPoint = getEntryPoint;
exports.getMessageFiles = getMessageFiles;
exports.parseSchemaList = parseSchemaList;
exports.getShellId = getShellId;
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
var assert = require('node:assert');
var fs = require('node:fs');
var path = require('node:path');
var spawnSync = require('node:child_process').spawnSync;
exports.CONFIG_NAME = 'workspace.json';
exports.COMMON_TYPES = {
    'dict': Object,
    'str': String,
    'bool': Boolean,
    'int': Number,
    'float': Number,
    'list': Array,
};
var SchemaInfo = /** @class */ (function () {
    function SchemaInfo(options) {
        var _a, _b, _c, _d, _e, _f, _g;
        this.projectName = (_a = options.projectName) !== null && _a !== void 0 ? _a : '';
        this.className = options.className;
        this.fieldName = (_b = options.fieldName) !== null && _b !== void 0 ? _b : "";
        this.fieldType = (_c = options.fieldType) !== null && _c !== void 0 ? _c : "";
        this.methodName = (_d = options.methodName) !== null && _d !== void 0 ? _d : "";
        this.methodRequest = (_e = options.methodRequest) !== null && _e !== void 0 ? _e : "";
        this.methodResponse = (_f = options.methodResponse) !== null && _f !== void 0 ? _f : "";
        this.idNumber = (_g = options.idNumber) !== null && _g !== void 0 ? _g : -1;
        assert(this.className);
        assert(this.methodName || this.fieldName);
    }
    return SchemaInfo;
}());
exports.SchemaInfo = SchemaInfo;
var FieldInfo = /** @class */ (function () {
    function FieldInfo(options) {
        var _a, _b, _c, _d, _e;
        this.className = (_a = options.className) !== null && _a !== void 0 ? _a : '';
        this.classType = (_b = options.classType) !== null && _b !== void 0 ? _b : null;
        this.fieldName = (_c = options.fieldName) !== null && _c !== void 0 ? _c : '';
        this.fieldType = (_d = options.fieldType) !== null && _d !== void 0 ? _d : '';
        this.idNumber = (_e = options.idNumber) !== null && _e !== void 0 ? _e : -1;
        assert(this.className);
        assert(this.fieldName);
    }
    return FieldInfo;
}());
exports.FieldInfo = FieldInfo;
var MethodInfo = /** @class */ (function () {
    function MethodInfo(options) {
        var _a, _b, _c, _d;
        this.className = (_a = options.className) !== null && _a !== void 0 ? _a : '';
        this.classType = (_b = options.classType) !== null && _b !== void 0 ? _b : null;
        this.classInstance = options.classInstance;
        this.methodName = (_c = options.methodName) !== null && _c !== void 0 ? _c : '';
        this.methodCall = options.methodCall;
        this.methodRequest = options.methodRequest;
        this.methodResponse = options.methodResponse;
        this.idNumber = (_d = options.idNumber) !== null && _d !== void 0 ? _d : -1;
        assert(this.classInstance);
        assert(this.methodCall);
        assert(this.methodRequest);
        assert(this.methodResponse);
    }
    return MethodInfo;
}());
exports.MethodInfo = MethodInfo;
var Connection = /** @class */ (function () {
    function Connection(options) {
        var _a, _b, _c, _d, _e, _f, _g, _h, _j, _k, _l, _m, _o, _p;
        this.connectionId = (_a = options.connectionId) !== null && _a !== void 0 ? _a : 0;
        this.socket = (_b = options.socket) !== null && _b !== void 0 ? _b : null;
        this.address = (_c = options.address) !== null && _c !== void 0 ? _c : [];
        this.stime = (_d = options.stime) !== null && _d !== void 0 ? _d : 0;
        this.wtime = (_e = options.wtime) !== null && _e !== void 0 ? _e : 0;
        this.readBuffer = '';
        this.projectId = (_f = options.projectId) !== null && _f !== void 0 ? _f : 0;
        this.closed = (_g = options.closed) !== null && _g !== void 0 ? _g : false;
        this.error = null;
        this.messageCount = (_h = options.messageCount) !== null && _h !== void 0 ? _h : 0;
        this.senderId = (_j = options.senderId) !== null && _j !== void 0 ? _j : '';
        this.callId = (_k = options.callId) !== null && _k !== void 0 ? _k : '';
        this.processId = (_l = options.processId) !== null && _l !== void 0 ? _l : '';
        this.parentId = (_m = options.parentId) !== null && _m !== void 0 ? _m : '';
        this.shellId = (_o = options.shellId) !== null && _o !== void 0 ? _o : '';
        this.entryPoint = (_p = options.entryPoint) !== null && _p !== void 0 ? _p : '';
    }
    return Connection;
}());
exports.Connection = Connection;
var Service = /** @class */ (function () {
    function Service(client) {
        this.client = client;
    }
    Service.prototype.getMetadata = function (param) {
        return __awaiter(this, void 0, void 0, function () {
            return __generator(this, function (_a) {
                assert(false);
                return [2 /*return*/, {}];
            });
        });
    };
    Service.prototype.close = function () {
        return __awaiter(this, void 0, void 0, function () {
            return __generator(this, function (_a) {
                switch (_a.label) {
                    case 0: return [4 /*yield*/, this.client.close()];
                    case 1:
                        _a.sent();
                        return [2 /*return*/];
                }
            });
        });
    };
    return Service;
}());
exports.Service = Service;
function verifyPython() {
    var _a = spawnSync('python', ['--version']), status = _a.status, stdout = _a.stdout, stderr = _a.stderr;
    if (status !== 0) {
        console.error('Failed to find python');
        process.exit(1);
    }
}
function getProjectName() {
    var folder = path.dirname(require.main.path);
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
    return path.basename(folder);
}
function getProjectPath() {
    var folder = path.dirname(require.main.path);
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
function getEntryPoint() {
    return require.main.path;
}
function getMessageFiles(projectPath) {
    var schemaName = "common";
    var result = [path.join(projectPath, "src", schemaName + ".ts")];
    assert(fs.existsSync(result[0]));
    return result;
}
function parseSchemaList(file) {
    var _a = spawnSync('python', [
        '-u',
        path.join(__dirname, 'parser.py'),
        file
    ]), status = _a.status, stdout = _a.stdout, stderr = _a.stderr;
    // console.log(1111, stdout.toString())
    // console.log(2222, stderr.toString())
    assert(status === 0);
    return JSON.parse(stdout.toString());
}
function getShellId() {
    var _a = spawnSync('python', [
        '-c',
        "import psutil; import os; print(':'.join([str(x.pid) for x in psutil.Process(os.getppid()).parent().parent().parents()]))",
    ]), status = _a.status, stdout = _a.stdout, stderr = _a.stderr;
    assert(status === 0);
    return stdout.toString();
}
var SchemaFileInfo = /** @class */ (function () {
    function SchemaFileInfo(_a) {
        var projectName = _a.projectName, projFiles = _a.projFiles, schemaFile = _a.schemaFile;
        this.projectName = projectName;
        this.projFiles = projFiles;
        this.schemaFile = schemaFile;
    }
    return SchemaFileInfo;
}());
exports.SchemaFileInfo = SchemaFileInfo;
var QueryInfo = /** @class */ (function () {
    function QueryInfo(_a) {
        var port = _a.port, promise = _a.promise, queryResult = _a.queryResult;
        this.port = port;
        this.promise = promise;
        this.queryResult = queryResult;
    }
    return QueryInfo;
}());
exports.QueryInfo = QueryInfo;
