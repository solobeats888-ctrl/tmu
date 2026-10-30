# TMU — Sound Font Player · Cómo instalarlo

TMU es solo el plugin: no trae soundfonts. Después de instalarlo cargás los tuyos (.sf2).

## Mac (Apple Silicon e Intel)
1. Abrí **TMU-Instalador-Mac.pkg** y seguí los pasos.
   Si la Mac no te deja abrirlo: click derecho sobre el archivo → **Abrir** → **Abrir**.
2. En FL Studio: **Options → Manage plugins → Find installed plugins**.
3. Cargá **TMU** como instrumento.
4. En el plugin tocá **CARPETA**, copiá ahí tus .sf2 y tocá **RESCAN**.

Si FL no lo ve, abrí **Terminal**, pegá esto y apretá Enter (te pide la contraseña de la Mac):
```
sudo xattr -cr "/Library/Audio/Plug-Ins/VST3/TMU.vst3"
sudo xattr -cr "/Library/Audio/Plug-Ins/Components/TMU.component"
sudo codesign --force --deep --sign - "/Library/Audio/Plug-Ins/VST3/TMU.vst3"
sudo codesign --force --deep --sign - "/Library/Audio/Plug-Ins/Components/TMU.component"
killall -9 AudioComponentRegistrar
```

## Windows 10 / 11 (64 bits)
1. Cerrá FL Studio.
2. Abrí **TMU-Instalador-Windows.exe**.
   Si aparece "Windows protegió su PC": **Más información → Ejecutar de todas formas**.
3. En FL Studio: **Options → Manage plugins → Find plugins**.
4. Cargá **TMU** como instrumento.
5. En el plugin tocá **CARPETA**, copiá ahí tus .sf2 y tocá **RESCAN**.

Requisitos: FL Studio (o cualquier DAW con VST3; en Mac también AU).
