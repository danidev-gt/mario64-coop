# Ideas de Mario Odyssey para integrar en Mario 64 Cooperativo

Dificultad: F = facil, M = media, D = dificil. "Coop" = que hay que sincronizar en linea.

| # | Elemento de Odyssey | Como encaja en SM64 | Dif. |
|---|---------------------|---------------------|------|
| 1 | **Lanzar la gorra (Cappy)**: ataque, plataforma para saltar encima, recoger monedas | Objeto nuevo con su comportamiento y colision; boton propio (tecla Project64: `V`) | M |
| 2 | **Captura**: lanzar la gorra a un enemigo y controlarlo (Goomba, Bob-omb, Koopa, Chain Chomp, Bullet Bill) | El controlador de Mario pasa a mover el objeto; camara sigue al objeto; soltar con salto/Z | M-D |
| 3 | **Movimientos extra**: rodar, salto de gorra, salto con voltereta, zambullida | SM64 ya tiene salto largo, de pared, de poder y zambullida; se anade rodar y salto de gorra | F-M |
| 4 | **Lunas** en vez de estrellas (varias por reino, sin salir del nivel al cogerlas) | Ampliar el guardado (hoy 7 bits por nivel) y cambiar el comportamiento de la estrella | F-M |
| 5 | **Reinos + nave** (nivel central desde el que se viaja) | Nivel nuevo "nave" con portales a cada reino/planeta | M |
| 6 | **Monedas regionales** (moradas) y **tienda** con trajes | Contador por nivel en el guardado + menu; trajes = sistema de personajes que ya tenemos | M |
| 7 | **Tramos 2D** dentro de paredes | Bloquear un eje de movimiento y fijar la camara lateral en zonas marcadas | M |
| 8 | **Vida en 3 corazones** | La vida de SM64 son 8 trozos; solo cambia el HUD y los danos | F |
| 9 | **Banderas de viaje** (puntos de reaparicion/teletransporte) | Objeto con guardado del punto y menu de viaje | F |
| 10 | **Camara** mejorada y panoramica | BETTERCAMERA ya esta activo; falta afinar | M |
| 11 | **Cooperativo estilo Odyssey**: el 2o jugador es la gorra, o se pueden capturar entre si | Necesita sincronizar capturas y la gorra por la red | D |
| 12 | **Gravedad esférica** (de Galaxy) | Ver conversacion: rotar el marco de referencia por planeta | D |

## Orden propuesto
1. Gorra lanzable (1) y movimientos extra (3).
2. Captura de un solo enemigo, el Goomba (2), para validar el sistema.
3. Lunas y guardado ampliado (4), vida en corazones (8), banderas (9).
4. Nave central y primer reino nuevo (5).
5. Tramos 2D (7), tienda (6), mas capturas.
6. Cooperativo de gorra y capturas (11) y planetas esfericos (12).

## Reglas de trabajo
- El juego original (castillo y los 15 niveles) no se modifica: todo lo nuevo es opcional y va aparte.
- Se compila en `juego/build_dev` y se prueba con una **copia** del guardado, para no tocar nunca el progreso real en `juego/build/us_pc`.
