param(
    [string]$GameExecutable,
    [string]$EngineRoot = 'C:/Program Files/Epic Games/UE_5.5',
    [switch]$Lobby
)
$ErrorActionPreference = 'Stop'
$projectRoot = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '../..'))
if (-not (Get-Process -Name steam -ErrorAction SilentlyContinue)) { throw 'Start Steam and sign in before launching Internet play.' }
$config = [IO.File]::ReadAllText((Join-Path $projectRoot 'Config/DefaultEngine.ini'))
$section = [regex]::Match($config,'(?ms)^\[OnlineSubsystemSteam\]\s*\r?\n(?<body>.*?)(?=^\[|\z)')
$idMatch = [regex]::Match($section.Groups['body'].Value,'(?m)^SteamDevAppId=(\d+)')
if (-not $idMatch.Success -or [int64]$idMatch.Groups[1].Value -le 0) { throw 'Configure a Steam App ID first with Configure-Steam.ps1.' }
$previousSteamAppId = $env:SteamAppId
$previousSteamGameId = $env:SteamGameId
# Apply the size when the game window is created, before the first map loads.
$displayArguments = @('-windowed', '-ResX=1920', '-ResY=1080', '-ForceRes')
try {
    $env:SteamAppId = $idMatch.Groups[1].Value
    $env:SteamGameId = $idMatch.Groups[1].Value
    if ($GameExecutable) {
        if (-not (Test-Path -LiteralPath $GameExecutable -PathType Leaf)) { throw 'Packaged game executable not found.' }
        & $GameExecutable @displayArguments
    } else {
        $editor = Join-Path $EngineRoot 'Engine/Binaries/Win64/UnrealEditor.exe'
        if (-not (Test-Path -LiteralPath $editor -PathType Leaf)) { throw 'Unreal Engine 5.5 not found. Supply -EngineRoot.' }
        $map = if ($Lobby) { '/Game/Core/Levels/PVPLobby' } else { '/Game/Core/Levels/Start' }
        & $editor (Join-Path $projectRoot 'StrikeForge.uproject') $map '-game' @displayArguments
    }
} finally {
    $env:SteamAppId = $previousSteamAppId
    $env:SteamGameId = $previousSteamGameId
}
