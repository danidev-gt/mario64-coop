#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <curl/curl.h>
#include <SDL2/SDL.h>

#include "net_transport.h"

static SDL_mutex *sLock;
static SDL_Thread *sThread;
static volatile int sEnabled, sConnected, sMyId;
static char sUrl[256], sRoom[40] = "publica", sName[20] = "Mario";
static NetState sLocal;
static int sLocalValid;
static NetRemote sRemotes[NET_MAX_PLAYERS];
static int sChar;
typedef struct { int from, v[5]; char kind[8]; } NetEvent;
typedef struct { int from; char text[104]; } NetChat;
#define CHAT_CAP 8
static NetChat sChatIn[CHAT_CAP], sChatOut[CHAT_CAP];
static int sChatInHead, sChatInTail, sChatOutHead, sChatOutTail;
#define EV_CAP 16
static NetEvent sEvIn[EV_CAP], sEvOut[EV_CAP];
static int sEvInHead, sEvInTail, sEvOutHead, sEvOutTail;
static int sLoopback; // prueba: el juego se ve a si mismo como un jugador remoto
#define DEFAULT_URL "wss://mario64-coop.mayacodegt.workers.dev"
static volatile int sRun, sWantOn, sReconnect, sConnecting, sPingMs = -1, sSendHi;
static Uint32 sPingSent;

// ---------- JSON minimo (los mensajes los emite nuestro servidor, con formato conocido) ----------
static const char *find_key(const char *s, const char *key) {
    char pat[24];
    snprintf(pat, sizeof(pat), "\"%s\":", key);
    const char *r = strstr(s, pat);
    return r ? r + strlen(pat) : NULL;
}

static int get_num(const char *s, const char *key, double *out) {
    const char *p = find_key(s, key);
    if (!p) return 0;
    char *end;
    *out = strtod(p, &end);
    return end != p;
}

static int get_vec3(const char *s, const char *key, double v[3]) {
    const char *p = find_key(s, key);
    if (!p || *p != '[') return 0;
    char *end;
    p++;
    for (int i = 0; i < 3; i++) {
        v[i] = strtod(p, &end);
        if (end == p) return 0;
        p = end;
        if (*p == ',') p++;
    }
    return 1;
}

static void get_str(const char *s, const char *key, char *out, size_t cap) {
    const char *p = find_key(s, key);
    out[0] = 0;
    if (!p || *p != '"') return;
    p++;
    size_t n = 0;
    while (*p && *p != '"' && n + 1 < cap) out[n++] = *p++;
    out[n] = 0;
}

static NetRemote *slot_for(int id) {
    return (id >= 1 && id <= NET_MAX_PLAYERS) ? &sRemotes[id - 1] : NULL;
}

static void handle_message(const char *msg) {
    char type[16];
    double id = 0, v;
    if (!strcmp(msg, "pong")) { sPingMs = (int) (SDL_GetTicks() - sPingSent); return; }
    get_str(msg, "t", type, sizeof(type));

    SDL_LockMutex(sLock);
    if (!strcmp(type, "welcome")) {
        memset(sRemotes, 0, sizeof(sRemotes));
        if (get_num(msg, "id", &id)) sMyId = (int) id;
        const char *pl = strstr(msg, "\"players\":[");
        while (pl && (pl = strstr(pl, "{\"id\":"))) {
            double pid = 0;
            get_num(pl, "id", &pid);
            NetRemote *r = slot_for((int) pid);
            if (r) {
                double pch = 0;
                r->used = 1; r->id = (int) pid; get_str(pl, "name", r->name, sizeof(r->name));
                if (get_num(pl, "ch", &pch)) r->ch = (int) pch;
            }
            pl += 6;
        }
    } else if (!strcmp(type, "join")) {
        if (get_num(msg, "id", &id)) {
            NetRemote *r = slot_for((int) id);
            if (r) {
                double jch = 0;
                memset(r, 0, sizeof(*r)); r->used = 1; r->id = (int) id; get_str(msg, "name", r->name, sizeof(r->name));
                if (get_num(msg, "ch", &jch)) r->ch = (int) jch;
            }
        }
    } else if (!strcmp(type, "hi")) {
        if (get_num(msg, "id", &id) && get_num(msg, "ch", &v)) { NetRemote *r = slot_for((int) id); if (r) r->ch = (int) v; }
    } else if (!strcmp(type, "leave")) {
        if (get_num(msg, "id", &id)) { NetRemote *r = slot_for((int) id); if (r) memset(r, 0, sizeof(*r)); }
    } else if (!strcmp(type, "ev")) {
        static const char *keys[5] = { "a", "b", "c", "d", "e" };
        NetEvent e;
        memset(&e, 0, sizeof(e));
        get_str(msg, "k", e.kind, sizeof(e.kind));
        for (int i = 0; i < 5; i++) { double ev = 0; get_num(msg, keys[i], &ev); e.v[i] = (int) ev; }
        if (get_num(msg, "id", &id) && e.kind[0] && (sEvInHead + 1) % EV_CAP != sEvInTail) {
            e.from = (int) id;
            sEvIn[sEvInHead] = e;
            sEvInHead = (sEvInHead + 1) % EV_CAP;
        }
    } else if (!strcmp(type, "chat")) {
        NetChat ch;
        memset(&ch, 0, sizeof(ch));
        get_str(msg, "text", ch.text, sizeof(ch.text));
        if (get_num(msg, "id", &id) && ch.text[0] && (sChatInHead + 1) % CHAT_CAP != sChatInTail) {
            ch.from = (int) id;
            sChatIn[sChatInHead] = ch;
            sChatInHead = (sChatInHead + 1) % CHAT_CAP;
        }
    } else if (!strcmp(type, "s")) {
        if (get_num(msg, "id", &id)) {
            NetRemote *r = slot_for((int) id);
            double p[3], g[3];
            if (r && get_vec3(msg, "p", p) && get_vec3(msg, "g", g)) {
                r->used = 1; r->id = (int) id;
                for (int i = 0; i < 3; i++) { r->st.p[i] = (float) p[i]; r->st.g[i] = (int) g[i]; }
                if (get_num(msg, "a", &v)) r->st.anim = (int) v;
                if (get_num(msg, "f", &v)) r->st.frame = (int) v;
                if (get_num(msg, "c", &v)) r->st.accel = (int) v;
                if (get_num(msg, "l", &v)) r->st.level = (int) v;
                if (get_num(msg, "r", &v)) r->st.area = (int) v;
                r->hasState = 1;
            }
        }
    }
    SDL_UnlockMutex(sLock);
}

