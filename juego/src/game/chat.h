#ifndef CHAT_H
#define CHAT_H

// Chat del cooperativo en linea.
int  chat_is_open(void);
int  chat_on_keydown(int sdlScancode); // 1 si la tecla fue consumida por el chat
void chat_on_text(const char *utf8);
void chat_add(const char *name, const char *text);
void chat_draw(void); // dentro de render_game, tras los nombres

#endif
