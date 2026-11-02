import { DurableObject } from 'cloudflare:workers';

// Servidor de salas de Mario 64 Cooperativo.
// Un Worker recibe la conexion WebSocket y la entrega al Durable Object de la sala (una sala = un objeto).
// El servidor NO simula el juego: solo valida, impone la identidad de cada jugador y retransmite.

const PROTOCOL = 1;
const MAX_PLAYERS = 16;
const MAX_MSG = 1024;          // bytes por mensaje entrante
const MAX_CHARS = 8;           // personajes disponibles (0..7)
const RATE_PER_SEC = 40;       // mensajes/s por jugador (el cliente envia ~15)
const BURST = 60;
const ROOM_RE = /[^a-z0-9_-]/g;
const NAME_RE = /[^\p{L}\p{N} _-]/gu;

const JSON_HEADERS = { 'content-type': 'application/json; charset=utf-8', 'access-control-allow-origin': '*' };

export default {
  async fetch(request, env) {
    const url = new URL(request.url);

    if (url.pathname === '/health') return new Response('ok');
    if (url.pathname === '/') return new Response(LANDING, { headers: { 'content-type': 'text/html; charset=utf-8' } });
    if (url.pathname === '/info') {
      return new Response(JSON.stringify({ name: 'mario64-coop', protocol: PROTOCOL, ws: '/ws?room=SALA&name=APODO&ch=0' }), { headers: JSON_HEADERS });
    }
    if (url.pathname === '/save') return saveRoute(request, url, env);
    if (url.pathname !== '/ws') return new Response('No encontrado', { status: 404 });
    if (request.headers.get('Upgrade') !== 'websocket') return new Response('Se esperaba WebSocket', { status: 426 });

    // Clave opcional: si el Worker tiene el secreto ROOM_KEY, hay que enviarla (?key=...)
    if (env.ROOM_KEY && url.searchParams.get('key') !== env.ROOM_KEY) return new Response('Clave incorrecta', { status: 401 });

    const room = (url.searchParams.get('room') || 'publica').toLowerCase().replace(ROOM_RE, '').slice(0, 32) || 'publica';
    return env.ROOM.get(env.ROOM.idFromName(room)).fetch(request);
  },
};

const SAVE_MAX = 1024;  // el archivo de partida mide 512 bytes
const CODE_RE = /^[a-z0-9]{12,40}$/;

// Partida en la nube: solo el archivo de guardado, identificado por un codigo secreto que genera el jugador.
async function saveRoute(request, url, env) {
  const cors = { 'access-control-allow-origin': '*', 'access-control-allow-methods': 'GET, PUT, OPTIONS', 'access-control-allow-headers': '*' };
  if (request.method === 'OPTIONS') return new Response(null, { status: 204, headers: cors });
  const code = (url.searchParams.get('code') || '').toLowerCase();
  if (!CODE_RE.test(code)) return new Response('Codigo invalido (12 a 40 letras minusculas o numeros)', { status: 400, headers: cors });
  if (request.method !== 'GET' && request.method !== 'PUT') return new Response('Metodo no permitido', { status: 405, headers: cors });
  const res = await env.SAVES.get(env.SAVES.idFromName(code)).fetch(request);
  const out = new Response(res.body, res);
  for (const [k, v] of Object.entries(cors)) out.headers.set(k, v);
  return out;
}

const LANDING = `<!doctype html><html lang="es"><meta charset="utf-8"><meta name="viewport" content="width=device-width,initial-scale=1">
<title>Mario 64 Cooperativo</title>
<style>body{font:16px system-ui;max-width:640px;margin:2rem auto;padding:0 16px;line-height:1.5}code{background:#8882;padding:2px 6px;border-radius:4px}</style>
<h1>Mario 64 Cooperativo</h1>
<p>Servidor en linea del port de PC. Aqui se juega en sala con otros jugadores y se guarda la partida en la nube.</p>
<h2>Como entrar</h2>
<ol><li>Compila el juego en tu PC con tu propia ROM (el juego no se distribuye: contiene material de Nintendo).</li>
<li>En el juego: <b>Pausa → R → Multijugador → Conectar</b>.</li></ol>
<p>Estado del servidor: <code>/health</code> · Protocolo: <code>/info</code></p>
</html>`;

const clampInt = (v, lo, hi, d = 0) => {
  const n = Number.parseInt(v, 10);
  return Number.isFinite(n) ? Math.min(hi, Math.max(lo, n)) : d;
};

