#ifdef EXT_OPTIONS_MENU

#include <stdio.h>
#include <string.h>

#include "sm64.h"
#include "text_strings.h"
#include "engine/math_util.h"
#include "audio/external.h"
#include "game/camera.h"
#include "game/level_update.h"
#include "game/print.h"
#include "game/segment2.h"
#include "game/save_file.h"
#ifdef BETTERCAMERA
#include "game/bettercamera.h"
#endif
#include "game/mario_misc.h"
#include "game/game_init.h"
#include "game/ingame_menu.h"
#include "game/options_menu.h"
#include "game/ribbon.h"
#include "game/chat.h"
#include "pc/net/net_transport.h"
#include "pc/pc_main.h"
#include "pc/cliopts.h"
#include "pc/cheats.h"
#include "pc/configfile.h"
#include "pc/controller/controller_api.h"

#include <stdbool.h>
#include "../../include/libc/stdlib.h"

u8 optmenu_open = 0;

static u8 optmenu_binding = 0;
static u8 optmenu_bind_idx = 0;

/* Keeps track of how many times the user has pressed L while in the options menu, so cheats can be unlocked */
static s32 l_counter = 0;

// How to add stuff:
// strings: add them to include/text_strings.h.in
//          and to menuStr[] / opts*Str[]
// options: add them to the relevant options list
// menus:   add a new submenu definition and a new
//          option to the optsMain list

static const u8 toggleStr[][16] = {
    { TEXT_OPT_DISABLED },
    { TEXT_OPT_ENABLED },
};

static const u8 optSmallStr[][32] = {
    { TEXT_OPT_BUTTON1 },
    { TEXT_OPT_BUTTON2 },
    { TEXT_OPT_L_HIGHLIGHT },
    { TEXT_OPT_R_HIGHLIGHT },
};

static const u8 menuStr[][32] = {
    { TEXT_OPT_OPTIONS },
    { TEXT_OPT_CAMERA },
    { TEXT_OPT_CONTROLS },
    { TEXT_OPT_VIDEO },
    { TEXT_OPT_AUDIO },
    { TEXT_EXIT_GAME },
    { TEXT_OPT_CHEATS },
    { TEXT_OPT_POINTS },
    { TEXT_OPT_PLAYER },
    { TEXT_OPT_MULTI },
};

static const u8 ribSaveStr[RIBBON_SLOTS][32] = {
    { TEXT_RIB_SAVE1 }, { TEXT_RIB_SAVE2 }, { TEXT_RIB_SAVE3 },
};

static const u8 ribLoadStr[RIBBON_SLOTS][32] = {
    { TEXT_RIB_LOAD1 }, { TEXT_RIB_LOAD2 }, { TEXT_RIB_LOAD3 },
};

static const u8 ribEmptyStr[RIBBON_SLOTS][32] = {
    { TEXT_RIB_EMPTY1 }, { TEXT_RIB_EMPTY2 }, { TEXT_RIB_EMPTY3 },
};

static const u8 ribUndoStr[2][32] = {
    { TEXT_RIB_UNDO }, { TEXT_RIB_NOUNDO },
};

static const u8 ribPlayerStr[][32] = {
    { TEXT_RIB_LIFE1 },
    { TEXT_RIB_LIFE5 },
    { TEXT_RIB_LIFE99 },
    { TEXT_RIB_HEALTH },
    { TEXT_RIB_COINS },
    { TEXT_RIB_WING },
    { TEXT_RIB_METAL },
    { TEXT_RIB_VANISH },
};

static const u8 optsCameraStr[][32] = {
    { TEXT_OPT_CAMX },
    { TEXT_OPT_CAMY },
    { TEXT_OPT_INVERTX },
    { TEXT_OPT_INVERTY },
    { TEXT_OPT_CAMC },
    { TEXT_OPT_CAMP },
    { TEXT_OPT_ANALOGUE },
    { TEXT_OPT_MOUSE },
    { TEXT_OPT_CAMD },
    { TEXT_OPT_CAMON },
};

static const u8 optsVideoStr[][32] = {
    { TEXT_OPT_FSCREEN },
    { TEXT_OPT_TEXFILTER },
    { TEXT_OPT_NEAREST },
    { TEXT_OPT_LINEAR },
    { TEXT_OPT_RESETWND },
    { TEXT_OPT_VSYNC },
    { TEXT_OPT_AUTO },
    { TEXT_OPT_HUD },
    { TEXT_OPT_THREEPT },
    { TEXT_OPT_APPLY },
    { TEXT_OPT_BICUBIC },
};

static const u8 optsAudioStr[][32] = {
    { TEXT_OPT_MVOLUME },    
    { TEXT_OPT_MUSVOLUME },
    { TEXT_OPT_SFXVOLUME },
    { TEXT_OPT_ENVVOLUME },
};

