// Prueba del servidor de salas. Uso: node test/cliente.mjs [ws://host:puerto]
const base = process.argv[2] || 'ws://localhost:8787';
const room = 'prueba' + Math.floor(Math.random() * 1e6);

const open = (name, ch) => new Promise((res, rej) => {
  const ws = new WebSocket(`${base}/ws?room=${room}&name=${encodeURIComponent(name)}&ch=${ch}`);
  const inbox = [];
  ws.onmessage = (e) => inbox.push(e.data === 'pong' ? { t: 'pong' } : JSON.parse(e.data));
  ws.onopen = () => res({ ws, inbox });
  ws.onerror = () => rej(new Error('no conecta'));
});
const wait = (ms) => new Promise((r) => setTimeout(r, ms));
const fails = [];
const check = (cond, msg) => { console.log(cond ? '  ok  ' : ' FALLO', msg); if (!cond) fails.push(msg); };

const A = await open('Mario', 0); await wait(250);
const B = await open('Luigi<script>', 1); await wait(250);

check(A.inbox[0]?.t === 'welcome' && A.inbox[0].id === 1, 'A recibe welcome con id 1');
check(B.inbox[0]?.id === 2 && B.inbox[0].name === 'Luigiscript' && B.inbox[0].ch === 1, 'B: id 2, nombre saneado, personaje 1');
check(B.inbox[0]?.players?.length === 1 && B.inbox[0].players[0].name === 'Mario', 'B ve a Mario en la lista inicial');
check(A.inbox.some((m) => m.t === 'join' && m.id === 2 && m.ch === 1), 'A recibe join de B');

A.ws.send(JSON.stringify({ t: 's', id: 99, p: [1, 2, 3], g: [0, 0, 0], a: 5 })); // intenta suplantar id 99
await wait(250);
check(B.inbox.some((m) => m.t === 's' && m.id === 1 && m.p[0] === 1), 'B recibe estado de A con id impuesto (1, no 99)');

A.ws.send(JSON.stringify({ t: 'hi', ch: 3 })); await wait(250);
check(B.inbox.some((m) => m.t === 'hi' && m.id === 1 && m.ch === 3), 'cambio de personaje se retransmite');

A.ws.send('x'.repeat(5000)); A.ws.send('no es json'); A.ws.send(JSON.stringify({ t: 'mala' })); await wait(250);
check(!B.inbox.some((m) => m.t === 'mala'), 'mensajes invalidos/desconocidos se descartan');

A.ws.send('ping'); await wait(200);
check(A.inbox.some((m) => m.t === 'pong'), 'ping -> pong');

for (let i = 0; i < 300; i++) A.ws.send(JSON.stringify({ t: 's', p: [i, 0, 0], g: [0, 0, 0] }));
await wait(500);
const got = B.inbox.filter((m) => m.t === 's').length;
check(got < 200, `limite de ritmo activo (de 300 mensajes llegaron ${got})`);

A.ws.close(); await wait(300);
check(B.inbox.at(-1)?.t === 'leave' && B.inbox.at(-1).id === 1, 'B recibe leave al salir A');
B.ws.close();

console.log(fails.length ? `\n${fails.length} FALLOS` : '\nTODAS LAS PRUEBAS OK');
process.exit(fails.length ? 1 : 0);
