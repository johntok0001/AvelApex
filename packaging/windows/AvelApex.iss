; Inno Setup script for Avel Apex (Windows, VST3).
; Build:  iscc /DAppVersion=1.0.0 packaging\windows\AvelApex.iss
; Expects the plugin at build\AvelApex_artefacts\Release\VST3\Avel Apex.vst3

#ifndef AppVersion
  #define AppVersion "1.0.0"
#endif

#define Root "..\.."

[Setup]
AppId={{9B6F2C4E-3D1A-4F7B-A2E5-7C1D8E0F4A63}
AppName=Avel Apex
AppVersion={#AppVersion}
AppVerName=Avel Apex {#AppVersion}
AppPublisher=Avel
DefaultDirName={commoncf64}\VST3\Avel Apex.vst3
DisableDirPage=yes
DisableProgramGroupPage=yes
DisableReadyPage=no
UninstallFilesDir={autopf}\Avel\Avel Apex
UninstallDisplayName=Avel Apex {#AppVersion} (VST3)
PrivilegesRequired=admin
ArchitecturesAllowed=x64compatible
ArchitecturesInstallIn64BitMode=x64compatible
OutputDir={#Root}\dist
OutputBaseFilename=AvelApex-{#AppVersion}-Windows-Setup
Compression=lzma2
SolidCompression=yes
WizardStyle=modern
CloseApplications=no

[Languages]
Name: "en"; MessagesFile: "compiler:Default.isl"
Name: "ru"; MessagesFile: "compiler:Languages\Russian.isl"

[Messages]
en.WelcomeLabel2=This will install Avel Apex {#AppVersion} (VST3) into%n%nC:\Program Files\Common Files\VST3%n%nClose your DAW before continuing.
ru.WelcomeLabel2=Будет установлен Avel Apex {#AppVersion} (VST3) в папку%n%nC:\Program Files\Common Files\VST3%n%nПеред продолжением закройте DAW.
en.FinishedLabel=Avel Apex is installed. Start your DAW and rescan plugins (REAPER: Preferences > Plug-ins > VST > Re-scan).
ru.FinishedLabel=Avel Apex установлен. Запустите DAW и пересканируйте плагины (REAPER: Preferences > Plug-ins > VST > Re-scan).

[InstallDelete]
; remove files of an older version inside the bundle
Type: filesandordirs; Name: "{commoncf64}\VST3\Avel Apex.vst3"

[Files]
Source: "{#Root}\build\AvelApex_artefacts\Release\VST3\Avel Apex.vst3\*"; DestDir: "{app}"; Flags: ignoreversion recursesubdirs createallsubdirs
Source: "{#Root}\README.md"; DestDir: "{autopf}\Avel\Avel Apex"; DestName: "README.txt"; Flags: ignoreversion

[UninstallDelete]
Type: filesandordirs; Name: "{commoncf64}\VST3\Avel Apex.vst3"