static const u8 optsCheatsStr[][64] = {
    { TEXT_OPT_CHEAT1 },
    { TEXT_OPT_CHEAT2 },
    { TEXT_OPT_CHEAT3 },
    { TEXT_OPT_CHEAT4 },
    { TEXT_OPT_CHEAT5 },
    { TEXT_OPT_CHEAT6 },
    { TEXT_OPT_CHEAT7 },
    { TEXT_OPT_CHEAT8 },
    { TEXT_OPT_CHEAT9 },
};

static const u8 bindStr[][32] = {
    { TEXT_OPT_UNBOUND },
    { TEXT_OPT_PRESSKEY },
    { TEXT_BIND_A },
    { TEXT_BIND_B },
    { TEXT_BIND_START },
    { TEXT_BIND_L },
    { TEXT_BIND_R },
    { TEXT_BIND_Z },
    { TEXT_BIND_C_UP },
    { TEXT_BIND_C_DOWN },
    { TEXT_BIND_C_LEFT },
    { TEXT_BIND_C_RIGHT },
    { TEXT_BIND_UP },
    { TEXT_BIND_DOWN },
    { TEXT_BIND_LEFT },
    { TEXT_BIND_RIGHT },
    { TEXT_OPT_DEADZONE },
    { TEXT_OPT_RUMBLE },
};

static const u8 *filterChoices[] = {
    optsVideoStr[2],
    optsVideoStr[3],
    optsVideoStr[8],
    optsVideoStr[10],
};

static const u8 *vsyncChoices[] = {
    toggleStr[0],
    toggleStr[1],
    optsVideoStr[6],
};

enum OptType {
    OPT_INVALID = 0,
    OPT_TOGGLE,
    OPT_CHOICE,
    OPT_SCROLL,
    OPT_SUBMENU,
    OPT_BIND,
    OPT_BUTTON,
};

struct SubMenu;

struct Option {
    enum OptType type;
    const u8 *label;
    union {
        u32 *uval;
        bool *bval;
    };
    union {
        struct {
            const u8 **choices;
            u32 numChoices;
        };
        struct {
            u32 scrMin;
            u32 scrMax;
            u32 scrStep;
        };
        struct SubMenu *nextMenu;
        void (*actionFn)(struct Option *, s32);
    };
};

struct SubMenu {
    struct SubMenu *prev; // this is set at runtime to avoid needless complication
    const u8 *label;
    struct Option *opts;
    s32 numOpts;
    s32 select;
    s32 scroll;
};

/* helper macros */

#define DEF_OPT_TOGGLE(lbl, bv) \
    { .type = OPT_TOGGLE, .label = lbl, .bval = bv }
#define DEF_OPT_SCROLL(lbl, uv, min, max, st) \
    { .type = OPT_SCROLL, .label = lbl, .uval = uv, .scrMin = min, .scrMax = max, .scrStep = st }
#define DEF_OPT_CHOICE(lbl, uv, ch) \
    { .type = OPT_CHOICE, .label = lbl, .uval = uv, .choices = ch, .numChoices = sizeof(ch) / sizeof(ch[0]) }
#define DEF_OPT_SUBMENU(lbl, nm) \
    { .type = OPT_SUBMENU, .label = lbl, .nextMenu = nm }
#define DEF_OPT_BIND(lbl, uv) \
    { .type = OPT_BIND, .label = lbl, .uval = uv }
#define DEF_OPT_BUTTON(lbl, act) \
    { .type = OPT_BUTTON, .label = lbl, .actionFn = act }
#define DEF_SUBMENU(lbl, opt) \
    { .label = lbl, .opts = opt, .numOpts = sizeof(opt) / sizeof(opt[0]) }

/* button action functions */

static void optmenu_act_exit(UNUSED struct Option *self, s32 arg) {
    if (!arg) game_exit(); // only exit on A press and not directions
}

static void optvideo_reset_window(UNUSED struct Option *self, s32 arg) {
    if (!arg) {
        // Restrict reset to A press and not directions
        configWindow.reset = true;
        configWindow.settings_changed = true;
    }
}

static void optvideo_apply(UNUSED struct Option *self, s32 arg) {
    if (!arg) configWindow.settings_changed = true;
}

/* cinta de herramientas: puntos de control y ayudas */

static struct Option optsPoints[2 * RIBBON_SLOTS + 1];

static void ribbon_refresh_labels(void) {
    for (int i = 0; i < RIBBON_SLOTS; i++) {
        optsPoints[RIBBON_SLOTS + i].label = ribbon_slot_used(i) ? ribLoadStr[i] : ribEmptyStr[i];
    }
    optsPoints[2 * RIBBON_SLOTS].label = ribbon_slot_used(RIBBON_SLOTS) ? ribUndoStr[0] : ribUndoStr[1];
}

