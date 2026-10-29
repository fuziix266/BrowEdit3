import * as net from 'net';
import { EventEmitter } from 'events';

export class BrowEditIpcClient extends EventEmitter {
  private socket: net.Socket | null = null;
  private port: number;
  private host: string;
  private requestId = 0;
  private pendingRequests = new Map<number, {
    resolve: (val: any) => void;
    reject: (err: Error) => void;
    timer: NodeJS.Timeout;
  }>();
  private buffer = '';
  private connected = false;
  private connecting = false;

  constructor(host = '127.0.0.1', port = 38200) {
    super();
    this.host = host;
    this.port = port;
  }

  async connect(): Promise<boolean> {
    if (this.connected && this.socket) return true;
    if (this.connecting) {
      return new Promise((resolve) => {
        this.once('connect_result', (ok) => resolve(ok));
      });
    }

    this.connecting = true;
    return new Promise((resolve) => {
      const socket = new net.Socket();
      this.socket = socket;

      const timeout = setTimeout(() => {
        socket.destroy();
        this.connecting = false;
        this.connected = false;
        this.emit('connect_result', false);
        resolve(false);
      }, 3000);

      socket.connect(this.port, this.host, () => {
        clearTimeout(timeout);
        this.connected = true;
        this.connecting = false;
        this.emit('connect_result', true);
        resolve(true);
      });

      socket.on('data', (data) => {
        this.buffer += data.toString('utf-8');
        let idx = -1;
        while ((idx = this.buffer.indexOf('\n')) !== -1) {
          const line = this.buffer.slice(0, idx).trim();
          this.buffer = this.buffer.slice(idx + 1);
          if (line.length === 0) continue;

          try {
            const parsed = JSON.parse(line);
            const reqId = parsed.id;
            const pending = this.pendingRequests.get(reqId);
            if (pending) {
              clearTimeout(pending.timer);
              this.pendingRequests.delete(reqId);
              if (parsed.error) {
                pending.reject(new Error(parsed.error));
              } else {
                pending.resolve(parsed.result);
              }
            }
          } catch (e) {
            console.error('[BrowEditIpcClient] Failed to parse message:', line, e);
          }
        }
      });

      socket.on('error', (err) => {
        this.connected = false;
        this.connecting = false;
      });

      socket.on('close', () => {
        this.connected = false;
        this.connecting = false;
        this.socket = null;
        for (const [id, req] of this.pendingRequests.entries()) {
          clearTimeout(req.timer);
          req.reject(new Error('Conexión con BrowEdit3 cerrada.'));
        }
        this.pendingRequests.clear();
      });
    });
  }

  async send(method: string, params: Record<string, any> = {}, timeoutMs = 15000): Promise<any> {
    const isConn = await this.connect();
    if (!isConn || !this.socket) {
      throw new Error(
        `No se pudo conectar a BrowEdit3 en ${this.host}:${this.port}. Asegúrate de que BrowEdit3 esté abierto con el puente MCP activo.`
      );
    }

    const id = ++this.requestId;
    const payload = JSON.stringify({ id, method, params }) + '\n';

    return new Promise((resolve, reject) => {
      const timer = setTimeout(() => {
        this.pendingRequests.delete(id);
        reject(new Error(`Timeout de la operación '${method}' en BrowEdit3.`));
      }, timeoutMs);

      this.pendingRequests.set(id, { resolve, reject, timer });
      this.socket?.write(payload, 'utf-8', (err) => {
        if (err) {
          clearTimeout(timer);
          this.pendingRequests.delete(id);
          reject(err);
        }
      });
    });
  }

  disconnect() {
    if (this.socket) {
      this.socket.destroy();
      this.socket = null;
    }
    this.connected = false;
  }
}
