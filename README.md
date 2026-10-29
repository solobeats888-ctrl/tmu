# TMU — SoundFont Library

Instrumento VST3 / AU para FL Studio (Mac) que junta todos los soundfonts (.sf2) en un solo plugin
y los organiza **por tipo de sonido**, **por soundfont** o **de la A a la Z**, con buscador.

## Cómo se usa
- **POR TIPO / POR SOUNDFONT / A - Z**: cambia cómo se agrupan los sonidos (columna izquierda).
- **Buscar**: filtra por nombre del sonido, del soundfont o de la categoría (por ejemplo `bass`, `mario`, `pad`).
- **Click en un sonido** lo carga al instante. Con las flechas ↑ ↓ (o los botones `<` `>`) pasás al anterior o siguiente.
- **VOL**, **MOD** (vibrato, igual que la rueda de modulación), **FILTRO** (centro sin filtro, izquierda corta agudos, derecha corta graves) y **ST** (transporte ±24 semitonos). Doble click los vuelve a cero.
- **Favoritos**: click en las estrellas de un sonido (1, 2 o 3), click derecho > Favoritos, o las teclas 1, 2, 3 (0 para quitar). Aparecen en el grupo **★ FAVORITOS**, ordenados por estrellas. Se guardan en `~/Library/Application Support/TMU/TMU-favoritos.xml` y valen para todos los proyectos.
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
1. Subí todo el contenido de esta carpeta a un repositorio nuevo de GitHub.
   La carpeta `.github` está oculta en el Finder: si subís por el navegador, creá el archivo a mano con
   **Add file > Create new file**, nombre `.github/workflows/build-mac.yml`, y pegá el contenido de `COPIAR_build-mac.yml.txt`.
2. En la pestaña **Actions**, abrí "Build macOS" y esperá que termine en verde (~15 min).
3. Bajá el artifact **TMU-mac**. Trae:
   - `TMU-VST3.pkg` y `TMU-AU.pkg`: las piezas del plugin para armar el instalador completo con los soundfonts.
   - `TMU-Solo-Plugin.pkg`: instala solo el plugin (sin soundfonts).

## Instalador completo
`tools/make_pkg.py` arma `TMU.pkg` (plugin + soundfonts) sin necesitar una Mac. Ver la ayuda dentro del archivo.

## Repetidos
`tools/tmufp.cpp` hace sonar cada preset en 4 notas, calcula una huella del espectro y la compara entre todos.
Los grupos con similitud ≥ 0,995 (o audio idéntico) quedan en `Resources/repetidos.tsv`, que va adentro del plugin.

## Créditos
Motor de SoundFont: [TinySoundFont](https://github.com/schellingb/TinySoundFont) (MIT). Framework: JUCE 8.
Fuente: Press Start 2P (SIL Open Font License, ver `Resources/fonts`).
