@echo off
powershell.exe -NoProfile -ExecutionPolicy Bypass -File "%~dp0Scripts\Multiplayer\Play-Internet.ps1" -Lobby
if errorlevel 1 pause
