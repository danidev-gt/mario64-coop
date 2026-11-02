#ifndef CAPPY_H
#define CAPPY_H

struct MarioState;

// Gorra lanzable (estilo Mario Odyssey): se lanza con la tecla de gorra (D-pad arriba),
// ataca enemigos, recoge monedas y vuelve a Mario.
void cappy_update(struct MarioState *m);
void bhv_cappy_loop(void);

#endif