// ---------- hilo de red ----------
static int net_thread(void *unused) {
    (void) unused;
    while (sRun) {
        if (!sWantOn) { SDL_Delay(150); continue; }
        sReconnect = 0;
        char *en = curl_easy_escape(NULL, sName, 0), *er = curl_easy_escape(NULL, sRoom, 0);
        char full[512];
        snprintf(full, sizeof(full), "%s/ws?room=%s&name=%s&ch=%d", sUrl[0] ? sUrl : DEFAULT_URL, er, en, sChar);
        curl_free(en); curl_free(er);

        sConnecting = 1;
        CURL *c = curl_easy_init();
        curl_easy_setopt(c, CURLOPT_URL, full);
        curl_easy_setopt(c, CURLOPT_CONNECT_ONLY, 2L);
        curl_easy_setopt(c, CURLOPT_CONNECTTIMEOUT, 8L);
        CURLcode rc = curl_easy_perform(c);
        sConnecting = 0;
        if (rc != CURLE_OK) {
            printf("net: no se pudo conectar a %s (%s)\n", full, curl_easy_strerror(rc));
            curl_easy_cleanup(c);
            for (int i = 0; i < 20 && sWantOn && !sReconnect; i++) SDL_Delay(150);
            continue;
        }
        printf("net: conectado a %s\n", full);
        sConnected = 1;
        sPingMs = -1;

        char buf[2048];
        size_t fill = 0;
        Uint32 lastSend = 0, lastPing = 0;
        int alive = 1;
        while (alive && sWantOn && !sReconnect) {
            size_t got = 0;
            const struct curl_ws_frame *meta = NULL;
            CURLcode r = curl_ws_recv(c, buf + fill, sizeof(buf) - 1 - fill, &got, &meta);
            if (r == CURLE_OK && meta) {
                if (meta->flags & CURLWS_CLOSE) { alive = 0; break; }
                fill += got;
                if ((meta->flags & CURLWS_TEXT) && meta->bytesleft == 0 && !(meta->flags & CURLWS_CONT)) {
                    buf[fill] = 0;
                    handle_message(buf);
                    fill = 0;
                } else if (fill >= sizeof(buf) - 1) {
                    fill = 0; // mensaje demasiado grande: descartar
                }
                continue; // seguir leyendo mientras haya datos
            } else if (r != CURLE_AGAIN) {
                alive = 0; break;
            }

            for (;;) { // eventos pendientes de enviar
                NetEvent e; int have = 0;
                SDL_LockMutex(sLock);
                if (sEvOutTail != sEvOutHead) { e = sEvOut[sEvOutTail]; sEvOutTail = (sEvOutTail + 1) % EV_CAP; have = 1; }
                SDL_UnlockMutex(sLock);
                if (!have) break;
                char eo[192];
                int en2 = snprintf(eo, sizeof(eo), "{\"t\":\"ev\",\"k\":\"%s\",\"a\":%d,\"b\":%d,\"c\":%d,\"d\":%d,\"e\":%d}",
                                   e.kind, e.v[0], e.v[1], e.v[2], e.v[3], e.v[4]);
                size_t esent = 0;
                curl_ws_send(c, eo, (size_t) en2, &esent, 0, CURLWS_TEXT);
            }
            for (;;) { // mensajes de chat pendientes
                NetChat ch; int have = 0;
                SDL_LockMutex(sLock);
                if (sChatOutTail != sChatOutHead) { ch = sChatOut[sChatOutTail]; sChatOutTail = (sChatOutTail + 1) % CHAT_CAP; have = 1; }
                SDL_UnlockMutex(sLock);
                if (!have) break;
                char co[192];
                int cn = snprintf(co, sizeof(co), "{\"t\":\"chat\",\"text\":\"%s\"}", ch.text);
                size_t csent = 0;
                curl_ws_send(c, co, (size_t) cn, &csent, 0, CURLWS_TEXT);
            }

            Uint32 now = SDL_GetTicks();
            if (sSendHi) { // cambio de personaje en caliente
                char hi[48];
                int hn = snprintf(hi, sizeof(hi), "{\"t\":\"hi\",\"ch\":%d}", sChar);
                size_t hsent = 0;
                sSendHi = 0;
                curl_ws_send(c, hi, (size_t) hn, &hsent, 0, CURLWS_TEXT);
            }
            if (now - lastPing >= 2000) { // medicion de latencia: "pong" lo contesta el servidor
                size_t psent = 0;
                lastPing = now;
                sPingSent = now;
                curl_ws_send(c, "ping", 4, &psent, 0, CURLWS_TEXT);
            }
            if (now - lastSend >= 66) { // ~15 Hz
                lastSend = now;
                NetState s; int valid;
                SDL_LockMutex(sLock); s = sLocal; valid = sLocalValid; SDL_UnlockMutex(sLock);
                if (valid) {
                    char out[256];
                    int n = snprintf(out, sizeof(out),
                        "{\"t\":\"s\",\"p\":[%.1f,%.1f,%.1f],\"g\":[%d,%d,%d],\"a\":%d,\"f\":%d,\"c\":%d,\"l\":%d,\"r\":%d}",
                        s.p[0], s.p[1], s.p[2], s.g[0], s.g[1], s.g[2], s.anim, s.frame, s.accel, s.level, s.area);
                    size_t sent = 0;
                    CURLcode sr = curl_ws_send(c, out, (size_t) n, &sent, 0, CURLWS_TEXT);
                    if (sr != CURLE_OK && sr != CURLE_AGAIN) alive = 0;
                }
            }
            SDL_Delay(4);
        }
        sConnected = 0;
        sMyId = 0;
        SDL_LockMutex(sLock); memset(sRemotes, 0, sizeof(sRemotes)); SDL_UnlockMutex(sLock);
        curl_easy_cleanup(c);
        printf("net: desconectado\n");
        for (int i = 0; i < 13 && sWantOn && !sReconnect; i++) SDL_Delay(150);
    }
    return 0;
}

