#include <stdio.h>
#include <math.h>
#include <string.h>
#include <ctype.h>
#include <stdint.h>
#include <PR/ultratypes.h>

#include "sm64.h"
#include "area.h"
#include "behavior_data.h"
#include "camera.h"
#include "engine/behavior_script.h"
#include "engine/graph_node.h"
#include "engine/math_util.h"
#include "game_init.h"
#include "level_update.h"
#include "memory.h"
#include "model_ids.h"
#include "object_helpers.h"
#include "object_list_processor.h"
#include "print.h"
#include "audio/external.h"
#include "save_file.h"
#include "chat.h"
#include "interaction.h"
#include "engine/surface_collision.h"
#include "mario.h"
#include "course_table.h"
#include "remote_players.h"

#include "pc/net/net_transport.h"
#include "pc/gfx/gfx_pc.h"

extern struct CameraFOVStatus sFOVState; // camera.c

// Un buffer de animacion propio por jugador remoto (el de Mario es uno solo y se pisaria)
static struct MarioAnimation sAnim[NET_MAX_PLAYERS];
static u8 sAnimBuf[NET_MAX_PLAYERS][0x4000] __attribute__((aligned(16)));
static struct Object *sObj[NET_MAX_PLAYERS];
static int sObjChar[NET_MAX_PLAYERS];
static NetRemote sSnap[NET_MAX_PLAYERS];
static int sObjSent, sObjRecv, sObjApplied; // contadores para net_status.txt
static s32 obj_is_remote(struct Object *obj, int slot);

// Personajes: 0 Mario, 1 Luigi, 2 Wario, 3 Waluigi (modelos generados con tools/gen_characters.py)
static s32 model_for_char(int ch) {
    switch (ch) {
        case 1: return MODEL_LUIGI;
        case 2: return MODEL_WARIO;
        case 3: return MODEL_WALUIGI;
        default: return MODEL_MARIO;
    }
}

// ---------- estrellas compartidas ----------
// Cuando un jugador consigue una estrella, el resto la recibe en su partida (la estrella del mundo
// sigue ahi: el progreso es compartido, no el objeto). Un aviso en pantalla dice quien fue.
static char sToast[40];
static int sToastTimer;

void remote_players_on_star(int courseIndex, int starIndex) {
    if (courseIndex < 0 || courseIndex >= COURSE_STAGES_COUNT || starIndex < 0 || starIndex > 6) return;
    int v[5] = { courseIndex, starIndex, 0, 0, 0 };
    nt_send_event("star", v);
}

static void apply_remote_star(int from, int course, int idx) {
    if (course < 0 || course >= COURSE_STAGES_COUNT || idx < 0 || idx > 6) return;
    s32 file = gCurrSaveFileNum - 1;
    if (file < 0) return;
    u32 flags = save_file_get_star_flags(file, course);
    if (flags & (1u << idx)) return; // ya la tenia
    save_file_set_star_flags(file, course, 1u << idx);
    if (gMarioState != NULL)
        gMarioState->numStars = save_file_get_total_star_count(file, COURSE_MIN - 1, COURSE_MAX - 1);
    play_sound(SOUND_MENU_STAR_SOUND, gDefaultSoundArgs);

    const char *who = (from >= 1 && from <= NET_MAX_PLAYERS && sSnap[from - 1].name[0]) ? sSnap[from - 1].name : "ALGUIEN";
    char nm[16];
    snprintf(nm, sizeof(nm), "%.12s", who);
    for (char *c = nm; *c; c++) *c = (char) toupper((unsigned char) *c);
    snprintf(sToast, sizeof(sToast), "%s +1 ESTRELLA", nm);
    sToastTimer = 150;
}

// ---------- objetos compartidos (monedas, estrellas, enemigos) ----------
// Los objetos de cada nivel nacen igual en todos los clientes, asi que se identifican por
// tipo de comportamiento + posicion de origen (la "casa" del objeto, o su posicion si no tiene).
static void obj_key(struct Object *ob, int out[3]) {
    if (ob->oHomeX != 0.0f || ob->oHomeZ != 0.0f) {
        out[0] = (int) ob->oHomeX; out[1] = (int) ob->oHomeY; out[2] = (int) ob->oHomeZ;
    } else {
        out[0] = (int) ob->oPosX; out[1] = (int) ob->oPosY; out[2] = (int) ob->oPosZ;
    }
}