static void optribbon_save(struct Option *self, s32 arg) {
    if (arg) return;
    ribbon_save_slot((int)(self - optsPoints));
    ribbon_refresh_labels();
}

static void optribbon_load(struct Option *self, s32 arg) {
    if (arg) return;
    ribbon_load_slot((int)(self - optsPoints) - RIBBON_SLOTS);
    if (ribbon_slot_used((int)(self - optsPoints) - RIBBON_SLOTS)) optmenu_toggle(); // cierra el menu
}

static void optribbon_life1(UNUSED struct Option *self, s32 arg)  { if (!arg) ribbon_give_lives(1); }
static void optribbon_life5(UNUSED struct Option *self, s32 arg)  { if (!arg) ribbon_give_lives(5); }
static void optribbon_life99(UNUSED struct Option *self, s32 arg) { if (!arg) ribbon_give_lives(99); }
static void optribbon_health(UNUSED struct Option *self, s32 arg) { if (!arg) ribbon_full_health(); }
static void optribbon_coins(UNUSED struct Option *self, s32 arg)  { if (!arg) ribbon_give_coins(25); }
static void optribbon_wing(UNUSED struct Option *self, s32 arg)   { if (!arg) ribbon_give_cap(MARIO_WING_CAP); }
static void optribbon_metal(UNUSED struct Option *self, s32 arg)  { if (!arg) ribbon_give_cap(MARIO_METAL_CAP); }
static void optribbon_vanish(UNUSED struct Option *self, s32 arg) { if (!arg) ribbon_give_cap(MARIO_VANISH_CAP); }

/* multijugador: etiquetas dinamicas (texto ASCII convertido a la fuente del juego) */

#define MP_LINES 6
#define MP_CHAT 4
#define MP_N (4 + MP_LINES + MP_CHAT)

static struct Option optsMulti[MP_N];

static u8 mpStatusStr[40], mpToggleStr[24], mpRoomStr[40], mpCharStr[32], mpLineStr[MP_LINES][32];
static u8 mpChatStr[MP_CHAT][24];
static const char *mpRooms[] = { "publica", "sala1", "sala2", "sala3", "sala4" };
static const char *mpChars[] = { "Mario", "Luigi", "Wario", "Waluigi" };
static const char *mpChat[MP_CHAT] = { "Hola!", "Ayuda!", "Vamos!", "Gracias!" };

static void mp_str(u8 *dst, size_t cap, const char *s) {
    size_t n = 0;
    for (; *s && n + 1 < cap; s++, n++) {
        char c = *s;
        if (c >= '0' && c <= '9') dst[n] = c - '0';
        else if (c >= 'A' && c <= 'Z') dst[n] = 10 + (c - 'A');
        else if (c >= 'a' && c <= 'z') dst[n] = 36 + (c - 'a');
        else if (c == '-') dst[n] = 0x9F;
        else if (c == '.') dst[n] = 0x3F;
        else if (c == ',') dst[n] = 0x6F;
        else if (c == ':') dst[n] = 0xE6;
        else if (c == '!') dst[n] = 0xF2;
        else if (c == '?') dst[n] = 0xF4;
        else if (c == '/') dst[n] = 0xD0;
        else dst[n] = 0x9E; // espacio y todo lo demas
    }
    dst[n] = 0xFF;
}

static void mp_refresh_labels(void) {
    char buf[48];
    NetRemote snap[NET_MAX_PLAYERS];
    int ch = nt_my_char(), line = 0;

    switch (nt_state()) {
        case 2:
            if (nt_ping_ms() >= 0) snprintf(buf, sizeof(buf), "Conectado %d ms", nt_ping_ms());
            else snprintf(buf, sizeof(buf), "Conectado");
            break;
        case 1: snprintf(buf, sizeof(buf), "Conectando..."); break;
        default: snprintf(buf, sizeof(buf), "Sin conexion"); break;
    }
    mp_str(mpStatusStr, sizeof(mpStatusStr), buf);
    mp_str(mpToggleStr, sizeof(mpToggleStr), nt_state() ? "Desconectar" : "Conectar");
    snprintf(buf, sizeof(buf), "Sala: %s", nt_room());
    mp_str(mpRoomStr, sizeof(mpRoomStr), buf);
    snprintf(buf, sizeof(buf), "Personaje: %s", mpChars[(ch >= 0 && ch < 4) ? ch : 0]);
    mp_str(mpCharStr, sizeof(mpCharStr), buf);

    nt_snapshot(snap);
    snprintf(buf, sizeof(buf), "Tu: %s", nt_my_name());
    mp_str(mpLineStr[line++], sizeof(mpLineStr[0]), buf);
    for (int i = 0; i < NET_MAX_PLAYERS && line < MP_LINES; i++) {
        if (!snap[i].used || snap[i].id == nt_my_id()) continue;
        snprintf(buf, sizeof(buf), "%d %s", snap[i].id, snap[i].name);
        mp_str(mpLineStr[line++], sizeof(mpLineStr[0]), buf);
    }
    for (; line < MP_LINES; line++) mp_str(mpLineStr[line], sizeof(mpLineStr[0]), "-");

    optsMulti[0].label = mpStatusStr;
    optsMulti[1].label = mpToggleStr;
    optsMulti[2].label = mpRoomStr;
    optsMulti[3].label = mpCharStr;
    for (int i = 0; i < MP_LINES; i++) optsMulti[4 + i].label = mpLineStr[i];
    for (int i = 0; i < MP_CHAT; i++) {
        snprintf(buf, sizeof(buf), "Decir: %s", mpChat[i]);
        mp_str(mpChatStr[i], sizeof(mpChatStr[0]), buf);
        optsMulti[4 + MP_LINES + i].label = mpChatStr[i];
    }
}

