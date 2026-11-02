#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <PR/ultratypes.h>

#include "sm64.h"
#include "area.h"
#include "camera.h"
#include "engine/math_util.h"
#include "engine/surface_collision.h"
#include "level_update.h"
#include "mario.h"
#include "audio/external.h"
#include "audio/seqplayer.h"
#include "seq_ids.h"
#include "sound_init.h"
#include "save_file.h"
#include "course_table.h"
#include "pc/fs/fs.h"
#include "ribbon.h"

extern void initiate_warp(s16 destLevel, s16 destArea, s16 destWarpNode, s32 arg3);
extern struct SaveBuffer gSaveBuffer;
#define WARP_TYPE_NOT_WARPING 0 // igual que en level_update.c

#define RIBBON_FILE  "puntos_control.bin"
#define RIBBON_MAGIC  0x52494232 // "RIB2"
#define RIBBON_UNDO   RIBBON_SLOTS // ranura extra: estado previo a la ultima carga
#define RIBBON_TOTAL  (RIBBON_SLOTS + 1)
#define CAP_BITS      (MARIO_NORMAL_CAP | MARIO_WING_CAP | MARIO_METAL_CAP | MARIO_VANISH_CAP)
#define RIBBON_WAIT   4          // frames de espera tras llegar al nivel destino

struct Checkpoint {
    s16 used;
    s16 level, area, node;
    s16 yaw;
    f32 pos[3];
    // avance completo: partida guardada (estrellas, llaves, monedas por curso...) y estado de Mario
    s16 lives, coins, health, capTimer;
    u32 capFlags;
    struct SaveFile save;
};

static struct Checkpoint sSlots[RIBBON_TOTAL];
static int sLoaded;

static struct Checkpoint sPending;
static int sHasPending;
static int sWarpStarted;
static int sWait;
static int sUnpause;

static void ribbon_read_file(void) {
    u32 magic = 0;
    FILE *f;
    if (sLoaded) return;
    sLoaded = 1;
    f = fopen(fs_get_write_path(RIBBON_FILE), "rb");
    if (f == NULL) return;
    if (fread(&magic, sizeof(magic), 1, f) == 1 && magic == RIBBON_MAGIC) {
        if (fread(sSlots, sizeof(sSlots), 1, f) != 1) memset(sSlots, 0, sizeof(sSlots));
    }
    fclose(f);
}

static void ribbon_write_file(void) {
    u32 magic = RIBBON_MAGIC;
    FILE *f = fopen(fs_get_write_path(RIBBON_FILE), "wb");
    if (f == NULL) return;
    fwrite(&magic, sizeof(magic), 1, f);
    fwrite(sSlots, sizeof(sSlots), 1, f);
    fclose(f);
}

// Registro de depuracion (solo si existe la variable SM64_CAPPY_LOG)
static void ribbon_log(const char *what, f32 x, f32 y, f32 z) {
    const char *path = getenv("SM64_CAPPY_LOG");
    FILE *f = path ? fopen(path, "a") : NULL;
    if (f) { fprintf(f, "ribbon %s %.0f %.0f %.0f lvl=%d area=%d vidas=%d monedas=%d estrellas=%d\n", what, x, y, z, gCurrLevelNum, gCurrentArea ? gCurrentArea->index : -1, gMarioState->numLives, gMarioState->numCoins, gMarioState->numStars); fclose(f); }
}

int ribbon_slot_used(int slot) {
    ribbon_read_file();
    return slot >= 0 && slot < RIBBON_TOTAL && sSlots[slot].used;
}

static void ribbon_beep_ok(void) {
    play_sound(SOUND_MENU_STAR_SOUND, gDefaultSoundArgs);
}

static void ribbon_beep_fail(void) {
    play_sound(SOUND_MENU_CAMERA_BUZZ, gDefaultSoundArgs);
}

static int ribbon_capture(struct Checkpoint *c) {
    struct MarioState *m = gMarioState;
    if (gCurrentArea == NULL || gCurrLevelNum == 0 || m == NULL || m->action == ACT_UNINITIALIZED
        || gCurrSaveFileNum < 1) return 0;
    c->used = 1;
    c->level = gCurrLevelNum;
    c->area = gCurrentArea->index;
    // nodo de aparicion valido de esta zona (el destino del ultimo aviso)
    c->node = (sWarpDest.nodeId < 0xF0) ? sWarpDest.nodeId : 0x0A;
    c->yaw = m->faceAngle[1];
    c->pos[0] = m->pos[0];
    c->pos[1] = m->pos[1];
    c->pos[2] = m->pos[2];
    c->lives = m->numLives;
    c->coins = m->numCoins;
    c->health = m->health;
    c->capTimer = m->capTimer;
    c->capFlags = m->flags & CAP_BITS;
    c->save = gSaveBuffer.files[gCurrSaveFileNum - 1][0];
    return 1;
}

void ribbon_save_slot(int slot) {
    ribbon_read_file();
    if (slot < 0 || slot >= RIBBON_SLOTS || !ribbon_capture(&sSlots[slot])) {
        ribbon_beep_fail();
        return;
    }
    ribbon_write_file();
    ribbon_log("guardar", sSlots[slot].pos[0], sSlots[slot].pos[1], sSlots[slot].pos[2]);
    ribbon_beep_ok();
}