static int bhv_id(struct Object *ob) {
    return (int) ((intptr_t) ob->behavior - (intptr_t) segmented_to_virtual(bhvYellowCoin));
}

void remote_players_on_object_gone(struct Object *ob) {
    if (!nt_enabled() || ob == NULL || ob->behavior == NULL) return;
    int k[3];
    obj_key(ob, k);
    int v[5] = { k[0], k[1], k[2], bhv_id(ob), gCurrLevelNum * 16 + gCurrAreaIndex };
    nt_send_event("obj", v);
    sObjSent++;
}

static void apply_remote_obj(const int v[5]) {
    sObjRecv++;
    if (v[4] != gCurrLevelNum * 16 + gCurrAreaIndex) return;
    struct Object *best = NULL;
    f32 bestD = 150.0f * 150.0f;
    for (int i = 0; i < NUM_OBJ_LISTS; i++) {
        struct ObjectNode *head = &gObjectLists[i];
        for (struct ObjectNode *n = head->next; n != head; n = n->next) {
            struct Object *ob = (struct Object *) n;
            if (ob->activeFlags == ACTIVE_FLAG_DEACTIVATED || ob == gMarioObject || ob->behavior == NULL) continue;
            if (bhv_id(ob) != v[3]) continue;
            int k[3];
            obj_key(ob, k);
            f32 dx = (f32) (k[0] - v[0]), dy = (f32) (k[1] - v[1]), dz = (f32) (k[2] - v[2]);
            f32 d = dx * dx + dy * dy + dz * dz;
            if (d < bestD) { bestD = d; best = ob; }
        }
    }
    if (best == NULL) return;
    if (best->oInteractType & INTERACT_STAR_OR_KEY) {
        spawn_object(best, MODEL_NONE, bhvStarKeyCollectionPuffSpawner);
    } else if (!(best->oInteractType & INTERACT_COIN)) {
        struct Object *saved = gCurrentObject;
        gCurrentObject = best;
        spawn_mist_particles();
        gCurrentObject = saved;
    }
    obj_mark_for_deletion(best);
    sObjApplied++;
}

static void poll_events(void) {
    int from, v[5];
    char kind[8];
    while (nt_poll_event(&from, kind, v)) {
        kind[7] = 0;
        if (!strcmp(kind, "star")) apply_remote_star(from, v[0], v[1]);
        else if (!strcmp(kind, "obj")) apply_remote_obj(v);
    }
}

static void poll_chat(void) {
    int from;
    char text[104];
    while (nt_poll_chat(&from, text)) {
        text[103] = 0;
        const char *who = (from >= 1 && from <= NET_MAX_PLAYERS && sSnap[from - 1].name[0]) ? sSnap[from - 1].name : "?";
        chat_add(who, text);
    }
}

// ---------- contacto entre jugadores ----------
// Los jugadores remotos son solo visuales, asi que el contacto lo resuelve cada cliente con el suyo:
// no se atraviesan (se empujan) y se puede rebotar pisando la cabeza de otro, como con un enemigo.
static void player_contact(void) {
    struct MarioState *m = gMarioState;
    if (m == NULL || gMarioObject == NULL) return;
    for (int i = 0; i < NET_MAX_PLAYERS; i++) {
        struct Object *ob = sObj[i];
        if (!obj_is_remote(ob, i)) continue;
        f32 dx = m->pos[0] - ob->oPosX, dz = m->pos[2] - ob->oPosZ, dy = m->pos[1] - ob->oPosY;
        f32 d2 = dx * dx + dz * dz;
        if ((m->action & ACT_FLAG_AIR) && m->vel[1] < 0.0f && dy > 90.0f && dy < 170.0f && d2 < 45.0f * 45.0f) {
            m->vel[1] = 42.0f;
            play_sound(SOUND_ACTION_BOUNCE_OFF_OBJECT, m->marioObj->header.gfx.cameraToObject);
            return;
        }
        if (!(m->action & ACT_FLAG_AIR) && fabsf(dy) < 100.0f && d2 < 55.0f * 55.0f) {
            f32 d = sqrtf(d2);
            if (d < 1.0f) { dx = 1.0f; dz = 0.0f; d = 1.0f; }
            f32 push = (55.0f - d) * 0.5f;
            f32 nx = m->pos[0] + dx / d * push, nz = m->pos[2] + dz / d * push;
            struct Surface *fl;
            f32 fh = find_floor(nx, m->pos[1] + 80.0f, nz, &fl);
            if (fl != NULL && fabsf(fh - m->pos[1]) < 60.0f) { m->pos[0] = nx; m->pos[2] = nz; }
        }
    }
}

