#ifndef REMOTE_PLAYERS_H
#define REMOTE_PLAYERS_H

struct Object;

// Cooperativo en linea: jugadores remotos dibujados como objetos con el modelo de Mario.

void remote_players_init(void);   // arranca la capa de red (online.txt)
void remote_players_frame(void);  // una vez por frame de juego, antes de actualizar objetos
void remote_players_draw_names(void); // llamar justo antes de render_text_labels()
void bhv_remote_player_loop(void);
void remote_players_on_object_gone(struct Object *obj); // moneda/estrella recogida o enemigo derrotado por el Mario local
void remote_players_on_star(int courseIndex, int starIndex); // el Mario local consiguio una estrella

#endif
