param(
    [string]$BuildDir = "F:\Cjibackup\build",
    [string]$DistDir  = "F:\Cjibackup\dist",
    [string]$QtDir    = "E:\Code\tools\Qt\6.5.3\mingw_64",
    [string]$MingwBin = "E:\Code\tools\MinGW\Tools\mingw1310_64\bin",
    [string]$Version  = "1.1.0",
    [string]$SignPfx  = $env:CJI_SIGN_PFX,          # 代码签名证书（.pfx），没有就跳过
    [string]$SignPassword = $env:CJI_SIGN_PASSWORD
)

$ErrorActionPreference = 'Stop'

function Write-Step($text) { Write-Host "==> $text" -ForegroundColor Cyan }

# 代码签名：有证书才签，没有就跳过（SmartScreen 的"发布者"只认数字签名）
function Find-SignTool {
    $candidates = @(
        "$env:ProgramFiles\Windows Kits\10\bin\*\x64\signtool.exe",
        "${env:ProgramFiles(x86)}\Windows Kits\10\bin\*\x64\signtool.exe"
    )
    foreach ($pattern in $candidates) {
        $found = Get-ChildItem $pattern -ErrorAction SilentlyContinue | Sort-Object FullName -Descending | Select-Object -First 1
        if ($found) { return $found.FullName }
    }
    return $null
}

function Sign-File([string]$path) {
    if (-not $SignPfx -or -not (Test-Path $SignPfx)) { return }
    $tool = Find-SignTool
    if (-not $tool) { Write-Host "  跳过签名：没找到 signtool（需要 Windows SDK）" -ForegroundColor Yellow; return }

    Write-Step "签名 $(Split-Path $path -Leaf)"
    & $tool sign /f "$SignPfx" /p "$SignPassword" /fd SHA256 /tr "http://timestamp.digicert.com" /td SHA256 "$path"
    if ($LASTEXITCODE -ne 0) { throw "签名失败：$path" }
}

$qtBin = Join-Path $QtDir "bin"
$qtPlugins = Join-Path $QtDir "plugins"

# packer / windeployqt 之类的小工具要能找到 Qt 和 MinGW 的 dll，
# 否则双击脚本或在干净环境里跑会「打包载荷失败」
$env:PATH = "$MingwBin;$qtBin;$env:PATH"

# 找主程序 exe（不硬编码文件名，靠排除法挑）
# 注意：build 目录里还有 CjibackupSetup / CjibackupUninstall / CjibackupBootstrap /
#       packer / CjiPublisher / 测试 / 截图工具，必须全部排除，只留主程序
$appExe = Get-ChildItem $BuildDir -Filter "*.exe" -File | Where-Object {
    $_.Name -notmatch 'Setup|Uninstall|Bootstrap|_test|smoke|shot_tool|packer|Publisher'
} | Select-Object -First 1
if (-not $appExe) { throw "找不到主程序 exe" }
if ($appExe.Name -ne "KOPAL.exe") {
    throw "挑到的主程序不对：$($appExe.Name)（应该是 KOPAL.exe）"
}

$setupExe     = Join-Path $BuildDir "CjibackupSetup.exe"
$uninstallExe = Join-Path $BuildDir "CjibackupUninstall.exe"
$bootstrapExe = Join-Path $BuildDir "CjibackupBootstrap.exe"
$packerExe    = Join-Path $BuildDir "packer.exe"
foreach ($f in @($setupExe, $uninstallExe, $bootstrapExe, $packerExe)) {
    if (-not (Test-Path $f)) { throw "缺少构建产物: $f" }
}

Write-Step "清理输出目录"
if (Test-Path $DistDir) { Remove-Item $DistDir -Recurse -Force }
$stage   = Join-Path $DistDir "portable"
$payload = Join-Path $DistDir "payload"
$boot    = Join-Path $DistDir "boot"
New-Item -ItemType Directory -Path $stage, $payload, $boot | Out-Null

Write-Step "复制主程序与 Qt 运行库"
Copy-Item $appExe.FullName (Join-Path $stage $appExe.Name)
Sign-File (Join-Path $stage $appExe.Name)
Sign-File $uninstallExe

$qtDlls = @("Qt6Core.dll", "Qt6Gui.dll", "Qt6Widgets.dll", "Qt6Network.dll", "Qt6Multimedia.dll")
foreach ($dll in $qtDlls) {
    $src = Join-Path $qtBin $dll
    if (-not (Test-Path $src)) { throw "缺少 Qt 运行库: $src" }
    Copy-Item $src $stage
}