void remote_players_init(void) {
    nt_start();
}

static void send_local_state(void) {
    if (gMarioObject == NULL || gMarioState == NULL) return;
    struct GraphNodeObject *g = &gMarioObject->header.gfx;
    NetState s;
    s.p[0] = g->pos[0]; s.p[1] = g->pos[1]; s.p[2] = g->pos[2];
    s.g[0] = g->angle[0]; s.g[1] = g->angle[1]; s.g[2] = g->angle[2];
    s.anim = g->unk38.animID;
    s.frame = g->unk38.animFrame;
    s.accel = g->unk38.animAccel;
    s.level = gCurrLevelNum;
    s.area = gCurrAreaIndex;
    nt_set_local(&s);
}

// net_status.txt: estado de la red, util para diagnosticar (se reescribe ~2 veces por segundo)
static void write_status(void) {
    static int tick;
    if (++tick % 15 != 0) return;
    FILE *f = fopen("net_status.txt", "w");
    if (!f) return;
    int remotos = 0, visibles = 0;
    float cercano = -1.0f;
    for (int i = 0; i < NET_MAX_PLAYERS; i++) {
        NetRemote *r = &sSnap[i];
        if (!r->used || !r->hasState) continue;
        remotos++;
        if (r->st.level == gCurrLevelNum && r->st.area == gCurrAreaIndex) {
            visibles++;
            float dx = r->st.p[0] - gMarioObject->header.gfx.pos[0];
            float dy = r->st.p[1] - gMarioObject->header.gfx.pos[1];
            float dz = r->st.p[2] - gMarioObject->header.gfx.pos[2];
            float d = sqrtf(dx * dx + dy * dy + dz * dz);
            if (cercano < 0 || d < cercano) cercano = d;
        }
    }
    fprintf(f, "conectado=%d mi_id=%d personaje=%d remotos=%d en_mi_nivel=%d distancia_mas_cercana=%.0f objetos_enviados=%d recibidos=%d aplicados=%d\n",
            nt_connected(), nt_my_id(), nt_my_char(), remotos, visibles, cercano, sObjSent, sObjRecv, sObjApplied);
    fclose(f);
}

static s32 obj_is_remote(struct Object *obj, int slot) {
    return obj != NULL && obj->activeFlags != ACTIVE_FLAG_DEACTIVATED
           && obj->behavior == segmented_to_virtual(bhvRemotePlayer) && obj->oBehParams2ndByte == slot;
}

void remote_players_frame(void) {
    // Mi propio personaje (tambien sin conexion): cambia el modelo del Mario local
    if (gMarioObject != NULL) {
        struct GraphNode *node = gLoadedGraphNodes[model_for_char(nt_my_char())];
        if (node != NULL) gMarioObject->header.gfx.sharedChild = node;
    }

    if (!nt_enabled()) return;
    send_local_state();
    nt_snapshot(sSnap);
    poll_events();
    poll_chat();
    write_status();

    for (int i = 0; i < NET_MAX_PLAYERS; i++) {
        NetRemote *r = &sSnap[i];
        s32 here = r->used && r->hasState && r->st.level == gCurrLevelNum && r->st.area == gCurrAreaIndex
                   && gMarioObject != NULL;
        if (!here) {
            if (obj_is_remote(sObj[i], i)) obj_mark_for_deletion(sObj[i]);
            sObj[i] = NULL;
            continue;
        }
        // si el jugador cambio de personaje, se vuelve a crear con el modelo nuevo
        if (obj_is_remote(sObj[i], i) && sObjChar[i] != r->ch) {
            obj_mark_for_deletion(sObj[i]);
            sObj[i] = NULL;
            continue;
        }
        if (!obj_is_remote(sObj[i], i)) {
            struct Object *obj = spawn_object(gMarioObject, model_for_char(r->ch), bhvRemotePlayer);
            obj->oBehParams2ndByte = i;
            obj->header.gfx.unk38.animID = -1; // forzar la primera carga de animacion
            obj->oPosX = r->st.p[0]; obj->oPosY = r->st.p[1]; obj->oPosZ = r->st.p[2];
            sObj[i] = obj;
            sObjChar[i] = r->ch;
        }
    }
    player_contact();
}

