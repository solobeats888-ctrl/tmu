# TMU — SoundFont Library

Instrumento VST3 / AU para FL Studio (Mac) que junta todos los soundfonts (.sf2) en un solo plugin
y los organiza **por tipo de sonido**, **por soundfont** o **de la A a la Z**, con buscador.

## Cómo se usa
- **POR TIPO / POR SOUNDFONT / A - Z**: cambia cómo se agrupan los sonidos (columna izquierda).
- **Buscar**: filtra por nombre del sonido, del soundfont o de la categoría (por ejemplo `bass`, `mario`, `pad`).
- **Click en un sonido** lo carga al instante. Con las flechas ↑ ↓ (o los botones `<` `>`) pasás al anterior o siguiente.
- **Volumen** y **Transporte** (±24 semitonos). Doble click los vuelve a cero.
- El sonido elegido se guarda con el proyecto de FL.
- **Agregar soundfonts propios**: tocá **Carpeta** (abre `Música/TMU SoundFonts`), copiá ahí los `.sf2` y tocá **Reescanear**.

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

## Créditos
Motor de SoundFont: [TinySoundFont](https://github.com/schellingb/TinySoundFont) (MIT). Framework: JUCE 8.
