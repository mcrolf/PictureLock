; Inno Setup script for PictureLock. Build with:
;   iscc /DAppVersion=0.1.0 packaging\windows\PictureLock.iss
; Expects a Release build in build\ and writes dist\PictureLock-<version>-Windows-Setup.exe.

#ifndef AppVersion
  #define AppVersion "0.0.0"
#endif

#define BuildDir "..\..\build\PictureLock_artefacts\Release"

[Setup]
AppId={{6F0D3B52-8C41-4E7A-9B1D-5A2C7E91F3D4}
AppName=PictureLock
AppVersion={#AppVersion}
AppPublisher=PictureLock
DefaultDirName={autopf}\PictureLock
DefaultGroupName=PictureLock
LicenseFile=..\..\LICENSE
OutputDir=..\..\dist
OutputBaseFilename=PictureLock-{#AppVersion}-Windows-Setup
ArchitecturesAllowed=x64compatible
ArchitecturesInstallIn64BitMode=x64compatible
PrivilegesRequired=admin
Compression=lzma2
SolidCompression=yes
WizardStyle=modern
DisableProgramGroupPage=yes
UninstallDisplayName=PictureLock

[Components]
Name: "vst3"; Description: "VST3 plugin"; Types: full compact custom; Flags: fixed
Name: "standalone"; Description: "Standalone app"; Types: full

[Files]
Source: "{#BuildDir}\VST3\PictureLock.vst3\*"; DestDir: "{commoncf64}\VST3\PictureLock.vst3"; Components: vst3; Flags: ignoreversion recursesubdirs createallsubdirs
Source: "{#BuildDir}\Standalone\PictureLock.exe"; DestDir: "{app}"; Components: standalone; Flags: ignoreversion
Source: "..\..\LICENSE"; DestDir: "{app}"; Flags: ignoreversion

[Icons]
Name: "{group}\PictureLock"; Filename: "{app}\PictureLock.exe"; Components: standalone

[Types]
Name: "full"; Description: "VST3 plugin and standalone app"
Name: "compact"; Description: "VST3 plugin only"
Name: "custom"; Description: "Custom"; Flags: iscustom
