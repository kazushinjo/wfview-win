#define MyAppName "wfview"
#define MyAppVersion "2.22"
#define MyAppPublisher "wf-group"
#define MyAppURL "https://wfview.org"
#define MyAppExeName "wfview.exe"
#define MySourceDir "C:\claude\wfview\wfview-release"

[Setup]
AppId={{6A3B2F1C-4D5E-4F6A-8B9C-0D1E2F3A4B5C}
AppName={#MyAppName}
AppVersion={#MyAppVersion}
AppPublisher={#MyAppPublisher}
AppPublisherURL={#MyAppURL}
DefaultDirName={autopf}\{#MyAppName}
DefaultGroupName={#MyAppName}
AllowNoIcons=yes
OutputDir=C:\claude\wfview\installer_output
OutputBaseFilename=wfview-{#MyAppVersion}-setup
Compression=lzma2
SolidCompression=yes
WizardStyle=modern
ArchitecturesInstallIn64BitMode=x64compatible
ArchitecturesAllowed=x64compatible
UninstallDisplayIcon={app}\{#MyAppExeName}
MinVersion=10.0

[Languages]
Name: "japanese"; MessagesFile: "compiler:Languages\Japanese.isl"

[Tasks]
Name: "desktopicon"; Description: "デスクトップにショートカットを作成"; GroupDescription: "追加タスク:"; Flags: unchecked

[Files]
; Main executable
Source: "{#MySourceDir}\{#MyAppExeName}"; DestDir: "{app}"; Flags: ignoreversion

; Qt core DLLs
Source: "{#MySourceDir}\Qt6Core.dll";         DestDir: "{app}"; Flags: ignoreversion
Source: "{#MySourceDir}\Qt6Gui.dll";          DestDir: "{app}"; Flags: ignoreversion
Source: "{#MySourceDir}\Qt6Widgets.dll";      DestDir: "{app}"; Flags: ignoreversion
Source: "{#MySourceDir}\Qt6Network.dll";      DestDir: "{app}"; Flags: ignoreversion
Source: "{#MySourceDir}\Qt6Multimedia.dll";   DestDir: "{app}"; Flags: ignoreversion
Source: "{#MySourceDir}\Qt6SerialPort.dll";   DestDir: "{app}"; Flags: ignoreversion
Source: "{#MySourceDir}\Qt6WebSockets.dll";   DestDir: "{app}"; Flags: ignoreversion
Source: "{#MySourceDir}\Qt6PrintSupport.dll"; DestDir: "{app}"; Flags: ignoreversion
Source: "{#MySourceDir}\Qt6Svg.dll";          DestDir: "{app}"; Flags: ignoreversion
Source: "{#MySourceDir}\Qt6Xml.dll";          DestDir: "{app}"; Flags: ignoreversion

; Third-party DLLs
Source: "{#MySourceDir}\qcustomplot2.dll";    DestDir: "{app}"; Flags: ignoreversion
Source: "{#MySourceDir}\portaudio_x64.dll";  DestDir: "{app}"; Flags: ignoreversion
Source: "{#MySourceDir}\hidapi.dll";          DestDir: "{app}"; Flags: ignoreversion
Source: "{#MySourceDir}\opus-0.dll";          DestDir: "{app}"; Flags: ignoreversion
Source: "{#MySourceDir}\opengl32sw.dll";      DestDir: "{app}"; Flags: ignoreversion

; FFmpeg DLLs
Source: "{#MySourceDir}\avcodec-61.dll";      DestDir: "{app}"; Flags: ignoreversion
Source: "{#MySourceDir}\avformat-61.dll";     DestDir: "{app}"; Flags: ignoreversion
Source: "{#MySourceDir}\avutil-59.dll";       DestDir: "{app}"; Flags: ignoreversion
Source: "{#MySourceDir}\swresample-5.dll";    DestDir: "{app}"; Flags: ignoreversion
Source: "{#MySourceDir}\swscale-8.dll";       DestDir: "{app}"; Flags: ignoreversion

; Qt plugin subdirectories
Source: "{#MySourceDir}\platforms\*";         DestDir: "{app}\platforms";         Flags: ignoreversion recursesubdirs
Source: "{#MySourceDir}\imageformats\*";      DestDir: "{app}\imageformats";      Flags: ignoreversion recursesubdirs
Source: "{#MySourceDir}\styles\*";            DestDir: "{app}\styles";            Flags: ignoreversion recursesubdirs
Source: "{#MySourceDir}\iconengines\*";       DestDir: "{app}\iconengines";       Flags: ignoreversion recursesubdirs
Source: "{#MySourceDir}\multimedia\*";        DestDir: "{app}\multimedia";        Flags: ignoreversion recursesubdirs
Source: "{#MySourceDir}\networkinformation\*"; DestDir: "{app}\networkinformation"; Flags: ignoreversion recursesubdirs
Source: "{#MySourceDir}\tls\*";               DestDir: "{app}\tls";               Flags: ignoreversion recursesubdirs
Source: "{#MySourceDir}\generic\*";           DestDir: "{app}\generic";           Flags: ignoreversion recursesubdirs
Source: "{#MySourceDir}\translations\*";      DestDir: "{app}\translations";      Flags: ignoreversion recursesubdirs
Source: "{#MySourceDir}\rigs\*";              DestDir: "{app}\rigs";              Flags: ignoreversion recursesubdirs
Source: "{#MySourceDir}\docs\*";              DestDir: "{app}\docs";              Flags: ignoreversion recursesubdirs

[Icons]
Name: "{group}\{#MyAppName}"; Filename: "{app}\{#MyAppExeName}"
Name: "{group}\{#MyAppName} のアンインストール"; Filename: "{uninstallexe}"
Name: "{autodesktop}\{#MyAppName}"; Filename: "{app}\{#MyAppExeName}"; Tasks: desktopicon

[Run]
Filename: "{app}\{#MyAppExeName}"; Description: "{#MyAppName} を起動する"; Flags: nowait postinstall skipifsilent
