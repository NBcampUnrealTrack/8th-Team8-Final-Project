param([ValidateRange(1,2147483647)][int]$AppId = 480)
$ErrorActionPreference = 'Stop'
$projectRoot = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '../..'))
$configPath = Join-Path $projectRoot 'Config/DefaultEngine.ini'
$content = [IO.File]::ReadAllText($configPath)
$section = [regex]::Match($content,'(?ms)^\[OnlineSubsystemSteam\]\s*\r?\n(?<body>.*?)(?=^\[|\z)')
if (-not $section.Success) { throw 'OnlineSubsystemSteam section is missing from DefaultEngine.ini.' }
$body = $section.Groups['body'].Value
if ($body -match '(?m)^SteamDevAppId=') {
    $updatedBody = [regex]::Replace($body,'(?m)^SteamDevAppId=[^\r\n]*',"SteamDevAppId=$AppId")
} else { $updatedBody = "SteamDevAppId=$AppId`r`n$body" }
$updatedSection = $section.Value.Replace($body,$updatedBody)
$content = $content.Substring(0,$section.Index) + $updatedSection + $content.Substring($section.Index+$section.Length)
[IO.File]::WriteAllText($configPath,$content,(New-Object Text.UTF8Encoding($false)))
Write-Output "Steam App ID configured: $AppId"
if ($AppId -eq 480) { Write-Output '480 is the shared development test application. Use your own Steam App ID for release.' }
Write-Output 'Both players must use the same App ID and game version, and sign into separate Steam accounts on separate PCs.'
