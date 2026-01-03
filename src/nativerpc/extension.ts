/**
 *  Native RPC Typescript Language Extension
 * 
 *      ClassType
 *      getHeaderMap
 *      createPayload
 *      SocketReceiver
 */
const assert = require('node:assert');
import {IncomingHttpHeaders} from 'http';

export type ClassType<T> = new (...args: any[]) => T;

export function getHeaderMap(headers: string | IncomingHttpHeaders, names: string[]) {
    const result = {};
    if (typeof headers === 'string') {
        headers = headers.toLowerCase();
        for (const name of names) {
            const hdr_name = name.toLowerCase() + ":";
            if (headers.indexOf(hdr_name) === -1) {
                result[name] = "";
                continue;
            }
            const idx1 = headers.indexOf(hdr_name) + hdr_name.length;
            const idx2 = headers.indexOf("\n", idx1)
            result[name] = headers.substring(idx1, idx2 !== -1 ? idx2 : undefined).trim();
        }
    } else {
        for (const name of names) {
            const hdr_name = name.toLowerCase();
            if (!(headers[hdr_name] ?? headers[name])) {
                result[name] = "";
                continue;
            }
            result[name] = headers[hdr_name] ?? headers[name];
        }
    }
    return result;
}

export function createPayload(...parts) {
    assert(Object.getPrototypeOf(parts[parts.length - 2]).constructor.name == "String");
    assert(Object.getPrototypeOf(parts[parts.length - 1]).constructor.name == "Object");
    const encoder = new TextEncoder();
    let headers = parts.slice(0, parts.length - 2).join('\r\n') + '\r\n\r\n';
    const body = encoder.encode(JSON.stringify(parts[parts.length - 1]));
    headers = headers.replace('$payloadSize', body.byteLength.toString());
    const headers2 = encoder.encode(headers);

    var result = new Uint8Array(headers2.length + body.length);
    result.set(headers2);
    result.set(body, headers2.length);
    return result;
}

export class SocketReceiver {
    waiter: any;
    preConnected: boolean;
    connected: boolean;
    socket: any;
    closed: boolean;
    body: any;

    constructor() {
        this.waiter = null;
        this.preConnected = false;
        this.connected = false;
        this.socket = null;
        this.closed = false;
    }
    connect(socket) {
        this.socket = socket;
        this.preConnected = true;
    }
    setConnected() {
        this.connected = true;
    }
    async waitConnected1() {
        while (true) {
            if (this.preConnected || this.closed || (this.socket && this.socket.closed)) {
                break;
            }
            await new Promise((resolve) => { setTimeout(resolve, 50) });
        }
    }
    async waitConnected() {
        while (true) {
            if (this.connected || this.closed || (this.socket && this.socket.closed)) {
                break;
            }
            await new Promise((resolve) => { setTimeout(resolve, 50) });
        }
    }
    data(value) {
        if (!this.waiter) {
            throw new Error('No receiver in SocketReceiver')
        }
        if (this.body) {
            throw new Error('Duplicate data in SocketReceiver')
        }
        const combined = value.toString();
        const first = combined.indexOf('\r\n');
        const first2 = combined.substring(0, first).split(' ');
        const middle = combined.indexOf('\r\n\r\n');
        const headers = combined.substring(0, middle);
        assert(getHeaderMap(headers, ['Transfer-Encoding'])['Transfer-Encoding'] == "chunked")
        const sizeEnd = combined.indexOf('\r\n', middle + 4);
        const sizeNum = parseInt(combined.substring(middle + 4, sizeEnd), 16);
        const body = combined.substring(sizeEnd + 2, sizeEnd + 2 + sizeNum);
        const body2 = JSON.parse(body);
        const endSign = combined.substring(sizeEnd + 2 + sizeNum).trim();
        assert(endSign === "0");
        const protocol = first2[0];
        const status = parseInt(first2[1]);
        const statusMsg = first2.slice(2).join(' ');
        const w = this.waiter;
        this.waiter = null;
        w.continue({ 
            status,
            headers,
            body: body2,
            error: null,
        });
    }
    async wait(id): Promise<{status: number, headers: string, body: any, error: string}> {
        if (!this.preConnected || this.closed || this.socket.closed) {
            return {
                status: 501,
                headers: '',
                body: null,
                error: 'Client lost',
            };
        }
        assert(!this.closed);
        assert(!this.socket || !this.socket.closed);
        if (this.waiter) {
            throw new Error(`Duplicate receiver in SocketReceiver, id=${id}`);
        }
        return await new Promise<{status: number, headers: string, body: any, error: string}>(
            (resolve, reject) => {
                this.waiter = {
                    active: true,
                    continue: ({ status, headers, body, error }) => {
                        resolve({ 
                            status, 
                            headers, 
                            body, 
                            error
                        })
                    },
                    throwError: (err) => {
                        reject(err);
                    },
                };
            }
        );
    }
    end() {
        this.closed = true;
        if (!this.waiter) {
            return;
        }
        const w = this.waiter;
        this.waiter = null;
        w.continue({
            status: 502, 
            headers: '', 
            body: null, 
            error: 'Socket closed'
        });
    }
    error(err) {
        this.closed = true;
        if (!this.waiter) {
            return;
        }
        const w = this.waiter;
        this.waiter = null;
        w.continue({
            status: 503, 
            headers: '', 
            body: null, 
            error: err
        });
    }
}

