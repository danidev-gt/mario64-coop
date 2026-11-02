#ifndef RIBBON_H
#define RIBBON_H

#include <PR/ultratypes.h>

#define RIBBON_SLOTS 3 // mas una ranura extra (indice RIBBON_SLOTS) para deshacer la ultima carga

struct MarioState;

// Cinta de herramientas: puntos de control, ayudas para el jugador
int ribbon_slot_used(int slot);
void ribbon_save_slot(int slot);
void ribbon_load_slot(int slot);
void ribbon_give_lives(int n);
void ribbon_full_health(void);
void ribbon_give_coins(int n);
void ribbon_give_cap(u32 capFlag);

// Se llama cada frame desde execute_mario_action
void ribbon_update(struct MarioState *m);
// Devuelve 1 una vez si hay que cerrar la pausa (tras cargar un punto)
int ribbon_take_unpause(void);

#endif // RIBBON_H
