# Cosméticos animados de LEX CLIENT

## Formato LEXTEX1

Todos los cosméticos animados que consume el overlay usan un único formato binario propio:

```
"LEXTEX1\0"         8 bytes  — magic
int32 LE width     4 bytes  — ancho de un frame
int32 LE height    4 bytes  — alto de un frame
int32 LE frames    4 bytes  — número de frames
int32 LE frameMs   4 bytes  — duración de cada frame
uint8[w*h*frames*4]         — BGRA premultiplicado, top-down
```

El overlay (`native/overlay-engine/src/engine.cpp`, `ensureCosmeticTexture`) reserva un DIB del
atlas completo (`height * frames`) y hace `blitAtlasFrame` con desplazamiento de frame.

## Crear cosméticos nuevos

1. Coloca los frames PNG en una carpeta, numerados `cape1.png`, `cape2.png`, …
2. Añade una entrada en `SETS` dentro de `scripts/import-mcpack-cosmetics.mjs`:
   `id`, `name`, `folder`, `fps`, `w`, `h`.
3. Ejecuta:
   ```powershell
   node scripts/import-mcpack-cosmetics.mjs "<carpeta con subpacks>"
   ```
4. El script genera:
   - `cosmetics/assets/<id>.lextex`
   - `cosmetics/assets/<id>.png` (atlas de preview)
   - `cosmetics/src/imported.ts`
   - `apps/launcher/modules/cosmetics.generated.mjs`
5. Compila con `npm run build`. El prebuild copia los assets a `apps/launcher/assets/cosmetics`
   y `native/overlay-engine/assets/cosmetics`.

## Pack importado

`animated_cosmetics_by_ambient_1.2.0_1612244287.mcpack` (Animated Cosmetics v1.2.0).

- Autor del pack: `ambient#2309`.
- Capas: `cBionicBen`, Lunar Client, Badlion Client.
- Armaduras: `JabaPacks`, `Alexuz`.
- Uso local en LEX CLIENT; no redistribuir públicamente sin permiso.

Conjuntos importados: Creeper (12 @ 8 fps), Lunar Chroma (14 @ 10 fps), Portal (30 @ 20 fps),
Speed (17 @ 12 fps), TNT (47 @ 8 fps) y Totem (15 @ 15 fps).

## Geometría 3D real (resource pack)

El `.mcpack` original se conserva en `apps/launcher/assets/cosmetics/animated-cosmetics.mcpack`.
El botón **GEOMETRÍA 3D · ABRIR MCPACK** en la galería lo abre con Minecraft; el juego aplica
entonces los `models/entity/*.geo.json`, `animations/*.json` y `render_controllers/*.json` al
esqueleto del jugador. Así las capas/alas van **pegadas al personaje**, no como overlay de pantalla.

Esto es complementario: los atlas LEXTEX1 siguen sirviendo para la vista previa del launcher y para
el HUD del overlay. Para crear más cosméticos 3D, duplica una subcarpeta de `subpacks/`, cambia la
textura y el render controller, y reempaqueta como `.mcpack` (mismo `manifest.json`).

## Logotipo

El launcher permite importar el PNG oficial desde Ajustes → Apariencia → **Importar logotipo PNG**.
Se guarda en `%APPDATA%\lex-client\lex-logo.png` y se genera `lex-logo.bgra` para que el overlay F8
lo dibuje tal cual sobre un recuadro negro sólido. No se redibuja ni se recolorea.

## Limitación actual

El overlay dibuja estos atlas como un efecto **client-side sobre la ventana del juego**, no como un
modelo 3D enlazado al esqueleto del jugador. El pack original usa geometría/animaciones de Bedrock
(`models/entity/*.geo.json`, `animations/*.json`, `render_controllers/*.json`); para pegar la capa o
las alas literalmente al personaje haría falta un adaptador de render en el puente inyectado.
Los atlas importados son la base reutilizable para ese futuro adaptador.
