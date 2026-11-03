# TMU — Sound Font Player

Instrumento VST3 / AU para FL Studio (Mac y Windows) que junta todos tus soundfonts (.sf2) en un solo plugin
y los organiza **por tipo de sonido**, **por soundfont** o **de la A a la Z**, con buscador.

## Cómo se usa
- **POR TIPO / POR SOUNDFONT / A - Z**: cambia cómo se agrupan los sonidos (columna izquierda).
- **Buscar**: filtra por nombre del sonido, del soundfont o de la categoría (por ejemplo `bass`, `mario`, `pad`).
- **Click en un sonido** lo carga al instante. Con las flechas ↑ ↓ (o los botones `<` `>`) pasás al anterior o siguiente.
- **VOL**, **MOD** (vibrato, igual que la rueda de modulación), **FILTRO** (centro sin filtro, izquierda corta agudos, derecha corta graves) y **ST** (transporte ±24 semitonos). Doble click los vuelve a cero.
- **RATE** (velocidad de la modulación) — **MOD** es estilo Roland: vibrato con retardo + filtro que se mueve.
- **SWING** (50–75 %, con el tempo de FL), **HUMAN** (velocity y timing humanos) y **VEL** (curva: Suave / Normal / Dura / Fija).
- **IN** y **OUT**: IN es el fundido de entrada de cada nota (0 a 1000 ms) y OUT cuánto tarda en apagarse al soltar la tecla (0 a 1000 ms). Valen como mínimo: si el soundfont ya tiene un ataque o un release más largo, queda ese. En 0 no hacen nada. Cada capa tiene los suyos.
- **Favoritos**: click en las estrellas de un sonido (1, 2 o 3), click derecho > Favoritos, o las teclas 1, 2, 3 (0 para quitar). Aparecen en el grupo **★ FAVORITOS**, ordenados por estrellas. Se guardan en `~/Library/Application Support/TMU/TMU-favoritos.xml` y valen para todos los proyectos.
- **Recientes**: los `.sf2` que agregás a la carpeta (después de tocar RESCAN) aparecen en **verde** y en el grupo **RECIENTES**, el último agregado primero. Quedan así 14 días.
- **Capas (layering)**: hasta 4 sonidos tocando a la vez con el mismo MIDI. Tocá una capa en la barra de abajo de la pantalla, elegí el sonido en la lista y listo. **Cada capa tiene todos los controles del primero** (VOL, MOD, RATE, FILTRO, ST, SWING, HUMAN, VEL, IN y OUT): las perillas de abajo manejan la capa elegida (dice CAPA 1, 2...). Al sumar una capa nueva sus perillas arrancan en 0. El cuadrito con el número la prende o apaga, la barra es su volumen (doble click = 0 dB) y click derecho te deja vaciarla. Todo se automatiza desde FL y se guarda con el proyecto.
- **SIN REPETIDOS**: oculta los sonidos que suenan igual a otro y los que no suenan (ver `REPORTE_REPETIDOS.txt`). Tocalo para verlos todos.
- **Rueda de modulación (CC1)** y **pitch bend** de tu teclado MIDI funcionan directo.
- **Click derecho en una perilla**: *Aprender controlador* (movés una perilla de tu teclado y queda asignada; se guarda con el proyecto) o el menú de FL con *Link to controller*.
  Si FL no muestra ese menú: mové la perilla y usá **Tools > Last tweaked > Link to controller** en FL.
- El sonido elegido se guarda con el proyecto de FL.
- **Agregar soundfonts propios**: tocá **Carpeta** (abre `Música/TMU SoundFonts`), copiá ahí los `.sf2` y tocá **Reescanear**.

## Si Mac bloquea el plugin o FL no lo ve
Abrí la app **Terminal**, pegá esto y apretá Enter (pide la contraseña de la Mac, no se ve al escribirla):

```
sudo xattr -cr "/Library/Audio/Plug-Ins/VST3/TMU.vst3"
sudo xattr -cr "/Library/Audio/Plug-Ins/Components/TMU.component"
sudo codesign --force --deep --sign - "/Library/Audio/Plug-Ins/VST3/TMU.vst3"
sudo codesign --force --deep --sign - "/Library/Audio/Plug-Ins/Components/TMU.component"
killall -9 AudioComponentRegistrar
```

Después reiniciá FL Studio y tocá **Options > Manage plugins > Find installed plugins**.
Si el instalador `.pkg` no abre: click derecho > Abrir, o en Terminal `xattr -cr ~/Downloads/TMU-Instalador.pkg`.
Más detalles en `LEEME.txt`.

## Cómo compilarlo (GitHub Actions)
Cada push corre el workflow **Build TMU (Mac + Windows)** (`.github/workflows/build.yml`; copia en `COPIAR_build.yml.txt`).
- Artifact **TMU-mac**: `TMU-Instalador-Mac.pkg` (VST3 + AU, Apple Silicon + Intel).
- Artifact **TMU-Windows**: `TMU-Instalador-Windows.exe` (VST3 64 bits, instalador Inno Setup en `installer/windows/TMU.iss`).

TMU no incluye soundfonts: cada usuario pone los suyos en `Música/TMU SoundFonts`.
Las pantallas del instalador de Mac están en `installer/mac-resources`.

## Repetidos
`tools/tmufp.cpp` hace sonar cada preset en 4 notas, calcula una huella del espectro y la compara entre todos.
Los grupos con similitud ≥ 0,995 (o audio idéntico) quedan en `Resources/repetidos.tsv`, que va adentro del plugin.

## Créditos
Motor de SoundFont: [TinySoundFont](https://github.com/schellingb/TinySoundFont) (MIT). Framework: JUCE 8.
Fuente: JetBrains Mono (SIL Open Font License, ver `Resources/fonts`). La firma "viszeversza" es una imagen (`Resources/viszeversa.png`) hecha con Kihim de Fontshare; el archivo de la fuente no se incluye.
