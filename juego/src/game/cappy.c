#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <PR/ultratypes.h>

#include "sm64.h"
#include "area.h"
#include "behavior_data.h"
#include "camera.h"
#include "engine/behavior_script.h"
#include "engine/math_util.h"
#include "engine/surface_collision.h"
#include "game_init.h"
#include "interaction.h"
#include "level_update.h"
#include "model_ids.h"
#include "object_helpers.h"
#include "object_list_processor.h"
#include "audio/external.h"
#include "spawn_object.h"
#include "mario.h"
#include "cappy.h"

extern u32 attack_object(struct Object *o, s32 interaction);
#define INT_FAST_ATTACK_OR_SHELL (1 << 5) // igual que en interaction.c

#define CAPPY_OUT_FRAMES   14    // vuelo de ida
#define CAPPY_HOVER_MIN    10    // flotando en el sitio (toque corto)
#define CAPPY_HOVER_MAX    150   // flotando mientras se mantiene la tecla (~5 s)
#define CAPPY_SPEED        55.0f
#define CAPPY_RETURN_SPEED 70.0f
#define CAPPY_HIT_RADIUS   140.0f
#define CAPPY_THROW_HEIGHT 70.0f // sale a la altura de la mano de Mario
#define CAPPY_TOP          32.0f // altura de la parte de arriba de la gorra (para saltar encima)
#define CAPPY_JUMP_RADIUS  95.0f
#define CAPPY_JUMP_VEL     62.0f

enum { CAPPY_OUT, CAPPY_HOVER, CAPPY_BACK };

static struct Object *sCappy;
static int sCapOff; // Mario esta sin gorra porque la lanzo

// Registro de depuracion (solo si existe la variable SM64_CAPPY_LOG)
static void cappy_log(const char *what, int n) {
    const char *path = getenv("SM64_CAPPY_LOG");
    if (!path) return;
    FILE *f = fopen(path, "a");
    if (f) { fprintf(f, "%s %d\n", what, n); fclose(f); }
}

static int cappy_alive(void) {
    return sCappy != NULL && sCappy->activeFlags != ACTIVE_FLAG_DEACTIVATED
           && sCappy->behavior == segmented_to_virtual(bhvCappy);
}

// Solo se puede lanzar con la gorra normal puesta, en tierra o en el aire.
static int cappy_can_throw(struct MarioState *m) {
    const u32 power = MARIO_WING_CAP | MARIO_METAL_CAP | MARIO_VANISH_CAP;
    u32 group = m->action & ACT_GROUP_MASK;
    if (!(m->flags & MARIO_NORMAL_CAP) || !(m->flags & MARIO_CAP_ON_HEAD) || (m->flags & power)) return 0;
    if (m->health < 0x100 || m->heldObj != NULL) return 0;
    return group == ACT_GROUP_STATIONARY || group == ACT_GROUP_MOVING || group == ACT_GROUP_AIRBORNE;
}

static void cappy_throw(struct MarioState *m) {
    sCappy = spawn_object(m->marioObj, MODEL_CAPPY, bhvCappy);
    if (sCappy == NULL) return;
    sCappy->oPosX = m->pos[0] + sins(m->faceAngle[1]) * 60.0f;
    sCappy->oPosY = m->pos[1] + CAPPY_THROW_HEIGHT;
    sCappy->oPosZ = m->pos[2] + coss(m->faceAngle[1]) * 60.0f;
    sCappy->oMoveAngleYaw = m->faceAngle[1];
    sCappy->oFaceAngleYaw = m->faceAngle[1];
    sCappy->oRoom = -1;
    sCappy->oOpacity = 0xFF; // el modelo de la gorra usa la opacidad; en 0 seria invisible
    sCappy->oAction = CAPPY_OUT;

    // Mario se queda sin gorra y hace el gesto de lanzar
    m->flags &= ~MARIO_CAP_ON_HEAD;
    sCapOff = 1;
    if ((m->action & ACT_GROUP_MASK) == ACT_GROUP_AIRBORNE) {
        set_mario_action(m, ACT_AIR_THROW, 0);
    } else {
        set_mario_action(m, ACT_THROWING, 0);
    }
    cappy_log("lanzada", 0);
    play_sound(SOUND_ACTION_THROW, m->marioObj->header.gfx.cameraToObject);
}

// Saltar encima de la gorra: rebote alto y la gorra vuelve a Mario.
static void cappy_check_jump_on(struct MarioState *m) {
    struct Object *c = sCappy;
    if ((m->action & ACT_GROUP_MASK) != ACT_GROUP_AIRBORNE || m->vel[1] > 0.0f || c->oAction == CAPPY_BACK) return;

    f32 dx = m->pos[0] - c->oPosX, dz = m->pos[2] - c->oPosZ;
    f32 top = c->oPosY + CAPPY_TOP;
    if (dx * dx + dz * dz > CAPPY_JUMP_RADIUS * CAPPY_JUMP_RADIUS) return;
    if (m->pos[1] > top + 40.0f || m->pos[1] < top - 30.0f) return;

    m->pos[1] = top;
    set_mario_action(m, ACT_DOUBLE_JUMP, 0);
    m->vel[1] = CAPPY_JUMP_VEL;
    play_sound(SOUND_ACTION_BOUNCE_OFF_OBJECT, m->marioObj->header.gfx.cameraToObject);
    c->oAction = CAPPY_BACK;
    cappy_log("salto", 0);
}

