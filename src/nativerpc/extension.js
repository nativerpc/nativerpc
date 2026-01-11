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
exports.SocketReceiver = void 0;
exports.getHeaderMap = getHeaderMap;
exports.createPayload = createPayload;
/**
 *  Native RPC Typescript Language Extension
 *
 *      ClassType
 *      getHeaderMap
 *      createPayload
 *      SocketReceiver
 */
var assert = require('node:assert');
function getHeaderMap(headers, names) {
    var _a, _b;
    var result = {};
    if (typeof headers === 'string') {
        headers = headers.toLowerCase();
        for (var _i = 0, names_1 = names; _i < names_1.length; _i++) {
            var name_1 = names_1[_i];
            var colonName = name_1.toLowerCase() + ":";
            if (headers.indexOf(colonName) === -1) {
                result[name_1] = "";
                continue;
            }
            var startIndex = headers.indexOf(colonName) + colonName.length;
            var endIndex = headers.indexOf("\n", startIndex);
            result[name_1] = headers.substring(startIndex, endIndex !== -1 ? endIndex : undefined).trim();
        }
    }
    else {
        for (var _c = 0, names_2 = names; _c < names_2.length; _c++) {
            var name_2 = names_2[_c];
            var lowerName = name_2.toLowerCase();
            if (!((_a = headers[lowerName]) !== null && _a !== void 0 ? _a : headers[name_2])) {
                result[name_2] = "";
                continue;
            }
            result[name_2] = (_b = headers[lowerName]) !== null && _b !== void 0 ? _b : headers[name_2];
        }
    }
    return result;
}
function createPayload() {
    var parts = [];
    for (var _i = 0; _i < arguments.length; _i++) {
        parts[_i] = arguments[_i];
    }
    assert(Object.getPrototypeOf(parts[parts.length - 2]).constructor.name == "String");
    assert(Object.getPrototypeOf(parts[parts.length - 1]).constructor.name == "Object");
    var encoder = new TextEncoder();
    var headers = parts.slice(0, parts.length - 2).join('\r\n') + '\r\n\r\n';
    var body = encoder.encode(JSON.stringify(parts[parts.length - 1]));
    headers = headers.replace('$payloadSize', body.byteLength.toString());
    var headers2 = encoder.encode(headers);
    var result = new Uint8Array(headers2.length + body.length);
    result.set(headers2);
    result.set(body, headers2.length);
    return result;
}
var SocketReceiver = /** @class */ (function () {
    function SocketReceiver() {
        this.waiter = null;
        this.preConnected = false;
        this.connected = false;
        this.socket = null;
        this.closed = false;
    }
    SocketReceiver.prototype.connect = function (socket) {
        this.socket = socket;
        this.preConnected = true;
    };
    SocketReceiver.prototype.setConnected = function () {
        this.connected = true;
    };
    SocketReceiver.prototype.waitConnected1 = function () {
        return __awaiter(this, void 0, void 0, function () {
            return __generator(this, function (_a) {
                switch (_a.label) {
                    case 0:
                        if (!true) return [3 /*break*/, 2];
                        if (this.preConnected || this.closed || (this.socket && this.socket.closed)) {
                            return [3 /*break*/, 2];
                        }
                        return [4 /*yield*/, new Promise(function (resolve) { setTimeout(resolve, 50); })];
                    case 1:
                        _a.sent();
                        return [3 /*break*/, 0];
                    case 2: return [2 /*return*/];
                }
            });
        });
    };
    SocketReceiver.prototype.waitConnected = function () {
        return __awaiter(this, void 0, void 0, function () {
            return __generator(this, function (_a) {
                switch (_a.label) {
                    case 0:
                        if (!true) return [3 /*break*/, 2];
                        if (this.connected || this.closed || (this.socket && this.socket.closed)) {
                            return [3 /*break*/, 2];
                        }
                        return [4 /*yield*/, new Promise(function (resolve) { setTimeout(resolve, 50); })];
                    case 1:
                        _a.sent();
                        return [3 /*break*/, 0];
                    case 2: return [2 /*return*/];
                }
            });
        });
    };
    SocketReceiver.prototype.data = function (value) {
        if (!this.waiter) {
            throw new Error('No receiver in SocketReceiver');
        }
        if (this.body) {
            throw new Error('Duplicate data in SocketReceiver');
        }
        var combined = value.toString();
        var first = combined.indexOf('\r\n');
        var first2 = combined.substring(0, first).split(' ');
        var middle = combined.indexOf('\r\n\r\n');
        var headers = combined.substring(0, middle);
        assert(getHeaderMap(headers, ['Transfer-Encoding'])['Transfer-Encoding'] == "chunked");
        var sizeEnd = combined.indexOf('\r\n', middle + 4);
        var sizeNum = parseInt(combined.substring(middle + 4, sizeEnd), 16);
        var body = combined.substring(sizeEnd + 2, sizeEnd + 2 + sizeNum);
        var body2 = JSON.parse(body);
        var endSign = combined.substring(sizeEnd + 2 + sizeNum).trim();
        assert(endSign === "0");
        var protocol = first2[0];
        var status = parseInt(first2[1]);
        var statusMsg = first2.slice(2).join(' ');
        var w = this.waiter;
        this.waiter = null;
        w.continue({
            status: status,
            headers: headers,
            body: body2,
            error: null,
        });
    };
    SocketReceiver.prototype.wait = function (id) {
        return __awaiter(this, void 0, void 0, function () {
            var _this = this;
            return __generator(this, function (_a) {
                switch (_a.label) {
                    case 0:
                        if (!this.preConnected && !this.closed && !this.socket.closed) {
                            throw new Error("Connection missing");
                        }
                        if (!this.preConnected || this.closed || this.socket.closed) {
                            return [2 /*return*/, {
                                    status: 501,
                                    headers: '',
                                    body: null,
                                    error: 'Client lost',
                                }];
                        }
                        assert(!this.closed);
                        assert(!this.socket || !this.socket.closed);
                        if (this.waiter) {
                            throw new Error("Duplicate receiver in SocketReceiver, id=".concat(id));
                        }
                        return [4 /*yield*/, new Promise(function (resolve, reject) {
                                _this.waiter = {
                                    active: true,
                                    continue: function (_a) {
                                        var status = _a.status, headers = _a.headers, body = _a.body, error = _a.error;
                                        resolve({
                                            status: status,
                                            headers: headers,
                                            body: body,
                                            error: error
                                        });
                                    },
                                    throwError: function (err) {
                                        reject(err);
                                    },
                                };
                            })];
                    case 1: return [2 /*return*/, _a.sent()];
                }
            });
        });
    };
    SocketReceiver.prototype.end = function () {
        this.closed = true;
        if (!this.waiter) {
            return;
        }
        var w = this.waiter;
        this.waiter = null;
        w.continue({
            status: 502,
            headers: '',
            body: null,
            error: 'Socket closed'
        });
    };
    SocketReceiver.prototype.error = function (err) {
        this.closed = true;
        if (!this.waiter) {
            return;
        }
        var w = this.waiter;
        this.waiter = null;
        w.continue({
            status: 503,
            headers: '',
            body: null,
            error: err
        });
    };
    return SocketReceiver;
}());
exports.SocketReceiver = SocketReceiver;