static void optmp_noop(UNUSED struct Option *self, UNUSED s32 arg) {}

static void optmp_toggle(UNUSED struct Option *self, s32 arg) {
    if (arg) return;
    nt_set_online(nt_state() == 0);
    mp_refresh_labels();
}

static void optmp_room(UNUSED struct Option *self, s32 arg) {
    int cur = -1, n = sizeof(mpRooms) / sizeof(mpRooms[0]);
    if (arg) return;
    for (int i = 0; i < n; i++) if (!strcmp(nt_room(), mpRooms[i])) cur = i;
    nt_set_room(mpRooms[(cur + 1) % n]);
    mp_refresh_labels();
}

static void optmp_char(UNUSED struct Option *self, s32 arg) {
    if (arg) return;
    nt_set_char((nt_my_char() + 1) % 4);
    mp_refresh_labels();
}

static void optmp_chat(struct Option *self, s32 arg) {
    const char *text = mpChat[self - &optsMulti[4 + MP_LINES]];
    if (arg || nt_state() != 2) return;
    nt_send_chat(text);
    chat_add(nt_my_name(), text);
}

/* submenu option lists */

#ifdef BETTERCAMERA
static struct Option optsCamera[] = {
    DEF_OPT_TOGGLE( optsCameraStr[9], &configEnableCamera ),
    DEF_OPT_TOGGLE( optsCameraStr[6], &configCameraAnalog ),
    DEF_OPT_TOGGLE( optsCameraStr[7], &configCameraMouse ),
    DEF_OPT_TOGGLE( optsCameraStr[2], &configCameraInvertX ),
    DEF_OPT_TOGGLE( optsCameraStr[3], &configCameraInvertY ),
    DEF_OPT_SCROLL( optsCameraStr[0], &configCameraXSens, 1, 100, 1 ),
    DEF_OPT_SCROLL( optsCameraStr[1], &configCameraYSens, 1, 100, 1 ),
    DEF_OPT_SCROLL( optsCameraStr[4], &configCameraAggr, 0, 100, 1 ),
    DEF_OPT_SCROLL( optsCameraStr[5], &configCameraPan, 0, 100, 1 ),
    DEF_OPT_SCROLL( optsCameraStr[8], &configCameraDegrade, 0, 100, 1 ),
};
#endif

static struct Option optsControls[] = {
    DEF_OPT_BIND( bindStr[ 2], configKeyA ),
    DEF_OPT_BIND( bindStr[ 3], configKeyB ),
    DEF_OPT_BIND( bindStr[ 4], configKeyStart ),
    DEF_OPT_BIND( bindStr[ 5], configKeyL ),
    DEF_OPT_BIND( bindStr[ 6], configKeyR ),
    DEF_OPT_BIND( bindStr[ 7], configKeyZ ),
    DEF_OPT_BIND( bindStr[ 8], configKeyCUp ),
    DEF_OPT_BIND( bindStr[ 9], configKeyCDown ),
    DEF_OPT_BIND( bindStr[10], configKeyCLeft ),
    DEF_OPT_BIND( bindStr[11], configKeyCRight ),
    DEF_OPT_BIND( bindStr[12], configKeyStickUp ),
    DEF_OPT_BIND( bindStr[13], configKeyStickDown ),
    DEF_OPT_BIND( bindStr[14], configKeyStickLeft ),
    DEF_OPT_BIND( bindStr[15], configKeyStickRight ),
    // max deadzone is 31000; this is less than the max range of ~32768, but this
    // way, the player can't accidentally lock themselves out of using the stick
    DEF_OPT_SCROLL( bindStr[16], &configStickDeadzone, 0, 100, 1 ),
    DEF_OPT_SCROLL( bindStr[17], &configRumbleStrength, 0, 100, 1)
};

