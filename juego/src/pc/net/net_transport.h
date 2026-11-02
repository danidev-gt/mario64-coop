#ifndef NET_TRANSPORT_H
#define NET_TRANSPORT_H

// Capa de transporte WebSocket (libcurl) del cooperativo. No incluye cabeceras del juego.

#define NET_MAX_PLAYERS 16

typedef struct {
    float p[3];     // posicion
    int g[3];       // angulos (pitch, yaw, roll)
    int anim;       // id de animacion de Mario
    int frame;      // fotograma de la animacion
    int accel;      // aceleracion de la animacion
    int level;      // nivel (gCurrLevelNum)
    int area;       // area dentro del nivel
} NetState;

typedef struct {
    int used;
    int id;
    char name[20];
    int ch;         // personaje (0 Mario, 1 Luigi, 2 Wario, 3 Waluigi)
    int hasState;
    NetState st;
} NetRemote;

// Lee online.txt (url=, room=, name=). Si no existe o la url esta vacia, queda desactivado.
void nt_start(void);
int  nt_enabled(void);
int  nt_connected(void);
int  nt_my_id(void);
int  nt_my_char(void);
const char *nt_my_name(void);
void nt_set_local(const NetState *s);
// Copia atomica de la tabla de jugadores remotos (indexada por id-1)
void nt_snapshot(NetRemote out[NET_MAX_PLAYERS]);

// Control en caliente desde el menu
void nt_set_online(int on);          // conectar / desconectar
int  nt_state(void);                 // 0 desconectado, 1 conectando, 2 conectado
int  nt_ping_ms(void);               // latencia en ms (-1 si aun no se mide)
const char *nt_room(void);
void nt_set_room(const char *room);  // reconecta a esa sala
void nt_set_char(int ch);            // cambia de personaje y avisa a los demas
void nt_save_config(void);           // reescribe online.txt

// Eventos de juego (estrellas compartidas, etc.). kind: texto corto ("star"), a y b: parametros.
void nt_send_event(const char *kind, const int v[5]);
// Saca un evento recibido de otro jugador. Devuelve 0 si no hay ninguno.
int  nt_poll_event(int *from, char kind[8], int v[5]);

// Chat: texto ASCII (max 100 caracteres).
void nt_send_chat(const char *text);
int  nt_poll_chat(int *from, char text[104]);

#endif
