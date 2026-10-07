@echo off
call "C:\Program Files\Epic Games\UE_5.8\Engine\Build\BatchFiles\Build.bat" GemTowerDefenseEditor Win64 Development "-Project=%~dp0GemTowerDefense.uproject" -WaitMutex -NoHotReloadFromIDE
pause
