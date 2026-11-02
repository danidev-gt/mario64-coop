#include <string.h>
#include <PR/ultratypes.h>
#include <PR/gbi.h>

#include "sm64.h"
#include "game_init.h"
#include "area.h"
#include "ingame_menu.h"
#include "segment2.h"
#include "guide.h"

// Guia de misiones de la pantalla de inicio: una pagina por nivel, rota sola.
// Texto sin tildes (la tipografia del juego no las tiene). Maximo ~44 caracteres por linea.
// "rojas" = 8 monedas rojas.
struct GuidePage { const char *title, *l1, *l2; };

static const struct GuidePage sPages[] = {
    { "GUIA DE MISIONES", "Salta a un cuadro del castillo y elige", "una estrella. 6 por nivel + 100 monedas." },
    { "1 Campo de batalla Bob-omb", "1 Vence al Gran Bob-omb  2 Gana a Koopa", "3 Canon a la isla 4 Rojas 5 Alas 6 Chomp" },
    { "2 Fortaleza de Whomp", "1 Vence a Whomp  2 Sube a la cima", "3 Canon  4 Rojas isla  5 Jaula  6 Pared" },
    { "3 Bahia del Pirata", "1 Barco hundido  2 Anguila  3 Cofres", "4 Rojas barco  5 Pilar  6 Corriente" },
    { "4 Monte Frio Frio", "1 Toboganes  2 Pinguino perdido  3 Carrera", "4 Rojas  5 Cabeza del muneco  6 Paredes" },
    { "5 Mansion de Big Boo", "1 Caza 5 Boos  2 Carrusel  3 Libros", "4 Rojas  5 Balcon  6 Sala secreta" },
    { "6 Cueva Laberinto", "1 Bestia nadadora  2 Rojas en elevador", "3 Mario Metal  4 Laberinto  5 Salida  6 Rocas" },
    { "7 Tierra de Lava", "1 Gran Maton  2 Matones  3 Puzzle", "4 Tronco  5 Corre al volcan  6 Ascensor" },
    { "8 Tierra de Arena", "1 Pajaro  2 Cima piramide  3 Dentro", "4 Pilares  5 Rojas volando  6 Puzzle" },
    { "9 Muelles del Terror", "1 Sub de Bowser  2 Cofres  3 Rojas postes", "4 Corriente  5 Mantarraya  6 Las gorras" },
    { "10 Tierra del Muneco", "1 Gran cabeza  2 Maton  3 Congelador", "4 Remolino  5 Rojas caparazon  6 Iglu" },
    { "11 Mundo Seco y Humedo", "1 Elevadores  2 Torre  3 Secretos", "4 Ascensor expres  5 Rojas  6 Carrera" },
    { "12 Monte Alto Alto", "1 Escala  2 Jaula del mono  3 Rojas hongos", "4 Ladera  5 Puente  6 Hongo solitario" },
    { "13 Isla Enorme y Diminuta", "1 Planta pirana  2 Cima  3 Revancha Koopa", "4 Cinco secretos  5 Rojas  6 Wiggler" },
    { "14 Reloj Tic Tac", "1 Jaula  2 Pendulos  3 Mano del reloj", "4 Thwomp  5 Saltos en barras  6 Tiempo" },
    { "15 Camino Arcoiris", "1 Crucero  2 Casona  3 Monedas en laberinto", "4 Columpio  5 Triangulos  6 Arcoiris" },
    { "NIVELES SECRETOS", "Tobogan: llega abajo en menos de 21 s", "Acuario: 8 rojas. Bowser: lanzalo a bombas" },
    { "INTERRUPTORES DE GORRA", "Pisa el interruptor rojo, verde o azul", "y rompe los bloques de ese color" },
    { "COOPERATIVO", "T abre el chat. Pisa la cabeza de un amigo", "para rebotar. Las estrellas se comparten" },
};

#define NUM_PAGES ((int) (sizeof(sPages) / sizeof(sPages[0])))
#define PAGE_FRAMES 180 // ~6 s a 30 fps

static void to_dialog(const char *s, u8 *out, int cap) {
    int n = 0;
    for (; *s && n < cap - 1; s++) {
        char c = *s;
        u8 d;
        if ((c >= '0' && c <= '9') || (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z')) d = ASCII_TO_DIALOG(c);
        else switch (c) {
            case '.': d = 0x3F; break;
            case ',': d = 0x6F; break;
            case '!': d = 0xF2; break;
            case '?': d = 0xF4; break;
            case '-': d = 0x9F; break;
            case '+': d = 0xFD; break;
            case ':': d = 0xE6; break;
            default: d = 0x9E; break;
        }
        out[n++] = d;
    }
    out[n] = DIALOG_CHAR_TERMINATOR;
}

static void draw_centered(int y, const char *text) {
    u8 buf[64];
    int x = 160 - (int) strlen(text) * 3;
    if (x < 4) x = 4;
    to_dialog(text, buf, sizeof(buf));
    gDPSetEnvColor(gDisplayListHead++, 0, 0, 0, 255);
    print_generic_string(x + 1, y - 1, buf);
    gDPSetEnvColor(gDisplayListHead++, 255, 255, 255, 255);
    print_generic_string(x, y, buf);
}

void guide_draw(void) {
    static int sFrame;
    if (gCurrLevelNum != 1 && gCurrDemoInput == NULL) return; // nivel 1 = pantalla de inicio (cabeza de Mario); tambien demos
    const struct GuidePage *p = &sPages[(sFrame++ / PAGE_FRAMES) % NUM_PAGES];

    create_dl_ortho_matrix();
    gSPDisplayList(gDisplayListHead++, dl_ia_text_begin);
    draw_centered(222, p->title);
    draw_centered(207, p->l1);
    draw_centered(192, p->l2);
    gSPDisplayList(gDisplayListHead++, dl_ia_text_end);
}