export class SaveBox extends DurableObject {
  async fetch(request) {
    if (request.method === 'PUT') {
      const buf = await request.arrayBuffer();
      if (buf.byteLength === 0 || buf.byteLength > SAVE_MAX) return new Response('Tamano invalido', { status: 413 });
      await this.ctx.storage.put({ data: buf, updated: Date.now() });
      return new Response(JSON.stringify({ ok: true, bytes: buf.byteLength }), { headers: JSON_HEADERS });
    }
    const { data, updated } = Object.fromEntries(await this.ctx.storage.get(['data', 'updated']).then((m) => m.entries()));
    if (!data) return new Response('No hay partida con ese codigo', { status: 404 });
    return new Response(data, { headers: { 'content-type': 'application/octet-stream', 'x-updated': String(updated) } });
  }
}

export class Room extends DurableObject {
  constructor(ctx, env) {
    super(ctx, env);
    // "ping" -> "pong" sin despertar al objeto (gratis y mantiene viva la conexion)
    this.ctx.setWebSocketAutoResponse(new WebSocketRequestResponsePair('ping', 'pong'));
    this.buckets = new Map(); // ws -> { tokens, t }  (se pierde al hibernar: no importa)
  }

  async fetch(request) {
    const sockets = this.ctx.getWebSockets();
    if (sockets.length >= MAX_PLAYERS) return new Response('Sala llena', { status: 503 });

    const url = new URL(request.url);
    const used = new Set(sockets.map((s) => s.deserializeAttachment()?.id));
    let id = 1;
    while (used.has(id)) id++;
    const name = (url.searchParams.get('name') || '').replace(NAME_RE, '').trim().slice(0, 16) || `Jugador${id}`;
    const ch = clampInt(url.searchParams.get('ch'), 0, MAX_CHARS - 1);

    const pair = new WebSocketPair();
    const [client, server] = Object.values(pair);
    this.ctx.acceptWebSocket(server);
    server.serializeAttachment({ id, name, ch });

    const players = sockets.map((s) => s.deserializeAttachment()).filter(Boolean);
    server.send(JSON.stringify({ t: 'welcome', id, name, ch, protocol: PROTOCOL, players }));
    this.broadcast({ t: 'join', id, name, ch }, server);
    return new Response(null, { status: 101, webSocket: client });
  }

  broadcast(msg, except) {
    const data = typeof msg === 'string' ? msg : JSON.stringify(msg);
    for (const ws of this.ctx.getWebSockets()) {
      if (ws === except) continue;
      try { ws.send(data); } catch { /* socket cerrado */ }
    }
  }

  allow(ws) {
    const now = Date.now();
    let b = this.buckets.get(ws);
    if (!b) { b = { tokens: BURST, t: now }; this.buckets.set(ws, b); }
    b.tokens = Math.min(BURST, b.tokens + ((now - b.t) / 1000) * RATE_PER_SEC);
    b.t = now;
    if (b.tokens < 1) return false;
    b.tokens -= 1;
    return true;
  }

  webSocketMessage(ws, message) {
    if (typeof message !== 'string' || message.length > MAX_MSG) return;
    if (!this.allow(ws)) return;
    let msg;
    try { msg = JSON.parse(message); } catch { return; }
    if (!msg || typeof msg !== 'object') return;
    const me = ws.deserializeAttachment();
    if (!me) return;

    switch (msg.t) {
      case 's': // estado del jugador: se reenvia tal cual, con el id impuesto por el servidor
      case 'ev': // evento de juego (reservado)
        msg.id = me.id;
        this.broadcast(msg, ws);
        break;
      case 'hi': { // cambio de personaje
        me.ch = clampInt(msg.ch, 0, MAX_CHARS - 1);
        ws.serializeAttachment(me);
        this.broadcast({ t: 'hi', id: me.id, ch: me.ch }, ws);
        break;
      }
      case 'chat':
        this.broadcast({ t: 'chat', id: me.id, text: String(msg.text || '').slice(0, 120) }, ws);
        break;
      default:
        break;
    }
  }

  webSocketClose(ws) {
    this.buckets.delete(ws);
    const me = ws.deserializeAttachment();
    try { ws.close(); } catch { /* ya cerrado */ }
    if (me) this.broadcast({ t: 'leave', id: me.id }, ws);
  }

  webSocketError(ws) { this.webSocketClose(ws); }
}
