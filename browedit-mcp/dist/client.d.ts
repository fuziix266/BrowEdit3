import { EventEmitter } from 'events';
export declare class BrowEditIpcClient extends EventEmitter {
    private socket;
    private port;
    private host;
    private requestId;
    private pendingRequests;
    private buffer;
    private connected;
    private connecting;
    constructor(host?: string, port?: number);
    connect(): Promise<boolean>;
    send(method: string, params?: Record<string, any>, timeoutMs?: number): Promise<any>;
    disconnect(): void;
}
