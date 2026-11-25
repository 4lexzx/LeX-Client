# LeX Client

Cliente para Minecraft Bedrock en Windows: HUD, menú dentro del juego y cosméticos 3D.
No está afiliado a Mojang ni a Microsoft.

## Partes

- `client/`: DLL en C++20 que se carga en el juego. Dibuja con Dear ImGui sobre D3D11 o D3D12.
- `launcher/`: aplicación Electron para jugar, activar módulos y elegir cosméticos.

## Compilar

Requiere CMake, Ninja y MinGW-w64 (WinLibs), y Node.js 22 o posterior.

```powershell
./build.ps1
cd launcher
npm install
npm start
```

`build.ps1` compila, ejecuta las pruebas, copia la DLL a `launcher/native` y regenera `launcher/shared/modules.json`.

## Uso

Pulsa Jugar. Dentro del juego, dentro de un mundo, la tecla L abre el menú. "Editar HUD" permite arrastrar cada elemento.

## Cosméticos

Al pulsar Jugar, LeX genera sus datos a partir de tu instalación y los activa solo. La DLL repinta la textura en la GPU,
así que equipar o quitar un cosmético es inmediato. Solo tú los ves. Hay capas con física, alas, cabeza, mascotas, auras 3D y partículas al correr. `python tools/make_cosmetics.py` regenera el arte.

## Datos del juego

Coordenadas, vida, velocidad, rumbo y armadura los publica el propio juego (ver `launcher/shared/telemetry.mjs`), sin offsets de memoria.

## Lista de jugadores

Mantén TAB dentro de un mundo o servidor. LeX lee (sin modificar) la lista que el juego ya recibe: nombre y cara de la skin.
Tu logo aparece junto a ti y junto a otros LeX de tu misma red local. Reconocerlos por internet exige un servidor de presencia, que aún no existe.

## Opciones en vivo

El zoom, el FOV, el brillo y el optimizador cambian las opciones del propio juego, que LeX localiza por su forma y etiqueta.
Al apagar un módulo se restaura el valor original.

## Objetos en el suelo

La física de objetos engancha la llamada de giro del juego, localizada por su código y sus constantes. Si una versión cambia ese código, el mod no se aplica y el juego sigue igual.

## Límites conocidos

La vida de otros jugadores depende de que el servidor la envíe. El tamaño del objeto en la mano no se puede cambiar en primera persona.
Cosméticos en vivo y datos requieren D3D12.
Si un mundo o servidor reemplaza la entidad del jugador con su propio paquete, sus definiciones ganan y los cosméticos no se ven ahí.

## Privacidad

Las teclas solo se leen con el juego en primer plano y nunca se guardan. No hay telemetría.
Con la lista de jugadores activa, LeX anuncia en tu red local (UDP 19190) un resumen del XUID, nunca el nombre.