void cappy_update(struct MarioState *m) {
    if (cappy_alive()) {
        cappy_check_jump_on(m);
        return;
    }
    if (sCapOff) { // la gorra volvio (o desaparecio): Mario se la pone
        sCapOff = 0;
        if ((m->flags & MARIO_NORMAL_CAP) && !(m->flags & MARIO_CAP_ON_HEAD)) m->flags |= MARIO_CAP_ON_HEAD;
    }
    if ((gPlayer1Controller->buttonPressed & U_JPAD) && cappy_can_throw(m)) cappy_throw(m);
}

static f32 dist3(struct Object *a, struct Object *b) {
    f32 dx = a->oPosX - b->oPosX, dy = a->oPosY - b->oPosY, dz = a->oPosZ - b->oPosZ;
    return sqrtf(dx * dx + dy * dy + dz * dz);
}

// Golpea enemigos y atrae monedas cercanas. Devuelve 1 si golpeo algo.
static int cappy_hit_things(struct Object *o, struct MarioState *m) {
    static const int lists[] = { OBJ_LIST_GENACTOR, OBJ_LIST_PUSHABLE, OBJ_LIST_DESTRUCTIVE,
                                 OBJ_LIST_LEVEL, OBJ_LIST_DEFAULT, OBJ_LIST_SURFACE };
    int hit = 0;
    for (unsigned i = 0; i < sizeof(lists) / sizeof(lists[0]); i++) {
        struct ObjectNode *head = &gObjectLists[lists[i]];
        for (struct ObjectNode *n = head->next; n != head;) {
            struct Object *t = (struct Object *) n;
            n = n->next;
            if (t == o || t == m->marioObj || t->activeFlags == ACTIVE_FLAG_DEACTIVATED) continue;
            if (dist3(o, t) > CAPPY_HIT_RADIUS + t->hitboxRadius) continue;

            if (t->oInteractType == INTERACT_COIN) {
                cappy_log("moneda", 0);
                // la moneda salta a Mario y la recoge de la forma normal
                t->oPosX = m->pos[0];
                t->oPosY = m->pos[1] + 40.0f;
                t->oPosZ = m->pos[2];
            } else if ((t->oInteractType & (INTERACT_BOUNCE_TOP | INTERACT_BOUNCE_TOP2 | INTERACT_BREAKABLE
                                            | INTERACT_HIT_FROM_BELOW))
                       && t->oInteractStatus == 0 && t->oIntangibleTimer == 0) {
                attack_object(t, INT_FAST_ATTACK_OR_SHELL);
                cappy_log("golpe", t->oInteractType);
                hit = 1;
            }
        }
    }
    return hit;
}

// Mira hacia la camara para que se vean los ojos al flotar.
static void cappy_face_camera(struct Object *o) {
    o->oFaceAngleYaw = atan2s(gLakituState.curPos[2] - o->oPosZ, gLakituState.curPos[0] - o->oPosX);
}

void bhv_cappy_loop(void) {
    struct Object *o = gCurrentObject;
    struct MarioState *m = gMarioState;

    cur_obj_enable_rendering(); // los objetos nuevos nacen ocultos

    switch (o->oAction) {
        case CAPPY_OUT: {
            o->oFaceAngleYaw += 0x3000; // giro
            o->oPosX += sins(o->oMoveAngleYaw) * CAPPY_SPEED;
            o->oPosZ += coss(o->oMoveAngleYaw) * CAPPY_SPEED;

            struct WallCollisionData cd;
            cd.x = o->oPosX; cd.y = o->oPosY; cd.z = o->oPosZ;
            cd.radius = 40.0f; cd.offsetY = 0.0f;
            int wall = find_wall_collisions(&cd) > 0;

            if (cappy_hit_things(o, m) || wall || o->oTimer >= CAPPY_OUT_FRAMES) {
                o->oAction = CAPPY_HOVER;
                o->oTimer = 0;
            }
            break;
        }
        case CAPPY_HOVER: {
            // flota en el sitio mirando a la camara; si se mantiene la tecla, se queda
            int held = (gPlayer1Controller->buttonDown & U_JPAD) != 0;
            cappy_face_camera(o);
            o->oPosY += sins(o->oTimer * 0x1000) * 1.5f;
            cappy_hit_things(o, m);
            if (o->oTimer >= CAPPY_HOVER_MAX || (o->oTimer >= CAPPY_HOVER_MIN && !held)) {
                o->oAction = CAPPY_BACK;
            }
            break;
        }
        case CAPPY_BACK: {
            o->oFaceAngleYaw += 0x3000;
            f32 tx = m->pos[0], ty = m->pos[1] + 100.0f, tz = m->pos[2];
            f32 dx = tx - o->oPosX, dy = ty - o->oPosY, dz = tz - o->oPosZ;
            f32 d = sqrtf(dx * dx + dy * dy + dz * dz);
            if (d < CAPPY_RETURN_SPEED) {
                cappy_log("vuelve", 0);
                obj_mark_for_deletion(o);
                return;
            }
            o->oPosX += dx / d * CAPPY_RETURN_SPEED;
            o->oPosY += dy / d * CAPPY_RETURN_SPEED;
            o->oPosZ += dz / d * CAPPY_RETURN_SPEED;
            cappy_hit_things(o, m);
            break;
        }
    }
}