static struct Option optsVideo[] = {
    DEF_OPT_TOGGLE( optsVideoStr[0], &configWindow.fullscreen ),
    DEF_OPT_TOGGLE( optsVideoStr[5], &configWindow.vsync ),
    DEF_OPT_CHOICE( optsVideoStr[1], &configFiltering, filterChoices ),
    DEF_OPT_TOGGLE( optsVideoStr[7], &configHUD ),
    DEF_OPT_BUTTON( optsVideoStr[4], optvideo_reset_window ),
    DEF_OPT_BUTTON( optsVideoStr[9], optvideo_apply ),
};

static struct Option optsAudio[] = {
    DEF_OPT_SCROLL( optsAudioStr[0], &configMasterVolume, 0, MAX_VOLUME, 1 ),
    DEF_OPT_SCROLL( optsAudioStr[1], &configMusicVolume, 0, MAX_VOLUME, 1),
    DEF_OPT_SCROLL( optsAudioStr[2], &configSfxVolume, 0, MAX_VOLUME, 1),
    DEF_OPT_SCROLL( optsAudioStr[3], &configEnvVolume, 0, MAX_VOLUME, 1),
};

static struct Option optsCheats[] = {
    DEF_OPT_TOGGLE( optsCheatsStr[0], &Cheats.EnableCheats ),
    DEF_OPT_TOGGLE( optsCheatsStr[1], &Cheats.MoonJump ),
    DEF_OPT_TOGGLE( optsCheatsStr[2], &Cheats.GodMode ),
    DEF_OPT_TOGGLE( optsCheatsStr[3], &Cheats.InfiniteLives ),
    DEF_OPT_TOGGLE( optsCheatsStr[4], &Cheats.SuperSpeed ),
    DEF_OPT_TOGGLE( optsCheatsStr[5], &Cheats.Responsive ),
    DEF_OPT_TOGGLE( optsCheatsStr[6], &Cheats.ExitAnywhere ),
    DEF_OPT_TOGGLE( optsCheatsStr[7], &Cheats.HugeMario ),
    DEF_OPT_TOGGLE( optsCheatsStr[8], &Cheats.TinyMario ),

};

static struct Option optsPoints[] = {
    DEF_OPT_BUTTON( ribSaveStr[0], optribbon_save ),
    DEF_OPT_BUTTON( ribSaveStr[1], optribbon_save ),
    DEF_OPT_BUTTON( ribSaveStr[2], optribbon_save ),
    DEF_OPT_BUTTON( ribLoadStr[0], optribbon_load ),
    DEF_OPT_BUTTON( ribLoadStr[1], optribbon_load ),
    DEF_OPT_BUTTON( ribLoadStr[2], optribbon_load ),
    DEF_OPT_BUTTON( ribUndoStr[1], optribbon_load ),
};

static struct Option optsPlayer[] = {
    DEF_OPT_BUTTON( ribPlayerStr[0], optribbon_life1 ),
    DEF_OPT_BUTTON( ribPlayerStr[1], optribbon_life5 ),
    DEF_OPT_BUTTON( ribPlayerStr[2], optribbon_life99 ),
    DEF_OPT_BUTTON( ribPlayerStr[3], optribbon_health ),
    DEF_OPT_BUTTON( ribPlayerStr[4], optribbon_coins ),
    DEF_OPT_BUTTON( ribPlayerStr[5], optribbon_wing ),
    DEF_OPT_BUTTON( ribPlayerStr[6], optribbon_metal ),
    DEF_OPT_BUTTON( ribPlayerStr[7], optribbon_vanish ),
};


static struct Option optsMulti[MP_N] = {
    DEF_OPT_BUTTON( mpStatusStr, optmp_noop ),
    DEF_OPT_BUTTON( mpToggleStr, optmp_toggle ),
    DEF_OPT_BUTTON( mpRoomStr, optmp_room ),
    DEF_OPT_BUTTON( mpCharStr, optmp_char ),
    DEF_OPT_BUTTON( mpLineStr[0], optmp_noop ),
    DEF_OPT_BUTTON( mpLineStr[1], optmp_noop ),
    DEF_OPT_BUTTON( mpLineStr[2], optmp_noop ),
    DEF_OPT_BUTTON( mpLineStr[3], optmp_noop ),
    DEF_OPT_BUTTON( mpLineStr[4], optmp_noop ),
    DEF_OPT_BUTTON( mpLineStr[5], optmp_noop ),
    DEF_OPT_BUTTON( mpChatStr[0], optmp_chat ),
    DEF_OPT_BUTTON( mpChatStr[1], optmp_chat ),
    DEF_OPT_BUTTON( mpChatStr[2], optmp_chat ),
    DEF_OPT_BUTTON( mpChatStr[3], optmp_chat ),
};
/* submenu definitions */

