#!/usr/bin/env python3
"""Genera personajes jugables a partir del modelo de Mario (recoloreados).

Para cada personaje copia actors/mario/{model,geo}.inc.c a actors/<id>/ cambiando el prefijo
de los simbolos (mario_ -> <id>_) y los colores de los 6 grupos de luces del modelo.
Tambien genera actors/characters.h con los `extern` que necesitan los demas archivos.

Uso (desde juego/):  python tools/gen_characters.py
"""
import os
import re
import sys

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))

# Colores (R, G, B) de cada grupo de luces del modelo de Mario.
#   blue   = overol (muslos, trasero)      red   = camisa, brazos y gorra
#   white  = guantes                       brown1= zapatos
#   beige  = piel                          brown2= pelo, bigote
PALETTES = {
    'luigi': {
        'model_id': 0xF4, 'name': 'Luigi',
        'red': (0x10, 0xB4, 0x2C), 'blue': (0x22, 0x32, 0xC8), 'logo': 'L',
    },
    'wario': {
        'model_id': 0xF5, 'name': 'Wario',
        'red': (0xF4, 0xCC, 0x10), 'blue': (0x74, 0x24, 0xA8),
        'brown1': (0x30, 0x6A, 0x2A), 'beige': (0xF0, 0xB4, 0x84), 'logo': 'W', 'logo_color': (0x58, 0x1C, 0x88),
    },
    'waluigi': {
        'model_id': 0xF6, 'name': 'Waluigi',
        'red': (0x5C, 0x14, 0x98), 'blue': (0x1C, 0x1C, 0x48),
        'brown1': (0x20, 0x20, 0x20), 'brown2': (0x10, 0x10, 0x10), 'logo': 'G',
    },
}


def make_logo(letter, rgb, path):
    """Emblema de la gorra (32x32 RGBA): letra de color sobre fondo blanco, con antialiasing.
    Si el PNG ya existe (esta versionado) no hace falta Pillow."""
    if os.path.exists(path) and '--force' not in sys.argv:
        return
    from PIL import Image, ImageDraw
    k = 8
    img = Image.new('RGBA', (32 * k, 32 * k), (255, 255, 255, 255))
    d = ImageDraw.Draw(img)
    col = rgb + (255,)
    pts = lambda l: [(x * k, y * k) for x, y in l]
    if letter == 'L':
        d.polygon(pts([(9, 9), (14.5, 9), (14.5, 17.5), (24, 17.5), (24, 23), (9, 23)]), fill=col)
    elif letter == 'G':  # L invertida (emblema de Waluigi)
        d.polygon(pts([(9, 9), (24, 9), (24, 23), (18.5, 23), (18.5, 14.5), (9, 14.5)]), fill=col)
    elif letter == 'W':
        d.line(pts([(8, 9.5), (12.5, 22), (16, 13.5), (19.5, 22), (24, 9.5)]), fill=col, width=int(4.6 * k), joint='curve')
    img = img.resize((32, 32), Image.LANCZOS)
    img.save(path)


def lights_def(rgb):
    r, g, b = rgb
    return ('gdSPDefLights1(\n    0x%02x, 0x%02x, 0x%02x,\n    0x%02x, 0x%02x, 0x%02x, 0x28, 0x28, 0x28\n);'
            % (r // 2, g // 2, b // 2, r, g, b))


def rename(text, name):
    out = []
    for line in text.split('\n'):
        if line.lstrip().startswith('#include'):
            out.append(line)  # no tocar rutas de texturas (actors/mario/mario_metal...)
        else:
            out.append(re.sub(r'\bmario_', name + '_', line))
    return '\n'.join(out)


def recolor(text, name, pal):
    for group in ('blue', 'red', 'white', 'brown1', 'beige', 'brown2'):
        if group not in pal:
            continue
        pat = re.compile(r'(static const Lights1 %s_%s_lights_group = )gdSPDefLights1\([^;]*\);' % (name, group), re.S)
        text, n = pat.subn(lambda m: m.group(1) + lights_def(pal[group]), text)
        if n != 1:
            sys.exit('no se encontro el grupo de luces %s_%s' % (name, group))
    return text


def main():
    model = open(os.path.join(ROOT, 'actors/mario/model.inc.c'), encoding='utf-8', newline='').read().replace('\r\n', '\n')
    geo = open(os.path.join(ROOT, 'actors/mario/geo.inc.c'), encoding='utf-8', newline='').read().replace('\r\n', '\n')

    header = ['#ifndef CHARACTERS_H', '#define CHARACTERS_H', '', '// GENERADO por tools/gen_characters.py - no editar a mano', '']
    def_re = re.compile(r'^(?!static)(?:ALIGNED8 )?const (Gfx|Vtx|GeoLayout|u8) (\w+)\[\]', re.M)

    for name, pal in PALETTES.items():
        m = recolor(rename(model, name), name, pal)
        g = rename(geo, name)
        if 'logo' in pal:
            logo_rel = 'actors/%s/%s_logo.rgba16.png' % (name, name)
            make_logo(pal['logo'], pal.get('logo_color', pal['red']), os.path.join(ROOT, logo_rel))
            m = m.replace('#include "actors/mario/mario_logo.rgba16.inc.c"', '#include "actors/%s/%s_logo.rgba16.inc.c"' % (name, name))
        d = os.path.join(ROOT, 'actors', name)
        os.makedirs(d, exist_ok=True)
        open(os.path.join(d, 'model.inc.c'), 'w', encoding='utf-8', newline='\n').write(
            '// %s: copia recoloreada de actors/mario/model.inc.c (generado por tools/gen_characters.py)\n%s' % (pal['name'], m))
        open(os.path.join(d, 'geo.inc.c'), 'w', encoding='utf-8', newline='\n').write(
            '// %s: copia de actors/mario/geo.inc.c (generado por tools/gen_characters.py)\n%s' % (pal['name'], g))
        header.append('// ' + pal['name'])
        for src in (m, g):
            for kind, sym in def_re.findall(src):
                header.append('extern const %s %s[];' % (kind, sym))
        header.append('')

    header.append('#endif')
    open(os.path.join(ROOT, 'actors/characters.h'), 'w', encoding='utf-8', newline='\n').write('\n'.join(header) + '\n')
    print('personajes generados:', ', '.join(PALETTES))


if __name__ == '__main__':
    main()