static void trim(char *s) {
    size_t n = strlen(s);
    while (n && (s[n - 1] == '\n' || s[n - 1] == '\r' || s[n - 1] == ' ')) s[--n] = 0;
}

void nt_start(void) {
    FILE *f = fopen("online.txt", "r");
    char line[300];
    while (f && fgets(line, sizeof(line), f)) {
        trim(line);
        if (!strncmp(line, "url=", 4)) snprintf(sUrl, sizeof(sUrl), "%s", line + 4);
        else if (!strncmp(line, "room=", 5)) snprintf(sRoom, sizeof(sRoom), "%s", line + 5);
        else if (!strncmp(line, "name=", 5)) snprintf(sName, sizeof(sName), "%s", line + 5);
        else if (!strncmp(line, "char=", 5)) sChar = atoi(line + 5);
        else if (!strncmp(line, "loopback=1", 10)) sLoopback = 1;
    }
    if (f) fclose(f);
    if (sLoopback) { sLock = SDL_CreateMutex(); sEnabled = 1; printf("net: modo loopback\n"); return; }

    // El hilo de red siempre existe para poder conectar desde el menu; solo se conecta solo si hay url=
    curl_global_init(CURL_GLOBAL_DEFAULT);
    sLock = SDL_CreateMutex();
    sEnabled = 1;
    sRun = 1;
    sWantOn = sUrl[0] != 0;
    sThread = SDL_CreateThread(net_thread, "net", NULL);
    printf("net: sala '%s' como '%s' (%s)\n", sRoom, sName, sWantOn ? sUrl : "desconectado hasta usar el menu");
}