#ifdef BETTERCAMERA
static struct SubMenu menuCamera   = DEF_SUBMENU( menuStr[1], optsCamera );
#endif
static struct SubMenu menuControls = DEF_SUBMENU( menuStr[2], optsControls );
static struct SubMenu menuVideo    = DEF_SUBMENU( menuStr[3], optsVideo );
static struct SubMenu menuAudio    = DEF_SUBMENU( menuStr[4], optsAudio );
static struct SubMenu menuCheats   = DEF_SUBMENU( menuStr[6], optsCheats );
static struct SubMenu menuPoints   = DEF_SUBMENU( menuStr[7], optsPoints );
static struct SubMenu menuPlayer   = DEF_SUBMENU( menuStr[8], optsPlayer );
static struct SubMenu menuMulti    = DEF_SUBMENU( menuStr[9], optsMulti );

/* main options menu definition */

static struct Option optsMain[] = {
    DEF_OPT_SUBMENU( menuStr[7], &menuPoints ),
    DEF_OPT_SUBMENU( menuStr[8], &menuPlayer ),
    DEF_OPT_SUBMENU( menuStr[9], &menuMulti ),
    DEF_OPT_SUBMENU( menuStr[6], &menuCheats ),
#ifdef BETTERCAMERA
    DEF_OPT_SUBMENU( menuStr[1], &menuCamera ),
#endif
    DEF_OPT_SUBMENU( menuStr[2], &menuControls ),
    DEF_OPT_SUBMENU( menuStr[3], &menuVideo ),
    DEF_OPT_SUBMENU( menuStr[4], &menuAudio ),
    DEF_OPT_BUTTON ( menuStr[5], optmenu_act_exit ),
};

static struct SubMenu menuMain = DEF_SUBMENU( menuStr[0], optsMain );

/* implementation */

static s32 optmenu_option_timer = 0;
static u8 optmenu_hold_count = 0;

static struct SubMenu *currentMenu = &menuMain;

static inline s32 wrap_add(s32 a, const s32 b, const s32 min, const s32 max) {
    a += b;
    if (a < min) a = max - (min - a) + 1;
    else if (a > max) a = min + (a - max) - 1;
    return a;
}

static void uint_to_hex(u32 num, u8 *dst) {
    u8 places = 4;
    while (places--) {
        const u32 digit = num & 0xF;
        dst[places] = digit;
        num >>= 4;
    }
    dst[4] = DIALOG_CHAR_TERMINATOR;
}

//Displays a box.
static void optmenu_draw_box(s16 x1, s16 y1, s16 x2, s16 y2, u8 r, u8 g, u8 b) {
    gDPPipeSync(gDisplayListHead++);
    gDPSetRenderMode(gDisplayListHead++, G_RM_OPA_SURF, G_RM_OPA_SURF2);
    gDPSetCycleType(gDisplayListHead++, G_CYC_FILL);
    gDPSetFillColor(gDisplayListHead++, GPACK_RGBA5551(r, g, b, 255));
    gDPFillRectangle(gDisplayListHead++, x1, y1, x2 - 1, y2 - 1);
    gDPPipeSync(gDisplayListHead++);
    gDPSetCycleType(gDisplayListHead++, G_CYC_1CYCLE);
}

static void optmenu_draw_text(s16 x, s16 y, const u8 *str, u8 col) {
    const u8 textX = get_str_x_pos_from_center(x, (u8*)str, 10.0f);
    gDPSetEnvColor(gDisplayListHead++, 0, 0, 0, 255);
    print_generic_string(textX+1, y-1, str);
    if (col == 0) {
        gDPSetEnvColor(gDisplayListHead++, 255, 255, 255, 255);
    } else {
        gDPSetEnvColor(gDisplayListHead++, 255, 32, 32, 255);
    }
    print_generic_string(textX, y, str);
}

static void optmenu_draw_opt(const struct Option *opt, s16 x, s16 y, u8 sel) {
    u8 buf[32] = { 0 };

    if (opt->type == OPT_SUBMENU || opt->type == OPT_BUTTON)
        y -= 6;

    optmenu_draw_text(x, y, opt->label, sel);

    switch (opt->type) {
        case OPT_TOGGLE:
            optmenu_draw_text(x, y-13, toggleStr[(int)*opt->bval], sel);
            break;

        case OPT_CHOICE:
            optmenu_draw_text(x, y-13, opt->choices[*opt->uval], sel);
            break;

        case OPT_SCROLL:
            int_to_str(*opt->uval, buf);
            optmenu_draw_text(x, y-13, buf, sel);
            break;

        case OPT_BIND:
            x = 112;
            for (u8 i = 0; i < MAX_BINDS; ++i, x += 48) {
                const u8 white = (sel && (optmenu_bind_idx == i));
                // TODO: button names
                if (opt->uval[i] == VK_INVALID) {
                    if (optmenu_binding && white)
                        optmenu_draw_text(x, y-13, bindStr[1], 1);
                    else
                        optmenu_draw_text(x, y-13, bindStr[0], white);
                } else {
                    uint_to_hex(opt->uval[i], buf);
                    optmenu_draw_text(x, y-13, buf, white);
                }
            }
            break;

        default: break;
    };
}

