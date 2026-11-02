@echo off
rem Abre el juego con la configuracion guardada la ultima vez (juego\build\us_pc\online.txt)
cd /d "%~dp0juego\build\us_pc"
start "" sm64.us.f3dex2e.exe --savepath ! --skip-intro
