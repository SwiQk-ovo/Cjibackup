<#
  C极备份 · 一键发布到 GitHub（命令行版）
  浏览器上传附件会被 uploads.github.com 的跨域策略拦住，所以附件上传走这个脚本，最稳。

  用法（在 PowerShell 里执行）：
    cd F:\Cjibackup\release
    .\publish.ps1 -Version 1.0.7

  或者指定附件：
    .\publish.ps1 -Version 1.0.7 -Assets "C:\path\a.exe","C:\path\b.zip"

  Token 获取顺序：-Token 参数 → 环境变量 GH_TOKEN → 环境变量 CJI_GITHUB_TOKEN → 同目录下的 .token 文件
#>
[CmdletBinding()]
param(
    [Parameter(Mandatory = $true)][string]$Version,
    [string]$Owner = "SwiQk-ovo",
    [string]$Repo  = "Cjibackup",
    [string]$Title,
    [string]$Notes,
    [string]$NotesPath,
    [string[]]$Assets,
    [string]$Token,
    [switch]$Draft,
    [switch]$Prerelease
)

$ErrorActionPreference = "Stop"
[Net.ServicePointManager]::SecurityProtocol = [Net.SecurityProtocolType]::Tls12

function Info($m) { Write-Host "  $m" -ForegroundColor Cyan }
function Ok($m)   { Write-Host "  $m" -ForegroundColor Green }
function Warn($m) { Write-Host "  $m" -ForegroundColor Yellow }
function Fail($m) { Write-Host "  $m" -ForegroundColor Red; exit 1 }

$Version = $Version -replace '^[vV]', ''
$tag = "v$Version"
if (-not $Title) { $Title = "Cjibackup v$Version" }

# ---- token ----
if (-not $Token) { $Token = $env:GH_TOKEN }
if (-not $Token) { $Token = $env:CJI_GITHUB_TOKEN }
$tokenFile = Join-Path $PSScriptRoot ".token"
if (-not $Token -and (Test-Path $tokenFile)) { $Token = (Get-Content $tokenFile -Raw).Trim() }
if (-not $Token) { Fail "没有找到 token。请把 token 存到 $tokenFile，或设置环境变量 GH_TOKEN" }

# ---- notes ----
if (-not $Notes -and $NotesPath -and (Test-Path $NotesPath)) { $Notes = Get-Content $NotesPath -Raw -Encoding UTF8 }
if (-not $Notes) { $Notes = "Cjibackup v$Version" }

# ---- assets ----
# 版本目录：先找 KOPAL，再退回几个可能的位置（文件夹搬过家）
$verRoots = @(
    "C:\Users\46051\Desktop\CCODE\KOPAL\版本",
    "C:\Users\46051\Desktop\CCODE\CLesson\版本",
    "C:\Users\46051\Desktop\CLesson\版本"
)
$verDir = $null
foreach ($root in $verRoots) {
    $candidate = Join-Path $root $tag
    if (Test-Path $candidate) { $verDir = $candidate; break }
}
if (-not $verDir) { $verDir = Join-Path $verRoots[0] $tag }
if (-not $Assets -or $Assets.Count -eq 0) {
    $Assets = @()
    $official = Join-Path $verDir "正式版"
    if (Test-Path $official) {
        $Assets += (Get-ChildItem $official -File | Where-Object { $_.Extension -in @(".exe", ".zip") } | Select-Object -ExpandProperty FullName)  # 只传安装包/压缩包
    }
    if ($Assets.Count -eq 0) { Fail "没找到附件：$official 目录是空的，且没有用 -Assets 指定" }
}
foreach ($a in $Assets) { if (-not (Test-Path $a)) { Fail "附件不存在：$a" } }

$headers = @{
    Authorization          = "token $Token"
    Accept                 = "application/vnd.github+json"
    "X-GitHub-Api-Version" = "2022-11-28"
}
$base = "https://api.github.com/repos/$Owner/$Repo"

Write-Host ""
Write-Host "Cjibackup 发布  $Owner/$Repo  ->  $tag" -ForegroundColor White
Write-Host ("=" * 60)

# ---- 1. 查已有 release ----
$release = $null
try {
    $release = Invoke-RestMethod -Uri "$base/releases/tags/$tag" -Headers $headers -Method Get
    Warn "该 tag 已有 release（id=$($release.id)），将追加附件"
} catch {
    if ($_.Exception.Response.StatusCode.value__ -ne 404) {
        Fail "查询 release 失败：$($_.Exception.Message)"
    }
}

# ---- 2. 创建 release ----
if (-not $release) {
    $body = @{
        tag_name         = $tag
        target_commitish = "main"
        name             = $Title
        body             = $Notes
        draft            = [bool]$Draft
        prerelease       = [bool]$Prerelease
    } | ConvertTo-Json -Depth 5

    $release = Invoke-RestMethod -Uri "$base/releases" -Headers $headers -Method Post `
                                 -Body ([Text.Encoding]::UTF8.GetBytes($body)) `
                                 -ContentType "application/json; charset=utf-8"
    Ok "Release 创建成功：$($release.html_url)"
}

# ---- 3. 上传附件 ----
$existing = @()
if ($release.assets) { $existing = $release.assets | ForEach-Object { $_.name } }

foreach ($file in $Assets) {
    $name = Split-Path $file -Leaf
    if ($existing -contains $name) { Warn "跳过已存在附件：$name"; continue }

    $sizeMb = [math]::Round((Get-Item $file).Length / 1MB, 1)
    Info "上传 $name（$sizeMb MB）…"

    $uploadUrl = "https://uploads.github.com/repos/$Owner/$Repo/releases/$($release.id)/assets?name=$([uri]::EscapeDataString($name))"
    $upHeaders = @{
        Authorization  = "token $Token"
        Accept         = "application/vnd.github+json"
        "Content-Type" = "application/octet-stream"
    }

    $null = Invoke-RestMethod -Uri $uploadUrl -Headers $upHeaders -Method Post -InFile $file -TimeoutSec 900
    Ok "上传完成：$name"
}

Write-Host ("=" * 60)
Ok "全部完成"
Write-Host "  发布页：$($release.html_url)" -ForegroundColor Green
Write-Host "  提示：客户端「检查更新」现在就能检测到这个版本。" -ForegroundColor DarkGray
Write-Host ""
