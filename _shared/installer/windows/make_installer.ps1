# Builds a Windows installer (.exe, Inno Setup 6) from one or more staged plugins.
#
# Each staged plugin is a folder <StageRoot>\<store-id>\ holding:
#   info.txt                       one line: <product name>|<version>|<bundle id>
#   VST3\<product name>.vst3\      the VST3 bundle folder
#   Standalone\<product name>.exe  the standalone app (optional)
#
# The installer puts each VST3 in C:\Program Files\Common Files\VST3 and each
# standalone in C:\Program Files\Nova Audio\<product name>\, with a Start-menu
# entry. With more than one plugin (Nova All Access) every plugin is a
# component the user can untick.
#
# Usage:
#   make_installer.ps1 -StageRoot stage -Ids nova-tape -OutFile out\nova-tape.exe
#   make_installer.ps1 -StageRoot stage -Ids (all) -OutFile out\all-access.exe `
#       -Title "Nova All Access" -AppId com.internationalnova.allaccess -Version 2026.11.28
param(
    [Parameter(Mandatory = $true)] [string]   $StageRoot,
    [Parameter(Mandatory = $true)] [string[]] $Ids,
    [Parameter(Mandatory = $true)] [string]   $OutFile,
    [string] $Title = "",
    [string] $AppId = "",
    [string] $Version = ""
)
$ErrorActionPreference = "Stop"

function Find-Iscc {
    $cmd = Get-Command iscc.exe -ErrorAction SilentlyContinue
    if ($cmd) { return $cmd.Source }
    foreach ($p in @("${env:ProgramFiles(x86)}\Inno Setup 6\ISCC.exe", "$env:ProgramFiles\Inno Setup 6\ISCC.exe",
                     "$env:LOCALAPPDATA\Programs\Inno Setup 6\ISCC.exe")) {
        if (Test-Path $p) { return $p }
    }
    throw "Inno Setup 6 (ISCC.exe) not found - install it with: choco install innosetup -y"
}

$StageRoot = (Resolve-Path $StageRoot).Path
$plugins = @()
foreach ($id in $Ids) {
    $dir = Join-Path $StageRoot $id
    $info = (Get-Content (Join-Path $dir "info.txt") -TotalCount 1).Trim().Split("|")
    $product = $info[0]
    $vst3 = Join-Path $dir "VST3\$product.vst3"
    if (-not (Test-Path $vst3)) { throw "missing VST3 bundle: $vst3" }
    $exe = Join-Path $dir "Standalone\$product.exe"
    $plugins += [pscustomobject]@{
        Id = $id; Product = $product; Version = $info[1]; Bundle = $info[2]
        Vst3 = $vst3; Exe = $(if (Test-Path $exe) { $exe } else { "" })
        Component = ($id -replace '[^A-Za-z0-9]', '_')
    }
}
if ($plugins.Count -eq 0) { throw "no plugins to package" }

$single = $plugins.Count -eq 1
if (-not $Title)   { $Title = $plugins[0].Product }
if (-not $AppId)   { $AppId = $plugins[0].Bundle }
if (-not $Version) { $Version = $plugins[0].Version }

$outPath = [System.IO.Path]::GetFullPath($OutFile)
$outDir = Split-Path $outPath
$outBase = [System.IO.Path]::GetFileNameWithoutExtension($outPath)
New-Item -ItemType Directory -Force -Path $outDir | Out-Null

$L = New-Object System.Collections.Generic.List[string]
$L.Add("[Setup]")
$L.Add("AppId=$AppId")
$L.Add("AppName=$Title")
$L.Add("AppVersion=$Version")
$L.Add("AppVerName=$Title $Version")
$L.Add("AppPublisher=Nova Audio")
$L.Add("AppPublisherURL=https://getnovaaudio.com")
$L.Add("AppSupportURL=https://getnovaaudio.com/support")
$L.Add("DefaultDirName={autopf}\Nova Audio\$Title")
$L.Add("DisableDirPage=yes")
$L.Add("DisableProgramGroupPage=yes")
$L.Add("UsePreviousAppDir=no")
$L.Add("OutputDir=$outDir")
$L.Add("OutputBaseFilename=$outBase")
$L.Add("ArchitecturesAllowed=x64compatible")
$L.Add("ArchitecturesInstallIn64BitMode=x64compatible")
$L.Add("MinVersion=10.0")
$L.Add("PrivilegesRequired=admin")
$L.Add("Compression=lzma2/max")
$L.Add("SolidCompression=yes")
$L.Add("WizardStyle=modern")
$L.Add("UninstallDisplayName=$Title")
$L.Add("CloseApplications=yes")
$L.Add("")

if (-not $single) {
    $L.Add("[Types]")
    $L.Add('Name: "full"; Description: "All plugins"')
    $L.Add('Name: "custom"; Description: "Choose plugins"; Flags: iscustom')
    $L.Add("")
    $L.Add("[Components]")
    foreach ($p in $plugins) { $L.Add("Name: ""$($p.Component)""; Description: ""$($p.Product)""; Types: full custom") }
    $L.Add("")
}

$L.Add("[InstallDelete]")
foreach ($p in $plugins) {
    $c = $(if ($single) { "" } else { "; Components: $($p.Component)" })
    $L.Add("Type: filesandordirs; Name: ""{commoncf64}\VST3\$($p.Product).vst3""$c")
}
$L.Add("")

$L.Add("[Files]")
foreach ($p in $plugins) {
    $c = $(if ($single) { "" } else { "; Components: $($p.Component)" })
    $L.Add("Source: ""$($p.Vst3)\*""; DestDir: ""{commoncf64}\VST3\$($p.Product).vst3""; Flags: ignoreversion recursesubdirs createallsubdirs$c")
    if ($p.Exe) {
        $L.Add("Source: ""$($p.Exe)""; DestDir: ""{autopf}\Nova Audio\$($p.Product)""; Flags: ignoreversion$c")
    }
}
$L.Add("")

$L.Add("[Icons]")
foreach ($p in $plugins) {
    if ($p.Exe) {
        $c = $(if ($single) { "" } else { "; Components: $($p.Component)" })
        $L.Add("Name: ""{autoprograms}\Nova Audio\$($p.Product)""; Filename: ""{autopf}\Nova Audio\$($p.Product)\$($p.Product).exe""$c")
    }
}
$L.Add("")

$L.Add("[UninstallDelete]")
foreach ($p in $plugins) { $L.Add("Type: dirifempty; Name: ""{autopf}\Nova Audio\$($p.Product)""") }
$L.Add('Type: dirifempty; Name: "{autopf}\Nova Audio"')

$iss = Join-Path $outDir "$outBase.iss"
[System.IO.File]::WriteAllLines($iss, $L, (New-Object System.Text.UTF8Encoding($true)))

$iscc = Find-Iscc
& $iscc /Qp $iss
if ($LASTEXITCODE -ne 0) { throw "ISCC failed ($LASTEXITCODE)" }
Remove-Item $iss
Write-Host "Installer: $outPath ($([math]::Round((Get-Item $outPath).Length / 1MB, 1)) MB, $($plugins.Count) plugin(s))"