// Nombres sobre la cabeza de cada jugador: se proyecta su posicion con la camara actual
// y se dibuja con la tipografia colorida del HUD (coordenadas de pantalla 320x240).
void remote_players_draw_names(void) {
    if (!nt_enabled() || gMarioObject == NULL) return;
    if (sToastTimer > 0) { sToastTimer--; print_text_centered(160, 190, sToast); }

    f32 *cp = gLakituState.curPos, *cf = gLakituState.curFocus;
    f32 fwd[3] = { cf[0] - cp[0], cf[1] - cp[1], cf[2] - cp[2] };
    f32 fl = sqrtf(fwd[0] * fwd[0] + fwd[1] * fwd[1] + fwd[2] * fwd[2]);
    if (fl < 1.0f) return;
    for (int k = 0; k < 3; k++) fwd[k] /= fl;

    f32 right[3] = { -fwd[2], 0.0f, fwd[0] }; // fwd x (0,1,0)
    f32 rl = sqrtf(right[0] * right[0] + right[2] * right[2]);
    if (rl < 0.001f) return; // mirando recto hacia arriba/abajo
    right[0] /= rl; right[2] /= rl;
    f32 up[3] = { right[1] * fwd[2] - right[2] * fwd[1], right[2] * fwd[0] - right[0] * fwd[2],
                  right[0] * fwd[1] - right[1] * fwd[0] };

    f32 tanv = tanf(sFOVState.fov * 0.5f * 3.14159265f / 180.0f);
    if (tanv < 0.05f) return;

    for (int i = 0; i < NET_MAX_PLAYERS; i++) {
        NetRemote *r = &sSnap[i];
        if (!r->used || !r->hasState || r->st.level != gCurrLevelNum || r->st.area != gCurrAreaIndex) continue;

        f32 base[3] = { r->st.p[0], r->st.p[1], r->st.p[2] };
        if (obj_is_remote(sObj[i], i)) { base[0] = sObj[i]->oPosX; base[1] = sObj[i]->oPosY; base[2] = sObj[i]->oPosZ; }

        f32 v[3] = { base[0] - cp[0], base[1] - cp[1], base[2] - cp[2] };
        f32 z = v[0] * fwd[0] + v[1] * fwd[1] + v[2] * fwd[2];
        if (z < 50.0f || z > 9000.0f) continue;
        f32 x = v[0] * right[0] + v[1] * right[1] + v[2] * right[2];
        f32 y = v[0] * up[0] + v[1] * up[1] + v[2] * up[2];
        f32 nx = x / (z * tanv * (4.0f / 3.0f));
        f32 nyFeet = y / (z * tanv);
        f32 ny = (y + 185.0f) / (z * tanv); // el nombre flota sobre la cabeza
        f32 nxMax = gfx_current_dimensions.aspect_ratio * (3.0f / 4.0f); // 1.0 en 4:3, mas en pantalla panoramica
        if (nx < -nxMax || nx > nxMax || nyFeet < -1.0f || nyFeet > 1.0f) continue; // el jugador no se ve
        if (ny > 0.9f) ny = 0.9f; // si el nombre se saldria por arriba, queda pegado al borde

        char label[20];
        if (r->name[0]) snprintf(label, sizeof(label), "%.15s", r->name);
        else snprintf(label, sizeof(label), "J%d", r->id);
        print_text_centered((s32) (160.0f + nx * 160.0f), (s32) (120.0f + ny * 120.0f), label);
    }
}