// ---------- control desde el menu ----------
void nt_set_online(int on) {
    if (sLoopback || !sEnabled) return;
    if (on && !sUrl[0]) snprintf(sUrl, sizeof(sUrl), "%s", DEFAULT_URL);
    sWantOn = on ? 1 : 0;
    nt_save_config();
}

int nt_state(void) { return sConnected ? 2 : ((sWantOn || sConnecting) ? 1 : 0); }
int nt_ping_ms(void) { return sConnected ? sPingMs : -1; }
const char *nt_room(void) { return sRoom; }

void nt_set_room(const char *room) {
    if (!strcmp(room, sRoom)) return;
    snprintf(sRoom, sizeof(sRoom), "%s", room);
    sReconnect = 1; // la sala se elige al conectar
    nt_save_config();
}

void nt_set_char(int ch) {
    if (ch < 0 || ch > 7) return;
    sChar = ch;
    sSendHi = 1;
    nt_save_config();
}

void nt_save_config(void) {
    FILE *f = fopen("online.txt", "w");
    if (!f) return;
    fprintf(f, "url=%s\nroom=%s\nname=%s\nchar=%d\n", sWantOn ? (sUrl[0] ? sUrl : DEFAULT_URL) : "", sRoom, sName, sChar);
    if (!sWantOn && sUrl[0]) fprintf(f, "#url_guardada=%s\n", sUrl);
    fclose(f);
}

int nt_enabled(void) { return sEnabled; }
int nt_connected(void) { return sConnected; }
int nt_my_id(void) { return sMyId; }
int nt_my_char(void) { return sChar; }
const char *nt_my_name(void) { return sName; }

void nt_set_local(const NetState *s) {
    if (!sEnabled) return;
    SDL_LockMutex(sLock);
    sLocal = *s; sLocalValid = 1;
    SDL_UnlockMutex(sLock);
}

void nt_snapshot(NetRemote out[NET_MAX_PLAYERS]) {
    if (!sEnabled) { memset(out, 0, sizeof(NetRemote) * NET_MAX_PLAYERS); return; }
    SDL_LockMutex(sLock);
    memcpy(out, sRemotes, sizeof(sRemotes));
    if (sLoopback && sLocalValid) {
        NetRemote *r = &out[NET_MAX_PLAYERS - 1];
        r->used = 1; r->id = NET_MAX_PLAYERS; r->hasState = 1;
        r->st = sLocal; r->st.p[0] += 150.0f;
        snprintf(r->name, sizeof(r->name), "%s", "Remoto"); r->ch = (sChar + 1) % 4;
    }
    SDL_UnlockMutex(sLock);
}

void nt_send_event(const char *kind, const int v[5]) {
    if (!sEnabled || sLoopback) return;
    SDL_LockMutex(sLock);
    if ((sEvOutHead + 1) % EV_CAP != sEvOutTail) {
        NetEvent *e = &sEvOut[sEvOutHead];
        memset(e, 0, sizeof(*e));
        snprintf(e->kind, sizeof(e->kind), "%s", kind);
        memcpy(e->v, v, sizeof(e->v));
        sEvOutHead = (sEvOutHead + 1) % EV_CAP;
    }
    SDL_UnlockMutex(sLock);
}

int nt_poll_event(int *from, char kind[8], int v[5]) {
    int have = 0;
    if (!sEnabled) return 0;
    SDL_LockMutex(sLock);
    if (sEvInTail != sEvInHead) {
        NetEvent *e = &sEvIn[sEvInTail];
        *from = e->from;
        memcpy(v, e->v, sizeof(e->v));
        memcpy(kind, e->kind, 8);
        sEvInTail = (sEvInTail + 1) % EV_CAP;
        have = 1;
    }
    SDL_UnlockMutex(sLock);
    return have;
}

void nt_send_chat(const char *text) {
    if (!sEnabled || sLoopback) return;
    SDL_LockMutex(sLock);
    if ((sChatOutHead + 1) % CHAT_CAP != sChatOutTail) {
        NetChat *c = &sChatOut[sChatOutHead];
        memset(c, 0, sizeof(*c));
        snprintf(c->text, sizeof(c->text), "%s", text);
        sChatOutHead = (sChatOutHead + 1) % CHAT_CAP;
    }
    SDL_UnlockMutex(sLock);
}

int nt_poll_chat(int *from, char text[104]) {
    int have = 0;
    if (!sEnabled) return 0;
    SDL_LockMutex(sLock);
    if (sChatInTail != sChatInHead) {
        *from = sChatIn[sChatInTail].from;
        memcpy(text, sChatIn[sChatInTail].text, 104);
        sChatInTail = (sChatInTail + 1) % CHAT_CAP;
        have = 1;
    }
    SDL_UnlockMutex(sLock);
    return have;
}
