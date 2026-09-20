; ===========================================================================
;  Kain — Inno Setup 6 installer script
;
;  Canonical, version-controlled source for the Windows installer.
;  Invoked by packaging/build_package.py as:
;
;    iscc /dMyAppVersion=<ver> /dSourceDir=<staged-dir> packaging/kain.iss \
;         /O<packaging/windows> /Fkain-installer-<ver>-x64
;
;  The staged directory already contains: bin/, lib/, stdlib/, toolchain/,
;  setup.py, setup.bat, install_manifest.json, config.toml.
; ===========================================================================

#ifndef MyAppVersion
  #define MyAppVersion "0.0.0"
#endif

#ifndef SourceDir
  #error SourceDir must be defined (pass /dSourceDir=<staged-dir>)
#endif

#define MyAppName "Kain"
#define MyAppPublisher "Kain"
#define MyAppURL "https://github.com/kainlang/kain"
#define MyAppExeName "kain.exe"

[Setup]
AppId={{9F3C7B1E-4A62-4D8B-9E11-6C2A5F0D3A71}
AppName={#MyAppName}
AppVersion={#MyAppVersion}
AppVerName={#MyAppName} {#MyAppVersion}
AppPublisher={#MyAppPublisher}
AppPublisherURL={#MyAppURL}
AppSupportURL={#MyAppURL}
AppUpdatesURL={#MyAppURL}
DefaultDirName={autopf}\Kain
DefaultGroupName={#MyAppName}
DisableProgramGroupPage=yes
DisableWelcomePage=no
LicenseFile=
PrivilegesRequiredOverridesAllowed=dialog commandline
ArchitecturesAllowed=x64compatible
ArchitecturesInstallIn64BitMode=x64compatible
OutputDir=.
OutputBaseFilename=kain-installer-{#MyAppVersion}-x64
Compression=lzma2/max
SolidCompression=yes
WizardStyle=modern
ChangesEnvironment=yes
UninstallDisplayIcon={app}\bin\{#MyAppExeName}
UninstallDisplayName={#MyAppName} {#MyAppVersion}

[Languages]
Name: "english"; MessagesFile: "compiler:Default.isl"

[Tasks]
Name: "addtopath"; Description: "Add Kain to PATH (recommended)"; GroupDescription: "Environment:"; Flags: checkedonce

[Files]
; The whole staged distribution. ignoreversion so reinstall/upgrade always wins.
Source: "{#SourceDir}\*"; DestDir: "{app}"; Flags: recursesubdirs createallsubdirs ignoreversion

[Registry]
; Per-user PATH + KAIN_HOME. HKCU works for both per-user and admin installs
; and avoids clobbering a machine PATH that other tools depend on.
Root: HKCU; Subkey: "Environment"; ValueType: expandsz; ValueName: "Path"; ValueData: "{olddata};{app}\bin"; Tasks: addtopath; Check: NeedsAddPath(ExpandConstant('{app}\bin'))
Root: HKCU; Subkey: "Environment"; ValueType: string; ValueName: "KAIN_HOME"; ValueData: "{app}"; Tasks: addtopath; Flags: uninsdeletevalue

[Icons]
Name: "{group}\Kain Doctor"; Filename: "{app}\bin\{#MyAppExeName}"; Parameters: "doctor"
Name: "{group}\Uninstall {#MyAppName}"; Filename: "{uninstallexe}"

[Run]
Filename: "{app}\bin\{#MyAppExeName}"; Parameters: "doctor"; Description: "Verify the Kain installation"; Flags: postinstall skipifsilent nowait

[Code]
function NeedsAddPath(Param: string): Boolean;
var
  OrigPath: string;
begin
  if not RegQueryStringValue(HKEY_CURRENT_USER, 'Environment', 'Path', OrigPath) then
  begin
    Result := True;
    exit;
  end;
  Result := Pos(';' + Uppercase(Param) + ';', ';' + Uppercase(OrigPath) + ';') = 0;
end;

procedure RemovePathEntry(Entry: string);
var
  OrigPath, NewPath, Remaining, Token: string;
  P: Integer;
begin
  if not RegQueryStringValue(HKEY_CURRENT_USER, 'Environment', 'Path', OrigPath) then
    exit;
  NewPath := '';
  Remaining := OrigPath;
  while Remaining <> '' do
  begin
    P := Pos(';', Remaining);
    if P = 0 then
    begin
      Token := Remaining;
      Remaining := '';
    end
    else
    begin
      Token := Copy(Remaining, 1, P - 1);
      Remaining := Copy(Remaining, P + 1, Length(Remaining) - P);
    end;
    if (Token <> '') and (CompareText(Token, Entry) <> 0) then
    begin
      if NewPath <> '' then
        NewPath := NewPath + ';';
      NewPath := NewPath + Token;
    end;
  end;
  if NewPath <> OrigPath then
    RegWriteExpandStringValue(HKEY_CURRENT_USER, 'Environment', 'Path', NewPath);
end;

procedure CurUninstallStepChanged(CurUninstallStep: TUninstallStep);
begin
  if CurUninstallStep = usPostUninstall then
    RemovePathEntry(ExpandConstant('{app}\bin'));
end;
