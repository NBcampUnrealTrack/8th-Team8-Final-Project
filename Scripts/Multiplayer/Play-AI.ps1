param([string]$EngineRoot = 'C:/Program Files/Epic Games/UE_5.5')
$ErrorActionPreference = 'Stop'
$projectRoot = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '../..'))
$editor = Join-Path $EngineRoot 'Engine/Binaries/Win64/UnrealEditor.exe'
if (-not (Test-Path -LiteralPath $editor -PathType Leaf)) { throw 'Unreal Engine 5.5 not found. Supply -EngineRoot.' }
# Offline practice uses the same PVP lobby and battle rules, with no Steam account required.
& $editor (Join-Path $projectRoot 'StrikeForge.uproject') '/Game/Core/Levels/PVPLobby?SFPractice=1?SFOnline=0' '-game' '-nosteam' '-windowed' '-ResX=1920' '-ResY=1080' '-ForceRes'
