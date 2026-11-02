// Gorra lanzable (Cappy): ojos dibujados sobre el modelo de la gorra de Mario.
// Coordenadas en el mismo espacio que mario_cap (la geo aplica la escala 0.25).

// ojo izquierdo/derecho: blanco (4 vertices) + pupila negra (4 vertices), mirando a +Z
static const Vtx cappy_eyes_vtx[] = {
    {{{   -46,    52,   146}, 0, {0, 0}, {0xff, 0xff, 0xff, 0xff}}},
    {{{    -8,    52,   146}, 0, {0, 0}, {0xff, 0xff, 0xff, 0xff}}},
    {{{    -8,   104,   146}, 0, {0, 0}, {0xff, 0xff, 0xff, 0xff}}},
    {{{   -46,   104,   146}, 0, {0, 0}, {0xff, 0xff, 0xff, 0xff}}},
    {{{    -34,    58,   150}, 0, {0, 0}, {0x10, 0x10, 0x10, 0xff}}},
    {{{    -14,    58,   150}, 0, {0, 0}, {0x10, 0x10, 0x10, 0xff}}},
    {{{    -14,    92,   150}, 0, {0, 0}, {0x10, 0x10, 0x10, 0xff}}},
    {{{    -34,    92,   150}, 0, {0, 0}, {0x10, 0x10, 0x10, 0xff}}},
    {{{     8,    52,   146}, 0, {0, 0}, {0xff, 0xff, 0xff, 0xff}}},
    {{{    46,    52,   146}, 0, {0, 0}, {0xff, 0xff, 0xff, 0xff}}},
    {{{    46,   104,   146}, 0, {0, 0}, {0xff, 0xff, 0xff, 0xff}}},
    {{{     8,   104,   146}, 0, {0, 0}, {0xff, 0xff, 0xff, 0xff}}},
    {{{    14,    58,   150}, 0, {0, 0}, {0x10, 0x10, 0x10, 0xff}}},
    {{{    34,    58,   150}, 0, {0, 0}, {0x10, 0x10, 0x10, 0xff}}},
    {{{    34,    92,   150}, 0, {0, 0}, {0x10, 0x10, 0x10, 0xff}}},
    {{{    14,    92,   150}, 0, {0, 0}, {0x10, 0x10, 0x10, 0xff}}},
};

// sin textura ni iluminacion: colores de vertice planos
const Gfx cappy_eyes_dl[] = {
    gsDPPipeSync(),
    gsSPClearGeometryMode(G_LIGHTING | G_CULL_BACK),
    gsDPSetCombineMode(G_CC_SHADE, G_CC_SHADE),
    gsSPTexture(0xFFFF, 0xFFFF, 0, G_TX_RENDERTILE, G_OFF),
    gsSPVertex(cappy_eyes_vtx, 16, 0),
    gsSP2Triangles(0, 1, 2, 0x0, 0, 2, 3, 0x0),
    gsSP2Triangles(4, 5, 6, 0x0, 4, 6, 7, 0x0),
    gsSP2Triangles(8, 9, 10, 0x0, 8, 10, 11, 0x0),
    gsSP2Triangles(12, 13, 14, 0x0, 12, 14, 15, 0x0),
    gsDPPipeSync(),
    gsSPSetGeometryMode(G_LIGHTING | G_CULL_BACK),
    gsSPEndDisplayList(),
};
