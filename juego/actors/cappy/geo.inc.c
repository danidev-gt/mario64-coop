// Igual que marios_cap_geo, mas los ojos de Cappy.
const GeoLayout cappy_geo[] = {
   GEO_SHADOW(SHADOW_CIRCLE_9_VERTS, 0xB4, 75),
   GEO_OPEN_NODE(),
      GEO_SCALE(0x00, 16384),
      GEO_OPEN_NODE(),
         GEO_ASM(10, geo_update_layer_transparency),
         GEO_SWITCH_CASE(2, geo_switch_anim_state),
         GEO_OPEN_NODE(),
            GEO_DISPLAY_LIST(LAYER_OPAQUE, mario_cap_seg3_dl_03022F48),
            GEO_DISPLAY_LIST(LAYER_TRANSPARENT, mario_cap_seg3_dl_03022F48),
         GEO_CLOSE_NODE(),
         GEO_DISPLAY_LIST(LAYER_OPAQUE, cappy_eyes_dl),
      GEO_CLOSE_NODE(),
   GEO_CLOSE_NODE(),
   GEO_END(),
};