void ribbon_load_slot(int slot) {
    ribbon_read_file();
    if (slot < 0 || slot >= RIBBON_TOTAL || !sSlots[slot].used || gCurrentArea == NULL) {
        ribbon_beep_fail();
        return;
    }
    if (slot != RIBBON_UNDO) { // antes de cargar se guarda el estado actual para poder deshacer
        struct Checkpoint cur;
        memset(&cur, 0, sizeof(cur));
        if (ribbon_capture(&cur)) {
            sSlots[RIBBON_UNDO] = cur;
            ribbon_write_file();
        }
    }
    sPending = sSlots[slot];
    sHasPending = 1;
    sWarpStarted = 0;
    sWait = 0;
    sUnpause = 1;
    ribbon_beep_ok();
}

void ribbon_give_lives(int n) {
    int v = gMarioState->numLives + n;
    gMarioState->numLives = (v > 99) ? 99 : (v < 0 ? 0 : v);
    ribbon_log("vidas", gMarioState->pos[0], gMarioState->pos[1], gMarioState->pos[2]);
    play_sound(SOUND_GENERAL_COLLECT_1UP, gDefaultSoundArgs);
}

void ribbon_full_health(void) {
    gMarioState->health = 0x880;
    gMarioState->healCounter = 0;
    gMarioState->hurtCounter = 0;
    ribbon_beep_ok();
}

void ribbon_give_coins(int n) {
    gMarioState->numCoins += n;
    gHudDisplay.coins = gMarioState->numCoins;
    play_sound(SOUND_GENERAL_COIN, gDefaultSoundArgs);
}

void ribbon_give_cap(u32 capFlag) {
    struct MarioState *m = gMarioState;
    u16 music = SEQUENCE_ARGS(4, SEQ_EVENT_POWERUP);
    u16 time = 600;

    m->flags &= ~(MARIO_WING_CAP | MARIO_METAL_CAP | MARIO_VANISH_CAP);
    m->flags |= capFlag | MARIO_CAP_ON_HEAD;
    if (capFlag == MARIO_WING_CAP) time = 1800;
    if (capFlag == MARIO_METAL_CAP) music = SEQUENCE_ARGS(4, SEQ_EVENT_METAL_CAP);
    m->capTimer = time;
    play_cap_music(music);
    ribbon_beep_ok();
}

static void ribbon_apply(struct MarioState *m) {
    struct Checkpoint *c = &sPending;
    struct Surface *floor = NULL;

    // avance: se restaura la partida guardada tal como estaba y el estado de Mario
    if (gCurrSaveFileNum >= 1) {
        gSaveBuffer.files[gCurrSaveFileNum - 1][0] = c->save;
        gSaveFileModified = TRUE;
        save_file_do_save(gCurrSaveFileNum - 1);
        m->numStars = save_file_get_total_star_count(gCurrSaveFileNum - 1, COURSE_MIN - 1, COURSE_MAX - 1);
    }
    m->numLives = c->lives;
    m->numCoins = c->coins;
    gHudDisplay.coins = m->numCoins;
    gHudDisplay.stars = m->numStars;
    gHudDisplay.lives = m->numLives;
    m->flags &= ~(CAP_BITS | MARIO_CAP_ON_HEAD);
    m->flags |= c->capFlags;
    if (c->capFlags) m->flags |= MARIO_CAP_ON_HEAD;
    m->capTimer = c->capTimer;

    find_floor(c->pos[0], c->pos[1], c->pos[2], &floor);
    if (floor == NULL) { // sin suelo en ese punto: Mario se queda donde aparecio
        ribbon_log("sin suelo", c->pos[0], c->pos[1], c->pos[2]);
        sHasPending = 0;
        return;
    }

    m->pos[0] = c->pos[0];
    m->pos[1] = c->pos[1];
    m->pos[2] = c->pos[2];
    m->faceAngle[1] = c->yaw;
    m->vel[0] = m->vel[1] = m->vel[2] = 0.0f;
    m->forwardVel = 0.0f;
    m->health = c->health < 0x100 ? 0x880 : c->health;
    m->floorHeight = find_floor(m->pos[0], m->pos[1], m->pos[2], &m->floor);
    vec3f_copy(m->marioObj->header.gfx.pos, m->pos);
    set_mario_action(m, (m->pos[1] > m->floorHeight + 10.0f) ? ACT_FREEFALL : ACT_IDLE, 0);
    reset_camera(gCurrentArea->camera);
    ribbon_log("cargado", m->pos[0], m->pos[1], m->pos[2]);
    sHasPending = 0;
}

void ribbon_update(struct MarioState *m) {
    if (!sHasPending) return;

    if (!sWarpStarted) {
        if (sPending.level == gCurrLevelNum && sPending.area == gCurrentArea->index) {
            ribbon_apply(m); // misma zona: basta con mover a Mario
            return;
        }
        initiate_warp(sPending.level, sPending.area, sPending.node, 0);
        play_transition(WARP_TRANSITION_FADE_INTO_COLOR, 20, 0x00, 0x00, 0x00);
        level_set_transition(30, NULL);
        sWarpStarted = 1;
        return;
    }

    // esperar a que el nivel destino este cargado y Mario aparecido
    if (sPending.level == gCurrLevelNum && sPending.area == gCurrentArea->index
        && sWarpDest.type == WARP_TYPE_NOT_WARPING && m->action != ACT_UNINITIALIZED) {
        if (++sWait >= RIBBON_WAIT) ribbon_apply(m);
    }
}

int ribbon_take_unpause(void) {
    int r = sUnpause;
    sUnpause = 0;
    return r;
}
