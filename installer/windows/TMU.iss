; Instalador de TMU para Windows (VST3, 64 bits). Solo el plugin: los soundfonts los pone cada uno.
#ifndef SourceDir
  #define SourceDir "..\..\build\TMU_artefacts\Release\VST3\TMU.vst3"
#endif

[Setup]
AppId={{2F7A4C1E-5B3D-4A9E-8C61-7D0000000001}
AppName=TMU - Sound Font Player
AppVersion=1.0.0
AppPublisher=Viszeversza
DefaultDirName={commoncf64}\VST3
DisableDirPage=yes
DisableProgramGroupPage=yes
DisableReadyPage=no
OutputBaseFilename=TMU-Instalador-Windows
ArchitecturesAllowed=x64
ArchitecturesInstallIn64BitMode=x64
PrivilegesRequired=admin
UninstallFilesDir={commonappdata}\Viszeversza\TMU
UninstallDisplayName=TMU - Sound Font Player (VST3)
Compression=lzma2
SolidCompression=yes
WizardStyle=modern

[Languages]
Name: "spanish"; MessagesFile: "compiler:Languages\Spanish.isl"
Name: "english"; MessagesFile: "compiler:Default.isl"

[InstallDelete]
; borra la version anterior antes de instalar la nueva (nunca quedan dos)
Type: filesandordirs; Name: "{commoncf64}\VST3\TMU.vst3"

[Files]
Source: "{#SourceDir}\*"; DestDir: "{commoncf64}\VST3\TMU.vst3"; Flags: ignoreversion recursesubdirs createallsubdirs

[Messages]
spanish.WelcomeLabel2=Esto instala TMU (VST3, 64 bits) en la carpeta de plugins de Windows:%nC:\Program Files\Common Files\VST3%n%nTMU es solo el plugin: no trae soundfonts. Despues de instalar vas a poner tus .sf2 en la carpeta Musica\TMU SoundFonts.%n%nImportante: TMU es una herramienta para reproducir soundfonts. No incluye ni vende soundfonts: la mayoria son de otras personas, pertenecen a sus autores y se descargan gratis desde sus paginas.%n%nCerra FL Studio antes de seguir.
spanish.FinishedLabel=TMU quedo instalado.%n%n1. Abri FL Studio > Options > Manage plugins > Find plugins.%n2. Carga TMU como instrumento (generador).%n3. Toca CARPETA en el plugin, copia ahi tus .sf2 y toca RESCAN.
english.WelcomeLabel2=This installs TMU (VST3, 64-bit) into the Windows VST3 folder:%nC:\Program Files\Common Files\VST3%n%nTMU is just the plugin: it doesn't include soundfonts. Put your .sf2 files in Music\TMU SoundFonts.%n%nImportant: TMU is a tool for playing soundfonts. It does not include or sell soundfonts: most belong to their authors and are free to download from their pages.%n%nClose FL Studio before continuing.
english.FinishedLabel=TMU is installed.%n%n1. Open FL Studio > Options > Manage plugins > Find plugins.%n2. Load TMU as an instrument (generator).%n3. Click CARPETA in the plugin, copy your .sf2 files there and click RESCAN.
