#include <stdio.h>
#include <string.h>
#include <SDL2/SDL.h>
#include <PR/ultratypes.h>
#include <PR/gbi.h>

#include "sm64.h"
#include "game_init.h"
#include "ingame_menu.h"
#include "segment2.h"
#include "chat.h"

#include "pc/net/net_transport.h"

// Chat dentro del juego: T abre la caja, Enter envia, Esc cancela.
// Las teclas del juego se ignoran mientras se escribe (ver gfx_sdl2.c).

#define CHAT_MAX_INPUT 60
#define CHAT_LINES 6
#define CHAT_SHOW_FRAMES 360

static int sOpen;
static int sSkipText; // la 't' que abre el chat tambien llega como texto: se descarta
static char sInput[CHAT_MAX_INPUT + 1];

static struct { char text[64]; int age; } sLines[CHAT_LINES];

int chat_is_open(void) {
    return sOpen;
}

void chat_add(const char *name, const char *text) {
    memmove(&sLines[0], &sLines[1], sizeof(sLines[0]) * (CHAT_LINES - 1));
    snprintf(sLines[CHAT_LINES - 1].text, sizeof(sLines[0].text), "%.12s: %.48s", name, text);
    sLines[CHAT_LINES - 1].age = 0;
}

static void close_chat(void) {
    sOpen = 0;
    SDL_StopTextInput();
}

int chat_on_keydown(int scancode) {
    if (!sOpen) {
        if (scancode == SDL_SCANCODE_T && nt_enabled()) {
            sOpen = 1;
            sInput[0] = 0;
            sSkipText = 1;
            SDL_StartTextInput();
            return 1;
        }
        return 0;
    }
    sSkipText = 0;
    if (scancode == SDL_SCANCODE_RETURN || scancode == SDL_SCANCODE_KP_ENTER) {
        if (sInput[0]) {
            nt_send_chat(sInput);
            chat_add(nt_my_name(), sInput);
        }
        close_chat();
    } else if (scancode == SDL_SCANCODE_ESCAPE) {
        close_chat();
    } else if (scancode == SDL_SCANCODE_BACKSPACE) {
        size_t n = strlen(sInput);
        if (n) sInput[n - 1] = 0;
    }
    return 1;
}

// Entrada de texto en UTF-8: se deja solo ASCII imprimible (las vocales con tilde y la n con
// tilde pasan a su letra base, porque la tipografia del juego no tiene esos caracteres).
void chat_on_text(const char *utf8) {
    if (!sOpen) return;
    if (sSkipText) { sSkipText = 0; return; }
    for (const unsigned char *p = (const unsigned char *) utf8; *p; p++) {
        char c = 0;
        if (*p == 0xC3 && p[1]) {
            p++;
            switch (*p) {
                case 0xA1: case 0x81: c = 'a'; break;
                case 0xA9: case 0x89: c = 'e'; break;
                case 0xAD: case 0x8D: c = 'i'; break;
                case 0xB3: case 0x93: c = 'o'; break;
                case 0xBA: case 0x9A: case 0xBC: c = 'u'; break;
                case 0xB1: case 0x91: c = 'n'; break;
            }
        } else if (*p >= 32 && *p < 127 && *p != '"' && *p != '\\') {
            c = (char) *p;
        }
        size_t n = strlen(sInput);
        if (c && n < CHAT_MAX_INPUT) { sInput[n] = c; sInput[n + 1] = 0; }
    }
}

static void to_dialog(const char *s, u8 *out, int cap) {
    int n = 0;
    for (; *s && n < cap - 1; s++) {
        u8 d;
        char c = *s;
        if ((c >= '0' && c <= '9') || (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z')) d = ASCII_TO_DIALOG(c);
        else switch (c) {
            case '.': d = 0x3F; break;
            case ',': d = 0x6F; break;
            case '!': d = 0xF2; break;
            case '?': d = 0xF4; break;
            case '-': d = 0x9F; break;
            case '\'': d = 0x3E; break;
            case ':': d = 0xE6; break;
            case '/': d = 0xD0; break;
            default: d = 0x9E; break; // espacio
        }
        out[n++] = d;
    }
    out[n] = DIALOG_CHAR_TERMINATOR;
}

static void draw_line(int x, int y, const char *text) {
    u8 buf[72];
    to_dialog(text, buf, sizeof(buf));
    gDPSetEnvColor(gDisplayListHead++, 0, 0, 0, 255); // sombra para leerse sobre cualquier fondo
    print_generic_string(x + 1, y - 1, buf);
    gDPSetEnvColor(gDisplayListHead++, 255, 255, 255, 255);
    print_generic_string(x, y, buf);
}

void chat_draw(void) {
    int any = sOpen;
    for (int i = 0; i < CHAT_LINES; i++) {
        if (sLines[i].text[0]) {
            if (sLines[i].age < CHAT_SHOW_FRAMES) sLines[i].age++;
            if (sLines[i].age < CHAT_SHOW_FRAMES) any = 1;
        }
    }
    if (!any) return;

    create_dl_ortho_matrix();
    gSPDisplayList(gDisplayListHead++, dl_ia_text_begin);
    int y = sOpen ? 30 : 14;
    for (int i = CHAT_LINES - 1; i >= 0; i--) {
        if (!sLines[i].text[0] || (!sOpen && sLines[i].age >= CHAT_SHOW_FRAMES)) continue;
        draw_line(10, y + 14, sLines[i].text);
        y += 14;
    }
    if (sOpen) {
        char line[CHAT_MAX_INPUT + 8];
        int n = (int) strlen(sInput);
        snprintf(line, sizeof(line), "> %s%s", n > 44 ? sInput + (n - 44) : sInput, (SDL_GetTicks() / 400) % 2 ? "-" : " ");
        draw_line(10, 14, line);
    }
    gSPDisplayList(gDisplayListHead++, dl_ia_text_end);
}