static void optmenu_opt_change(struct Option *opt, s32 val) {
    switch (opt->type) {
        case OPT_TOGGLE:
            *opt->bval = !*opt->bval;
            break;

        case OPT_CHOICE:
            *opt->uval = wrap_add(*opt->uval, val, 0, opt->numChoices - 1);
            break;

        case OPT_SCROLL:
            *opt->uval = wrap_add(*opt->uval, opt->scrStep * val, opt->scrMin, opt->scrMax);
            break;

        case OPT_SUBMENU:
            opt->nextMenu->prev = currentMenu;
            currentMenu = opt->nextMenu;
            break;

        case OPT_BUTTON:
            if (opt->actionFn)
                opt->actionFn(opt, val);
            break;

        case OPT_BIND:
            if (val == 0xFF) {
                // clear the bind
                opt->uval[optmenu_bind_idx] = VK_INVALID;
            } else if (val == 0) {
                opt->uval[optmenu_bind_idx] = VK_INVALID;
                optmenu_binding = 1;
                controller_get_raw_key(); // clear the last key, which is probably A
            } else {
                optmenu_bind_idx = wrap_add(optmenu_bind_idx, val, 0, MAX_BINDS - 1);
            }
            break;

        default: break;
    }
}

static inline s16 get_hudstr_centered_x(const s16 sx, const u8 *str) {
    const u8 *chr = str;
    s16 len = 0;
    while (*chr != GLOBAR_CHAR_TERMINATOR) ++chr, ++len;
    return sx - len * 6; // stride is 12
}

//Options menu
void optmenu_draw(void) {
    s16 scroll;
    s16 scrollpos;

    if (currentMenu == &menuMulti) mp_refresh_labels();
    const s16 labelX = get_hudstr_centered_x(160, currentMenu->label);
    gSPDisplayList(gDisplayListHead++, dl_rgba16_text_begin);
    gDPSetEnvColor(gDisplayListHead++, 255, 255, 255, 255);
    print_hud_lut_string(HUD_LUT_GLOBAL, labelX, 40, currentMenu->label);
    gSPDisplayList(gDisplayListHead++, dl_rgba16_text_end);

    if (currentMenu->numOpts > 4) {
        optmenu_draw_box(272, 90, 280, 208, 0x80, 0x80, 0x80);
        scrollpos = 54 * ((f32)currentMenu->scroll / (currentMenu->numOpts-4));
        optmenu_draw_box(272, 90+scrollpos, 280, 154+scrollpos, 0xFF, 0xFF, 0xFF);
    }

    gSPDisplayList(gDisplayListHead++, dl_ia_text_begin);
    gDPSetScissor(gDisplayListHead++, G_SC_NON_INTERLACE, 0, 80, SCREEN_WIDTH, SCREEN_HEIGHT);
    
    for (u8 i = 0; i < currentMenu->numOpts; i++) {
        scroll = 140 - 32 * i + currentMenu->scroll * 32;
        // FIXME: just start from the first visible option bruh
        if (scroll <= 140 && scroll > 32)
            optmenu_draw_opt(&currentMenu->opts[i], 160, scroll, (currentMenu->select == i));
    }

    gDPSetScissor(gDisplayListHead++, G_SC_NON_INTERLACE, 0, 0, SCREEN_WIDTH, SCREEN_HEIGHT);
    gDPSetEnvColor(gDisplayListHead++, 255, 255, 255, 255);
    print_generic_string(72, 132 - (32 * (currentMenu->select - currentMenu->scroll)), optSmallStr[2]);
    print_generic_string(232, 132 - (32 * (currentMenu->select - currentMenu->scroll)), optSmallStr[3]);
    gSPDisplayList(gDisplayListHead++, dl_ia_text_end);
}

//This has been separated for interesting reasons. Don't question it.
void optmenu_draw_prompt(void) {
    gSPDisplayList(gDisplayListHead++, dl_ia_text_begin);
    optmenu_draw_text(264, 212, optSmallStr[optmenu_open], 0);
    gSPDisplayList(gDisplayListHead++, dl_ia_text_end);
}