$mingwDlls = @("libgcc_s_seh-1.dll", "libstdc++-6.dll", "libwinpthread-1.dll")
foreach ($dll in $mingwDlls) {
    $src = Join-Path $MingwBin $dll
    if (-not (Test-Path $src)) { throw "缺少 MinGW 运行库: $src" }
    Copy-Item $src $stage
}

$pluginGroups = @(
    @{ dir = "platforms";      files = @("qwindows.dll") },
    @{ dir = "styles";         files = @("qwindowsvistastyle.dll") },
    @{ dir = "tls";            files = @("qschannelbackend.dll") },
    @{ dir = "imageformats";   files = @("qgif.dll", "qjpeg.dll", "qico.dll") },
    @{ dir = "multimedia";     files = @("windowsmediaplugin.dll") }
)
foreach ($group in $pluginGroups) {
    $targetDir = Join-Path $stage $group.dir
    New-Item -ItemType Directory -Path $targetDir -Force | Out-Null
    foreach ($file in $group.files) {
        $src = Join-Path (Join-Path $qtPlugins $group.dir) $file
        if (Test-Path $src) { Copy-Item $src $targetDir }
    }
}

Write-Step "复制 Qt 中文翻译"
$translationDir = Join-Path $stage "translations"
New-Item -ItemType Directory -Path $translationDir -Force | Out-Null
$qm = Join-Path $QtDir "translations\qtbase_zh_CN.qm"
if (Test-Path $qm) { Copy-Item $qm $translationDir }

Write-Step "打包便携版 zip"
$portableZip = Join-Path $DistDir "Cjibackup_$Version`_portable.zip"
Compress-Archive -Path (Join-Path $stage "*") -DestinationPath $portableZip -CompressionLevel Optimal

Write-Step "生成安装载荷（压缩，给安装器用）"
Copy-Item (Join-Path $stage "*") $payload -Recurse -Force
Copy-Item $uninstallExe (Join-Path $payload "CjibackupUninstall.exe") -Force
& $packerExe $payload (Join-Path $DistDir "payload.pack")
if ($LASTEXITCODE -ne 0) { throw "打包载荷失败" }

Write-Step "生成引导载荷（不压缩，给引导程序用）"
Copy-Item $setupExe (Join-Path $boot "CjibackupSetup.exe")
Copy-Item (Join-Path $DistDir "payload.pack") $boot
foreach ($dll in $qtDlls) { Copy-Item (Join-Path $qtBin $dll) $boot }
foreach ($dll in $mingwDlls) { Copy-Item (Join-Path $MingwBin $dll) $boot }
foreach ($group in $pluginGroups) {
    $targetDir = Join-Path $boot $group.dir
    New-Item -ItemType Directory -Path $targetDir -Force | Out-Null
    foreach ($file in $group.files) {
        $src = Join-Path (Join-Path $qtPlugins $group.dir) $file
        if (Test-Path $src) { Copy-Item $src $targetDir }
    }
}
& $packerExe --stored $boot (Join-Path $DistDir "boot.pack")
if ($LASTEXITCODE -ne 0) { throw "打包引导载荷失败" }

Write-Step "拼接单文件安装包"
$setupOut = Join-Path $DistDir "Cjibackup_Setup_$Version.exe"
$bootSize = (Get-Item (Join-Path $DistDir "boot.pack")).Length

$out = [System.IO.File]::Create($setupOut)
$srcStream = [System.IO.File]::OpenRead($bootstrapExe)
$srcStream.CopyTo($out); $srcStream.Close()
$srcStream = [System.IO.File]::OpenRead((Join-Path $DistDir "boot.pack"))
$srcStream.CopyTo($out); $srcStream.Close()

$writer = New-Object System.IO.BinaryWriter($out)
$writer.Write([UInt64]$bootSize)
$writer.Write([System.Text.Encoding]::ASCII.GetBytes("CJIBOOT1"))
$writer.Flush()
$out.Close()

Sign-File $setupOut
Remove-Item $boot -Recurse -Force

Write-Step "结果"
foreach ($file in (Get-ChildItem $DistDir -File)) {
    Write-Host ("  {0}  {1} MB" -f $file.Name, [math]::Round($file.Length/1MB,2))
}
$stageFiles = Get-ChildItem $stage -Recurse -File
Write-Host ("便携版文件数: " + $stageFiles.Count)
Write-Host ("便携版大小: " + [math]::Round(($stageFiles | Measure-Object Length -Sum).Sum/1MB,2) + " MB")
Write-Host ("安装包: " + $setupOut)
Write-Host ("便携版: " + $portableZip)
