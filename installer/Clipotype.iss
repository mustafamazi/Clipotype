; Clipotype Windows installer (Inno Setup 6.3+).
;
; Build after a CMake Release build:
;   iscc installer\Clipotype.iss /DAppVersion=1.1.0
;
; Optional defines:
;   AppVersion  version shown in the installer and in Apps & features (default below)
;   BuildDir    folder that holds the JUCE artefacts (VST3\, Standalone\)
;
; Components:
;   VST3        required, installed to {commoncf64}\VST3
;   Standalone  optional, installed to {commonpf64}\BopsAudio\Clipotype with a Start menu shortcut

#ifndef AppVersion
  #define AppVersion "1.1.0"
#endif

#ifndef BuildDir
  #define BuildDir AddBackslash(SourcePath) + "..\build\Clipotype_artefacts\Release"
#endif

; On Windows, JUCE builds the VST3 as a bundle folder (Clipotype.vst3\Contents\x86_64-win\Clipotype.vst3
; plus Contents\Resources\moduleinfo.json), not as a single .vst3 file. The whole folder is installed.
#define Vst3Bundle BuildDir + "\VST3\Clipotype.vst3"
#define StandaloneExe BuildDir + "\Standalone\Clipotype.exe"

#if !DirExists(Vst3Bundle)
  #error VST3 bundle folder (VST3\Clipotype.vst3) not found in BuildDir. Build Release first, or pass /DBuildDir=...
#endif
#if !FileExists(StandaloneExe)
  #error Standalone app (Standalone\Clipotype.exe) not found in BuildDir. Build Release first, or pass /DBuildDir=...
#endif

[Setup]
; Never change AppId: it lets a newer installer upgrade the existing installation.
AppId={{05B1DB16-094E-402C-BA38-E5CCEB8D5D89}
AppName=Clipotype
AppVersion={#AppVersion}
AppVerName=Clipotype {#AppVersion}
AppPublisher=BopsAudio
AppPublisherURL=https://bopsaudio.com
AppSupportURL=https://github.com/mustafamazi/Clipotype
VersionInfoVersion={#AppVersion}
VersionInfoCompany=BopsAudio
VersionInfoProductName=Clipotype

DefaultDirName={commonpf64}\BopsAudio\Clipotype
DisableDirPage=yes
DefaultGroupName=BopsAudio
DisableProgramGroupPage=yes

LicenseFile=..\LICENSE

ArchitecturesAllowed=x64compatible
ArchitecturesInstallIn64BitMode=x64compatible
PrivilegesRequired=admin
MinVersion=10.0

WizardStyle=modern
Compression=lzma2
SolidCompression=yes
OutputDir=Output
OutputBaseFilename=Clipotype-v{#AppVersion}-Windows-Setup
UninstallDisplayName=Clipotype
CloseApplications=yes

[Types]
Name: "full";   Description: "VST3 plugin and standalone app"
Name: "custom"; Description: "Custom"; Flags: iscustom

[Components]
Name: "vst3";       Description: "VST3 plugin";    Types: full custom; Flags: fixed
Name: "standalone"; Description: "Standalone app"; Types: full

[Files]
Source: "{#Vst3Bundle}\*"; DestDir: "{commoncf64}\VST3\Clipotype.vst3"; Components: vst3; Flags: ignoreversion recursesubdirs createallsubdirs
Source: "{#StandaloneExe}"; DestDir: "{app}"; Components: standalone; Flags: ignoreversion
; AGPLv3 text, installed next to the uninstaller whichever components are chosen.
Source: "..\LICENSE"; DestDir: "{app}"; DestName: "LICENSE.txt"; Flags: ignoreversion

[Icons]
Name: "{group}\Clipotype"; Filename: "{app}\Clipotype.exe"; Components: standalone

[Run]
Filename: "{app}\Clipotype.exe"; Description: "Launch Clipotype"; Components: standalone; Flags: nowait postinstall skipifsilent unchecked

[UninstallDelete]
; Remove the whole bundle, including anything a host may have left inside it.
Type: filesandordirs; Name: "{commoncf64}\VST3\Clipotype.vst3"