// Misma logica que set_mario_anim_with_accel, pero con el buffer del jugador remoto
static void remote_set_anim(struct Object *obj, int slot, s32 animID, s32 accel) {
    struct MarioAnimation *ma = &sAnim[slot];
    if (ma->targetAnim == NULL) {
        ma->animDmaTable = D_80339D10.animDmaTable;
        ma->currentAnimAddr = NULL;
        ma->targetAnim = (struct Animation *) sAnimBuf[slot];
    }
    struct Animation *targetAnim = ma->targetAnim;
    struct GraphNodeObject_sub *ai = &obj->header.gfx.unk38;

    // El Mario local arranca con animID -1 (sin animacion): no hay nada que mostrar todavia
    if (animID < 0 || (u32) animID >= ma->animDmaTable->count) return;

    if (load_patchable_table(ma, animID)) {
        targetAnim->values = (void *) VIRTUAL_TO_PHYSICAL((u8 *) targetAnim + (uintptr_t) targetAnim->values);
        targetAnim->index = (void *) VIRTUAL_TO_PHYSICAL((u8 *) targetAnim + (uintptr_t) targetAnim->index);
    }
    if (ma->currentAnimAddr == NULL) return; // la animacion no se pudo cargar
    if (ai->animID != animID) {
        ai->animID = animID;
        ai->curAnim = targetAnim;
        ai->animYTrans = gMarioState->unkB0;
        if (targetAnim->flags & ANIM_FLAG_2) {
            ai->animFrameAccelAssist = (targetAnim->unk04 << 0x10);
        } else if (targetAnim->flags & ANIM_FLAG_FORWARD) {
            ai->animFrameAccelAssist = (targetAnim->unk04 << 0x10) + accel;
        } else {
            ai->animFrameAccelAssist = (targetAnim->unk04 << 0x10) - accel;
        }
        ai->animFrame = (ai->animFrameAccelAssist >> 0x10);
    }
    ai->animAccel = accel;
}

static s16 approach_angle(s16 cur, s16 target) {
    return cur + (s16) ((s16) (target - cur) * 0.4f);
}

void bhv_remote_player_loop(void) {
    struct Object *obj = gCurrentObject;
    int slot = obj->oBehParams2ndByte;
    NetRemote *r = &sSnap[slot];

    if (!r->used || !r->hasState || r->st.level != gCurrLevelNum || r->st.area != gCurrAreaIndex) {
        obj_mark_for_deletion(obj);
        return;
    }

    f32 dx = r->st.p[0] - obj->oPosX, dy = r->st.p[1] - obj->oPosY, dz = r->st.p[2] - obj->oPosZ;
    if (dx * dx + dy * dy + dz * dz > 1000.0f * 1000.0f) { // teletransporte (puerta, tuberia...)
        obj->oPosX = r->st.p[0]; obj->oPosY = r->st.p[1]; obj->oPosZ = r->st.p[2];
    } else {
        obj->oPosX += dx * 0.4f; obj->oPosY += dy * 0.4f; obj->oPosZ += dz * 0.4f;
    }
    obj->oFaceAnglePitch = approach_angle(obj->oFaceAnglePitch, (s16) r->st.g[0]);
    obj->oFaceAngleYaw = approach_angle(obj->oFaceAngleYaw, (s16) r->st.g[1]);
    obj->oFaceAngleRoll = approach_angle(obj->oFaceAngleRoll, (s16) r->st.g[2]);
    obj->oMoveAngleYaw = obj->oFaceAngleYaw;

    int newAnim = obj->header.gfx.unk38.animID != r->st.anim;
    remote_set_anim(obj, slot, r->st.anim, r->st.accel);
    // resincronizar el fotograma si se desvia demasiado (la red llega a ~15 Hz)
    s32 diff = (s32) obj->header.gfx.unk38.animFrame - r->st.frame;
    if (newAnim || diff > 4 || diff < -4) {
        obj->header.gfx.unk38.animFrame = r->st.frame;
        obj->header.gfx.unk38.animFrameAccelAssist = r->st.frame << 0x10;
    }
}
