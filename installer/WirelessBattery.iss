; Inno Setup script for Wireless Battery (https://jrsoftware.org/isinfo.php).
; Per-user install: no administrator rights needed, installs to %LOCALAPPDATA%\Programs.
; Build with installer\build-installer.ps1 (or: ISCC /DAppVersion=1.2.3 installer\WirelessBattery.iss).

#ifndef AppVersion
  #define AppVersion "1.0.0"
#endif
#define Root ".."

[Setup]
AppId={{7E0B5C2A-3F4D-4C8E-9B61-2D5A8F1C4E93}
AppName=Wireless Battery
AppVersion={#AppVersion}
AppVerName=Wireless Battery {#AppVersion}
AppPublisher=Wireless Battery contributors
DefaultDirName={autopf}\Wireless Battery
DisableProgramGroupPage=yes
DisableDirPage=auto
PrivilegesRequired=lowest
ArchitecturesAllowed=x64compatible
ArchitecturesInstallIn64BitMode=x64compatible
MinVersion=10.0
OutputDir={#Root}\dist
OutputBaseFilename=WirelessBattery-Setup-{#AppVersion}
SetupIconFile={#Root}\res\app.ico
UninstallDisplayIcon={app}\WirelessBattery.exe
UninstallDisplayName=Wireless Battery
Compression=lzma2/max
SolidCompression=yes
WizardStyle=modern
CloseApplications=no

[Tasks]
Name: "autostart"; Description: "Start Wireless Battery when I sign in to Windows"

[Files]
Source: "{#Root}\WirelessBattery.exe"; DestDir: "{app}"; Flags: ignoreversion
Source: "{#Root}\third_party\headsetcontrol\headsetcontrol.exe"; DestDir: "{app}"; Flags: ignoreversion
Source: "{#Root}\LICENSE"; DestDir: "{app}"; DestName: "LICENSE.txt"; Flags: ignoreversion
Source: "{#Root}\installer\THIRD_PARTY_NOTICES.txt"; DestDir: "{app}"; Flags: ignoreversion

[Icons]
Name: "{autoprograms}\Wireless Battery"; Filename: "{app}\WirelessBattery.exe"

[Run]
Filename: "{app}\WirelessBattery.exe"; Description: "Start Wireless Battery now"; Flags: nowait postinstall skipifsilent

[UninstallRun]
Filename: "{app}\WirelessBattery.exe"; Parameters: "--quit"; Flags: runhidden waituntilterminated; RunOnceId: "QuitApp"

[Code]
const
  RunKey = 'Software\Microsoft\Windows\CurrentVersion\Run';

procedure QuitRunningApp();
var
  Code: Integer;
begin
  if FileExists(ExpandConstant('{app}\WirelessBattery.exe')) then
    Exec(ExpandConstant('{app}\WirelessBattery.exe'), '--quit', '', SW_HIDE, ewWaitUntilTerminated, Code);
end;

function InstalledRunValue(): String;
begin
  Result := '"' + ExpandConstant('{app}\WirelessBattery.exe') + '"';
end;

{ Removes the startup entry only if it points at this installed copy, so a portable copy's
  "Start with Windows" setting is left alone. }
procedure RemoveOwnAutostart();
var
  Value: String;
begin
  if RegQueryStringValue(HKCU, RunKey, 'WirelessBattery', Value) and (CompareText(Value, InstalledRunValue()) = 0) then
    RegDeleteValue(HKCU, RunKey, 'WirelessBattery');
end;

procedure CurStepChanged(CurStep: TSetupStep);
begin
  if CurStep = ssInstall then
    QuitRunningApp();  { upgrade: close the old version so its files can be replaced }
  if CurStep = ssPostInstall then
  begin
    { Same value the app's own "Start with Windows" menu item writes, so the two stay in sync. }
    if WizardIsTaskSelected('autostart') then
      RegWriteStringValue(HKCU, RunKey, 'WirelessBattery', InstalledRunValue())
    else
      RemoveOwnAutostart();
  end;
end;

procedure CurUninstallStepChanged(CurUninstallStep: TUninstallStep);
begin
  if CurUninstallStep = usPostUninstall then
  begin
    RemoveOwnAutostart();
    RegDeleteKeyIncludingSubkeys(HKCU, 'Software\WirelessBattery');
  end;
end;
