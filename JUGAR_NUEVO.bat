@echo off
rem Abre la version NUEVA (gorra lanzable con ojos, tecla V; cinta de herramientas en Pausa > R).
rem Usa su propio progreso en juego\build_dev\us_pc. Tu partida original sigue en juego\build\us_pc y no se toca.
cd /d "%~dp0juego\build_dev\us_pc"
set PATH=C:\msys64\mingw64\bin;%PATH%
if exist sm64.us.v6.exe ( start "" sm64.us.v6.exe --savepath ! --skip-intro & exit /b )
if exist sm64.us.v5.exe ( start "" sm64.us.v5.exe --savepath ! --skip-intro & exit /b )
if exist sm64.us.v4.exe ( start "" sm64.us.v4.exe --savepath ! --skip-intro & exit /b )
if exist sm64.us.v3.exe ( start "" sm64.us.v3.exe --savepath ! --skip-intro & exit /b )
if exist sm64.us.v2.exe ( start "" sm64.us.v2.exe --savepath ! --skip-intro & exit /b )
start "" sm64.us.f3dex2e.exe --savepath ! --skip-intro

