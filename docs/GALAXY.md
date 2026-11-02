# Hacia un estilo Mario Galaxy

## Enfoque tecnico
SM64 asume que "arriba" es el eje Y. Para planetas esfericos no se reescribe todo: cada planeta
define su centro y Mario trabaja en un **marco local** donde "arriba" es la direccion desde el centro.

1. **Marco local de gravedad**: cada fotograma se convierte la posicion/velocidad de Mario al marco del
   planeta (Y = arriba local), se ejecutan las acciones normales de Mario y se convierte de vuelta.
2. **Colision**: el planeta se modela como malla normal; en el marco local el suelo es "suelo" y las
   paredes son "paredes", asi que `find_floor`/`find_wall_collisions` siguen valiendo.
3. **Camara**: seguir a Mario con el arriba local y suavizar la transicion de un planeta a otro.
4. **Objetos y enemigos**: usan el mismo marco local segun el planeta mas cercano.
5. **Cooperativo**: se envia el planeta actual y la posicion local de cada jugador.

## Hitos
| Hito | Resultado verificable | Estado |
|------|-----------------------|--------|
| 0 | Gorra lanzable con ojos, saltar encima, dejarla en el sitio (base de Odyssey) | hecho |
| 1 | Planeta esferico de prueba: caminar y saltar alrededor de una esfera | pendiente |
| 2 | Varios planetas con paso de uno a otro (pozos de gravedad) | pendiente |
| 3 | Nave/zona central con portales a cada planeta | pendiente |
| 4 | Misiones y estrellas propias con guardado ampliado | pendiente |
| 5 | Planetas con historia, enemigos y musica propios | pendiente |