void optmenu_toggle(void) {
    if (optmenu_open == 0) {
        #ifndef nosound
        play_sound(SOUND_MENU_CHANGE_SELECT, gDefaultSoundArgs);
        #endif

        ribbon_refresh_labels();
        mp_refresh_labels();

        currentMenu = &menuMain;
        optmenu_open = 1;
        /* Resets l_counter to 0 every time the options menu is open */
        l_counter = 0;
    } else {
        #ifndef nosound
        play_sound(SOUND_MENU_MARIO_CASTLE_WARP2, gDefaultSoundArgs);
        #endif
        optmenu_open = 0;
        #ifdef BETTERCAMERA
        newcam_init_settings(); // load bettercam settings from config vars
        #endif
        controller_reconfigure(); // rebind using new config values
        configfile_save(configfile_name());
    }
}

void optmenu_check_buttons(void) {
    if (optmenu_binding) {
        u32 key = controller_get_raw_key();
        if (key != VK_INVALID) {
            #ifndef nosound
            play_sound(SOUND_MENU_CHANGE_SELECT, gDefaultSoundArgs);
            #endif
            currentMenu->opts[currentMenu->select].uval[optmenu_bind_idx] = key;
            optmenu_binding = 0;
            optmenu_option_timer = 12;
        }
        return;
    }

    if (gPlayer1Controller->buttonPressed & R_TRIG)
        optmenu_toggle();

    /* Enables cheats if the user press the L trigger 3 times while in the options menu. Also plays a sound. */
    
    if ((gPlayer1Controller->buttonPressed & L_TRIG) && !Cheats.EnableCheats) {
        if (l_counter == 2) {
                Cheats.EnableCheats = true;
                play_sound(SOUND_MENU_STAR_SOUND, gDefaultSoundArgs);
                l_counter = 0;
        } else {
            l_counter++;
        }
    }
    
    if (!optmenu_open) return;

    u8 allowInput = 0;

    optmenu_option_timer--;
    if (optmenu_option_timer <= 0) {
        if (optmenu_hold_count == 0) {
            optmenu_hold_count++;
            optmenu_option_timer = 10;
        } else {
            optmenu_option_timer = 3;
        }
        allowInput = 1;
    }

    if (ABS(gPlayer1Controller->stickY) > 60) {
        if (allowInput) {
            #ifndef nosound
            play_sound(SOUND_MENU_CHANGE_SELECT, gDefaultSoundArgs);
            #endif

            if (gPlayer1Controller->stickY >= 60) {
                currentMenu->select--;
                if (currentMenu->select < 0)
                    currentMenu->select = currentMenu->numOpts-1;
            } else {
                currentMenu->select++;
                if (currentMenu->select >= currentMenu->numOpts)
                    currentMenu->select = 0;
            }

            if (currentMenu->select < currentMenu->scroll)
                currentMenu->scroll = currentMenu->select;
            else if (currentMenu->select > currentMenu->scroll + 3)
                currentMenu->scroll = currentMenu->select - 3;
        }
    } else if (ABS(gPlayer1Controller->stickX) > 60) {
        if (allowInput) {
            #ifndef nosound
            play_sound(SOUND_MENU_CHANGE_SELECT, gDefaultSoundArgs);
            #endif
            if (gPlayer1Controller->stickX >= 60)
                optmenu_opt_change(&currentMenu->opts[currentMenu->select], 1);
            else
                optmenu_opt_change(&currentMenu->opts[currentMenu->select], -1);
        }
    } else if (gPlayer1Controller->buttonPressed & A_BUTTON) {
        if (allowInput) {
            #ifndef nosound
            play_sound(SOUND_MENU_CHANGE_SELECT, gDefaultSoundArgs);
            #endif
            optmenu_opt_change(&currentMenu->opts[currentMenu->select], 0);
        }
    } else if (gPlayer1Controller->buttonPressed & B_BUTTON) {
        if (allowInput) {
            if (currentMenu->prev) {
                #ifndef nosound
                play_sound(SOUND_MENU_CHANGE_SELECT, gDefaultSoundArgs);
                #endif
                currentMenu = currentMenu->prev;
            } else {
                // can't go back, exit the menu altogether
                optmenu_toggle();
            }
        }
    } else if (gPlayer1Controller->buttonPressed & Z_TRIG) {
        // HACK: clear binds with Z
        if (allowInput && currentMenu->opts[currentMenu->select].type == OPT_BIND)
            optmenu_opt_change(&currentMenu->opts[currentMenu->select], 0xFF);
    } else if (gPlayer1Controller->buttonPressed & START_BUTTON) {
        if (allowInput) optmenu_toggle();
    } else {
        optmenu_hold_count = 0;
        optmenu_option_timer = 0;
    }
}

#endif // EXT_OPTIONS_MENU
