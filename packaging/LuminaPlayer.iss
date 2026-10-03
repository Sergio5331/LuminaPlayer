#ifndef PayloadDir
  #error Falta PayloadDir
#endif
#ifndef OutputPath
  #error Falta OutputPath
#endif
#define AppVersion "0.9.0"

[Setup]
AppId={{E6C78D4A-2017-4C77-90D1-ECC4E634FF40}
AppName=LuminaPlayer
AppVersion={#AppVersion}
AppVerName=LuminaPlayer {#AppVersion} Beta
AppPublisher=Sergio
AppPublisherURL=https://github.com/Sergio5331
AppSupportURL=https://github.com/Sergio5331/LuminaPlayer/issues
AppUpdatesURL=https://github.com/Sergio5331/LuminaPlayer/releases
DefaultDirName={localappdata}\Programs\LuminaPlayer
DefaultGroupName=LuminaPlayer
PrivilegesRequired=lowest
ArchitecturesAllowed=x64compatible
ArchitecturesInstallIn64BitMode=x64compatible
MinVersion=10.0
OutputDir={#OutputPath}
OutputBaseFilename=LuminaPlayer-0.9.0-Beta-Setup-x64
SetupIconFile=..\assets\lumina.ico
UninstallDisplayIcon={app}\LuminaPlayer.exe
UninstallDisplayName=LuminaPlayer
Compression=lzma2/max
SolidCompression=yes
WizardStyle=modern dark
WizardSizePercent=110
DisableWelcomePage=no
DisableProgramGroupPage=yes
CloseApplications=yes
CloseApplicationsFilter=LuminaPlayer.exe,LuminaThumbnail.exe
RestartApplications=no
ChangesAssociations=yes
InfoBeforeFile={#PayloadDir}\AVISOS.txt
VersionInfoVersion=0.9.0.0
VersionInfoDescription=Instalador de LuminaPlayer para Windows x64
VersionInfoProductName=LuminaPlayer

[Languages]
Name: "spanish"; MessagesFile: "compiler:Languages\Spanish.isl"

[Tasks]
Name: "desktopicon"; Description: "Crear un acceso directo en el escritorio"; GroupDescription: "Accesos directos:"; Flags: unchecked
Name: "fileassociation"; Description: "Mostrar LuminaPlayer en Abrir con para vídeo y audio"; GroupDescription: "Integración con Windows:"

[Files]
Source: "{#PayloadDir}\*"; DestDir: "{app}"; Flags: ignoreversion recursesubdirs createallsubdirs

[Icons]
Name: "{userprograms}\LuminaPlayer"; Filename: "{app}\LuminaPlayer.exe"; WorkingDir: "{app}"
Name: "{userdesktop}\LuminaPlayer"; Filename: "{app}\LuminaPlayer.exe"; WorkingDir: "{app}"; Tasks: desktopicon

[Registry]
Root: HKCU; Subkey: "Software\Classes\LuminaPlayer.Media"; ValueType: string; ValueData: "Archivo multimedia de LuminaPlayer"; Flags: uninsdeletekey; Tasks: fileassociation
Root: HKCU; Subkey: "Software\Classes\LuminaPlayer.Media\DefaultIcon"; ValueType: string; ValueData: """{app}\LuminaPlayer.exe"",0"; Tasks: fileassociation
Root: HKCU; Subkey: "Software\Classes\LuminaPlayer.Media\shell\open\command"; ValueType: string; ValueData: """{app}\LuminaPlayer.exe"" ""%1"""; Tasks: fileassociation
Root: HKCU; Subkey: "Software\Classes\Applications\LuminaPlayer.exe"; ValueType: string; ValueName: "FriendlyAppName"; ValueData: "LuminaPlayer"; Flags: uninsdeletekey; Tasks: fileassociation
Root: HKCU; Subkey: "Software\Classes\Applications\LuminaPlayer.exe\shell\open\command"; ValueType: string; ValueData: """{app}\LuminaPlayer.exe"" ""%1"""; Tasks: fileassociation
Root: HKCU; Subkey: "Software\LuminaPlayer\Capabilities"; ValueType: string; ValueName: "ApplicationName"; ValueData: "LuminaPlayer"; Flags: uninsdeletekey; Tasks: fileassociation
Root: HKCU; Subkey: "Software\LuminaPlayer\Capabilities"; ValueType: string; ValueName: "ApplicationDescription"; ValueData: "Reproductor de vídeo y audio"; Tasks: fileassociation
Root: HKCU; Subkey: "Software\LuminaPlayer\Capabilities\FileAssociations"; ValueType: string; ValueName: ".mp4"; ValueData: "LuminaPlayer.Media"; Flags: uninsdeletekey; Tasks: fileassociation
Root: HKCU; Subkey: "Software\RegisteredApplications"; ValueType: string; ValueName: "LuminaPlayer"; ValueData: "Software\LuminaPlayer\Capabilities"; Flags: uninsdeletevalue; Tasks: fileassociation
Root: HKCU; Subkey: "Software\Classes\Applications\LuminaPlayer.exe\SupportedTypes"; ValueType: string; ValueData: ""; ValueName: ".mp4"; Tasks: fileassociation
Root: HKCU; Subkey: "Software\Classes\.mp4\OpenWithProgids"; ValueType: string; ValueData: ""; ValueName: "LuminaPlayer.Media"; Flags: uninsdeletevalue; Tasks: fileassociation
Root: HKCU; Subkey: "Software\LuminaPlayer\Capabilities\FileAssociations"; ValueType: string; ValueName: ".mkv"; ValueData: "LuminaPlayer.Media"; Tasks: fileassociation
Root: HKCU; Subkey: "Software\Classes\Applications\LuminaPlayer.exe\SupportedTypes"; ValueType: string; ValueData: ""; ValueName: ".mkv"; Tasks: fileassociation
Root: HKCU; Subkey: "Software\Classes\.mkv\OpenWithProgids"; ValueType: string; ValueData: ""; ValueName: "LuminaPlayer.Media"; Flags: uninsdeletevalue; Tasks: fileassociation
Root: HKCU; Subkey: "Software\LuminaPlayer\Capabilities\FileAssociations"; ValueType: string; ValueName: ".avi"; ValueData: "LuminaPlayer.Media"; Tasks: fileassociation
Root: HKCU; Subkey: "Software\Classes\Applications\LuminaPlayer.exe\SupportedTypes"; ValueType: string; ValueData: ""; ValueName: ".avi"; Tasks: fileassociation
Root: HKCU; Subkey: "Software\Classes\.avi\OpenWithProgids"; ValueType: string; ValueData: ""; ValueName: "LuminaPlayer.Media"; Flags: uninsdeletevalue; Tasks: fileassociation
Root: HKCU; Subkey: "Software\LuminaPlayer\Capabilities\FileAssociations"; ValueType: string; ValueName: ".mov"; ValueData: "LuminaPlayer.Media"; Tasks: fileassociation
Root: HKCU; Subkey: "Software\Classes\Applications\LuminaPlayer.exe\SupportedTypes"; ValueType: string; ValueData: ""; ValueName: ".mov"; Tasks: fileassociation
Root: HKCU; Subkey: "Software\Classes\.mov\OpenWithProgids"; ValueType: string; ValueData: ""; ValueName: "LuminaPlayer.Media"; Flags: uninsdeletevalue; Tasks: fileassociation
Root: HKCU; Subkey: "Software\LuminaPlayer\Capabilities\FileAssociations"; ValueType: string; ValueName: ".webm"; ValueData: "LuminaPlayer.Media"; Tasks: fileassociation
Root: HKCU; Subkey: "Software\Classes\Applications\LuminaPlayer.exe\SupportedTypes"; ValueType: string; ValueData: ""; ValueName: ".webm"; Tasks: fileassociation
Root: HKCU; Subkey: "Software\Classes\.webm\OpenWithProgids"; ValueType: string; ValueData: ""; ValueName: "LuminaPlayer.Media"; Flags: uninsdeletevalue; Tasks: fileassociation
Root: HKCU; Subkey: "Software\LuminaPlayer\Capabilities\FileAssociations"; ValueType: string; ValueName: ".wmv"; ValueData: "LuminaPlayer.Media"; Tasks: fileassociation
Root: HKCU; Subkey: "Software\Classes\Applications\LuminaPlayer.exe\SupportedTypes"; ValueType: string; ValueData: ""; ValueName: ".wmv"; Tasks: fileassociation
Root: HKCU; Subkey: "Software\Classes\.wmv\OpenWithProgids"; ValueType: string; ValueData: ""; ValueName: "LuminaPlayer.Media"; Flags: uninsdeletevalue; Tasks: fileassociation
Root: HKCU; Subkey: "Software\LuminaPlayer\Capabilities\FileAssociations"; ValueType: string; ValueName: ".m4v"; ValueData: "LuminaPlayer.Media"; Tasks: fileassociation
Root: HKCU; Subkey: "Software\Classes\Applications\LuminaPlayer.exe\SupportedTypes"; ValueType: string; ValueData: ""; ValueName: ".m4v"; Tasks: fileassociation
Root: HKCU; Subkey: "Software\Classes\.m4v\OpenWithProgids"; ValueType: string; ValueData: ""; ValueName: "LuminaPlayer.Media"; Flags: uninsdeletevalue; Tasks: fileassociation
Root: HKCU; Subkey: "Software\LuminaPlayer\Capabilities\FileAssociations"; ValueType: string; ValueName: ".mpg"; ValueData: "LuminaPlayer.Media"; Tasks: fileassociation
Root: HKCU; Subkey: "Software\Classes\Applications\LuminaPlayer.exe\SupportedTypes"; ValueType: string; ValueData: ""; ValueName: ".mpg"; Tasks: fileassociation
Root: HKCU; Subkey: "Software\Classes\.mpg\OpenWithProgids"; ValueType: string; ValueData: ""; ValueName: "LuminaPlayer.Media"; Flags: uninsdeletevalue; Tasks: fileassociation
Root: HKCU; Subkey: "Software\LuminaPlayer\Capabilities\FileAssociations"; ValueType: string; ValueName: ".mpeg"; ValueData: "LuminaPlayer.Media"; Tasks: fileassociation
Root: HKCU; Subkey: "Software\Classes\Applications\LuminaPlayer.exe\SupportedTypes"; ValueType: string; ValueData: ""; ValueName: ".mpeg"; Tasks: fileassociation
Root: HKCU; Subkey: "Software\Classes\.mpeg\OpenWithProgids"; ValueType: string; ValueData: ""; ValueName: "LuminaPlayer.Media"; Flags: uninsdeletevalue; Tasks: fileassociation
Root: HKCU; Subkey: "Software\LuminaPlayer\Capabilities\FileAssociations"; ValueType: string; ValueName: ".ts"; ValueData: "LuminaPlayer.Media"; Tasks: fileassociation
Root: HKCU; Subkey: "Software\Classes\Applications\LuminaPlayer.exe\SupportedTypes"; ValueType: string; ValueData: ""; ValueName: ".ts"; Tasks: fileassociation
Root: HKCU; Subkey: "Software\Classes\.ts\OpenWithProgids"; ValueType: string; ValueData: ""; ValueName: "LuminaPlayer.Media"; Flags: uninsdeletevalue; Tasks: fileassociation
Root: HKCU; Subkey: "Software\LuminaPlayer\Capabilities\FileAssociations"; ValueType: string; ValueName: ".m2ts"; ValueData: "LuminaPlayer.Media"; Tasks: fileassociation
Root: HKCU; Subkey: "Software\Classes\Applications\LuminaPlayer.exe\SupportedTypes"; ValueType: string; ValueData: ""; ValueName: ".m2ts"; Tasks: fileassociation
Root: HKCU; Subkey: "Software\Classes\.m2ts\OpenWithProgids"; ValueType: string; ValueData: ""; ValueName: "LuminaPlayer.Media"; Flags: uninsdeletevalue; Tasks: fileassociation
Root: HKCU; Subkey: "Software\LuminaPlayer\Capabilities\FileAssociations"; ValueType: string; ValueName: ".flv"; ValueData: "LuminaPlayer.Media"; Tasks: fileassociation
Root: HKCU; Subkey: "Software\Classes\Applications\LuminaPlayer.exe\SupportedTypes"; ValueType: string; ValueData: ""; ValueName: ".flv"; Tasks: fileassociation
Root: HKCU; Subkey: "Software\Classes\.flv\OpenWithProgids"; ValueType: string; ValueData: ""; ValueName: "LuminaPlayer.Media"; Flags: uninsdeletevalue; Tasks: fileassociation
Root: HKCU; Subkey: "Software\LuminaPlayer\Capabilities\FileAssociations"; ValueType: string; ValueName: ".ogv"; ValueData: "LuminaPlayer.Media"; Tasks: fileassociation
Root: HKCU; Subkey: "Software\Classes\Applications\LuminaPlayer.exe\SupportedTypes"; ValueType: string; ValueData: ""; ValueName: ".ogv"; Tasks: fileassociation
Root: HKCU; Subkey: "Software\Classes\.ogv\OpenWithProgids"; ValueType: string; ValueData: ""; ValueName: "LuminaPlayer.Media"; Flags: uninsdeletevalue; Tasks: fileassociation
Root: HKCU; Subkey: "Software\LuminaPlayer\Capabilities\FileAssociations"; ValueType: string; ValueName: ".vob"; ValueData: "LuminaPlayer.Media"; Tasks: fileassociation
Root: HKCU; Subkey: "Software\Classes\Applications\LuminaPlayer.exe\SupportedTypes"; ValueType: string; ValueData: ""; ValueName: ".vob"; Tasks: fileassociation
Root: HKCU; Subkey: "Software\Classes\.vob\OpenWithProgids"; ValueType: string; ValueData: ""; ValueName: "LuminaPlayer.Media"; Flags: uninsdeletevalue; Tasks: fileassociation
Root: HKCU; Subkey: "Software\LuminaPlayer\Capabilities\FileAssociations"; ValueType: string; ValueName: ".3gp"; ValueData: "LuminaPlayer.Media"; Tasks: fileassociation
Root: HKCU; Subkey: "Software\Classes\Applications\LuminaPlayer.exe\SupportedTypes"; ValueType: string; ValueData: ""; ValueName: ".3gp"; Tasks: fileassociation
Root: HKCU; Subkey: "Software\Classes\.3gp\OpenWithProgids"; ValueType: string; ValueData: ""; ValueName: "LuminaPlayer.Media"; Flags: uninsdeletevalue; Tasks: fileassociation
Root: HKCU; Subkey: "Software\LuminaPlayer\Capabilities\FileAssociations"; ValueType: string; ValueName: ".mp3"; ValueData: "LuminaPlayer.Media"; Tasks: fileassociation
Root: HKCU; Subkey: "Software\Classes\Applications\LuminaPlayer.exe\SupportedTypes"; ValueType: string; ValueData: ""; ValueName: ".mp3"; Tasks: fileassociation
Root: HKCU; Subkey: "Software\Classes\.mp3\OpenWithProgids"; ValueType: string; ValueData: ""; ValueName: "LuminaPlayer.Media"; Flags: uninsdeletevalue; Tasks: fileassociation
Root: HKCU; Subkey: "Software\LuminaPlayer\Capabilities\FileAssociations"; ValueType: string; ValueName: ".flac"; ValueData: "LuminaPlayer.Media"; Tasks: fileassociation
Root: HKCU; Subkey: "Software\Classes\Applications\LuminaPlayer.exe\SupportedTypes"; ValueType: string; ValueData: ""; ValueName: ".flac"; Tasks: fileassociation
Root: HKCU; Subkey: "Software\Classes\.flac\OpenWithProgids"; ValueType: string; ValueData: ""; ValueName: "LuminaPlayer.Media"; Flags: uninsdeletevalue; Tasks: fileassociation
Root: HKCU; Subkey: "Software\LuminaPlayer\Capabilities\FileAssociations"; ValueType: string; ValueName: ".wav"; ValueData: "LuminaPlayer.Media"; Tasks: fileassociation
Root: HKCU; Subkey: "Software\Classes\Applications\LuminaPlayer.exe\SupportedTypes"; ValueType: string; ValueData: ""; ValueName: ".wav"; Tasks: fileassociation
Root: HKCU; Subkey: "Software\Classes\.wav\OpenWithProgids"; ValueType: string; ValueData: ""; ValueName: "LuminaPlayer.Media"; Flags: uninsdeletevalue; Tasks: fileassociation
Root: HKCU; Subkey: "Software\LuminaPlayer\Capabilities\FileAssociations"; ValueType: string; ValueName: ".m4a"; ValueData: "LuminaPlayer.Media"; Tasks: fileassociation
Root: HKCU; Subkey: "Software\Classes\Applications\LuminaPlayer.exe\SupportedTypes"; ValueType: string; ValueData: ""; ValueName: ".m4a"; Tasks: fileassociation
Root: HKCU; Subkey: "Software\Classes\.m4a\OpenWithProgids"; ValueType: string; ValueData: ""; ValueName: "LuminaPlayer.Media"; Flags: uninsdeletevalue; Tasks: fileassociation
Root: HKCU; Subkey: "Software\LuminaPlayer\Capabilities\FileAssociations"; ValueType: string; ValueName: ".aac"; ValueData: "LuminaPlayer.Media"; Tasks: fileassociation
Root: HKCU; Subkey: "Software\Classes\Applications\LuminaPlayer.exe\SupportedTypes"; ValueType: string; ValueData: ""; ValueName: ".aac"; Tasks: fileassociation
Root: HKCU; Subkey: "Software\Classes\.aac\OpenWithProgids"; ValueType: string; ValueData: ""; ValueName: "LuminaPlayer.Media"; Flags: uninsdeletevalue; Tasks: fileassociation
Root: HKCU; Subkey: "Software\LuminaPlayer\Capabilities\FileAssociations"; ValueType: string; ValueName: ".ogg"; ValueData: "LuminaPlayer.Media"; Tasks: fileassociation
Root: HKCU; Subkey: "Software\Classes\Applications\LuminaPlayer.exe\SupportedTypes"; ValueType: string; ValueData: ""; ValueName: ".ogg"; Tasks: fileassociation
Root: HKCU; Subkey: "Software\Classes\.ogg\OpenWithProgids"; ValueType: string; ValueData: ""; ValueName: "LuminaPlayer.Media"; Flags: uninsdeletevalue; Tasks: fileassociation
Root: HKCU; Subkey: "Software\LuminaPlayer\Capabilities\FileAssociations"; ValueType: string; ValueName: ".opus"; ValueData: "LuminaPlayer.Media"; Tasks: fileassociation
Root: HKCU; Subkey: "Software\Classes\Applications\LuminaPlayer.exe\SupportedTypes"; ValueType: string; ValueData: ""; ValueName: ".opus"; Tasks: fileassociation
Root: HKCU; Subkey: "Software\Classes\.opus\OpenWithProgids"; ValueType: string; ValueData: ""; ValueName: "LuminaPlayer.Media"; Flags: uninsdeletevalue; Tasks: fileassociation
Root: HKCU; Subkey: "Software\LuminaPlayer\Capabilities\FileAssociations"; ValueType: string; ValueName: ".wma"; ValueData: "LuminaPlayer.Media"; Tasks: fileassociation
Root: HKCU; Subkey: "Software\Classes\Applications\LuminaPlayer.exe\SupportedTypes"; ValueType: string; ValueData: ""; ValueName: ".wma"; Tasks: fileassociation
Root: HKCU; Subkey: "Software\Classes\.wma\OpenWithProgids"; ValueType: string; ValueData: ""; ValueName: "LuminaPlayer.Media"; Flags: uninsdeletevalue; Tasks: fileassociation
Root: HKCU; Subkey: "Software\LuminaPlayer\Capabilities\FileAssociations"; ValueType: string; ValueName: ".m3u"; ValueData: "LuminaPlayer.Media"; Tasks: fileassociation
Root: HKCU; Subkey: "Software\Classes\Applications\LuminaPlayer.exe\SupportedTypes"; ValueType: string; ValueData: ""; ValueName: ".m3u"; Tasks: fileassociation
Root: HKCU; Subkey: "Software\Classes\.m3u\OpenWithProgids"; ValueType: string; ValueData: ""; ValueName: "LuminaPlayer.Media"; Flags: uninsdeletevalue; Tasks: fileassociation
Root: HKCU; Subkey: "Software\LuminaPlayer\Capabilities\FileAssociations"; ValueType: string; ValueName: ".m3u8"; ValueData: "LuminaPlayer.Media"; Tasks: fileassociation
Root: HKCU; Subkey: "Software\Classes\Applications\LuminaPlayer.exe\SupportedTypes"; ValueType: string; ValueData: ""; ValueName: ".m3u8"; Tasks: fileassociation
Root: HKCU; Subkey: "Software\Classes\.m3u8\OpenWithProgids"; ValueType: string; ValueData: ""; ValueName: "LuminaPlayer.Media"; Flags: uninsdeletevalue; Tasks: fileassociation

[Run]
Filename: "{app}\LuminaPlayer.exe"; Description: "Abrir LuminaPlayer"; Flags: nowait postinstall skipifsilent

[Code]
procedure InitializeWizard;
begin
  WizardForm.WelcomeLabel1.Caption := 'Bienvenido a LuminaPlayer';
  WizardForm.WelcomeLabel2.Caption := 'Vídeo y audio en una interfaz ligera y minimalista.' + #13#10 + #13#10 +
    'Este asistente instalará LuminaPlayer para tu usuario. No necesitas instalar Qt ni libmpv por separado.' + #13#10 + #13#10 +
    'Versión 0.9.0 Beta para Windows de 64 bits.';
end;
