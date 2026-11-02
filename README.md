# Mario 64 Cooperativo

Super Mario 64 original (tu ROM) en Windows, con cooperativo en linea propio y servidor en Cloudflare.

- Juego: port de PC `sm64ex` (decompilacion de SM64), en `juego/`. Mecanicas, niveles y fisicas son las originales.
- Multijugador: cliente de red propio dentro del juego (WebSocket, `wss://`). Ves a los demas jugadores con su nombre y personaje.
- Personajes: Mario, Luigi, Wario y Waluigi (modelo de Mario recoloreado, con su emblema en la gorra).
- Servidor: Cloudflare Worker + Durable Objects en `servidor/`, publicado en `https://mario64-coop.mayacodegt.workers.dev`.
- Controles estilo Project64.

## Instalar (rapido)
1. Descarga el repositorio (boton verde **Code > Download ZIP**) y descomprimelo.
2. Copia tu ROM `Super Mario 64 (USA).z64` dentro de la carpeta.
3. Ejecuta **`INSTALAR.bat`**: instala el compilador, compila el juego con tu ROM y crea el acceso directo "Mario 64 Cooperativo" en el escritorio.

El .exe no se publica ya hecho porque contiene material de Nintendo extraido de la ROM; se genera en tu PC.

## Como jugar (paso a paso)
1. Pon tu ROM `Super Mario 64 (USA).z64` en esta carpeta (SHA1 `9bef1128717f958171a4afac3ed78ee2bb4e86ce`).
2. (Una vez) `INSTALAR_ENTORNO.bat` instala el compilador (MSYS2).
3. `COMPILAR.bat` compila el juego con tu ROM.
4. `JUGAR.bat` te pregunta nombre, personaje y sala y abre el juego. Los amigos que usen el mismo nombre de sala se ven.
   `JUGAR_RAPIDO.bat` abre con lo ultimo que elegiste.

Importante: el .exe contiene los graficos y sonidos de Nintendo extraidos de la ROM. No lo publiques ni lo subas a internet;
cada jugador debe compilarlo con su propia ROM (por eso el repositorio ignora la ROM y lo que sale de ella).

## Controles (como Project64)
| N64 | Teclado |
|---|---|
| Palanca | Flechas |
| A / B / Z | X / C / Z |
| Start | Enter |
| Chat (solo en linea) | T abre, Enter envia, Esc cancela |
| L / R | A / S |
| Botones C (arriba, abajo, izq., der.) | I, K, J, L |

Se pueden cambiar en el menu de opciones del juego o en `config/sm64config.txt`. Mandos compatibles con SDL2.

## Configuracion de red (`juego/build/us_pc/online.txt`)
```
url=wss://mario64-coop.mayacodegt.workers.dev   (vacio = sin conexion)
room=publica
name=Jugador
char=0        (0 Mario, 1 Luigi, 2 Wario, 3 Waluigi)
```
Diagnostico: `juego/build/us_pc/net_status.txt` (conectado, tu id, jugadores remotos, distancia al mas cercano).

## Servidor (`servidor/`)
- `SERVIDOR.bat`: servidor local (`ws://localhost:8787`). Para probarlo: `cd servidor && npm test` (o `node test/cliente.mjs wss://TU-WORKER`).
- Publicar: `cd servidor && npx wrangler deploy` (requiere `wrangler login`).
- Clave opcional de sala: `npx wrangler secret put ROOM_KEY` y anade `&key=...` a la URL.
- El servidor no simula el juego: valida, impone el id de cada jugador, limita el ritmo (40 msg/s) y retransmite.

## Estado
- **Graficos**: texturas con filtro bicubico (Catmull-Rom), mipmaps + anisotropico 16x y antialiasing MSAA 4x. Se cambia en Opciones > Filtro de textura (Cercano / Lineal / Tres puntos / Bicubico) o con `texture_filtering` en sm64config.txt.
Hecho:
- Ver a los demas jugadores (posicion, animacion, personaje, nombre) en el mismo nivel y area.
- Estrellas compartidas: cuando alguien consigue una estrella, a los demas se les marca en su partida, suena el aviso y aparece "NOMBRE +1 ESTRELLA". Ademas la estrella del mundo desaparece para los demas (ver mas abajo).

- Juego traducido al espanol: 170 dialogos, nombres de niveles y estrellas, menus de pausa, seleccion de partida, opciones y final. Se escribe sin tildes ni enie porque la tipografia del juego no tiene esos caracteres. Los originales en ingles estan en `texto_original/` y el texto traducido en `juego/text/us/` y `juego/include/text_*strings.h.in`. Las imagenes con texto (logo, "GAME OVER", creditos) siguen en ingles.
- Guia de misiones en la pantalla de inicio (la de la cabeza de Mario): una pagina por nivel que rota cada pocos segundos, con las 6 estrellas de cada uno; ademas niveles secretos, interruptores de gorra y consejos del cooperativo. Esta en `juego/src/game/guide.c`.
- Chat dentro del juego (tecla T): los mensajes salen abajo a la izquierda durante unos segundos. Las tildes y la n con tilde se escriben sin ellas porque la tipografia del juego no las tiene.
- Monedas, estrellas y enemigos compartidos: lo que recoge o derrota un jugador desaparece tambien en los demas (se identifica por tipo de objeto y posicion de origen, en el mismo nivel y area). Las monedas cuentan solo para quien las recoge.
- Contacto entre jugadores: no se atraviesan (se empujan) y se puede rebotar pisando la cabeza de otro.

Limites conocidos: los enemigos no se mueven igual en todos los clientes (cada uno simula los suyos); solo se comparte su derrota. `net_status.txt` muestra objetos_enviados/recibidos/aplicados para diagnosticar.

Pendiente: movimiento sincronizado de enemigos y plataformas moviles, ataques entre jugadores, jefes.

## Estructura
- `juego/` codigo del juego (cliente de red: `src/pc/net/` y `src/game/remote_players.c`; personajes: `tools/gen_characters.py`).
- `servidor/` Cloudflare Worker. `config/` configuracion por defecto. `scripts/` lanzador.
- `UPSTREAM_COMMIT.txt` version de sm64ex de la que partimos.
