@echo off
rem Compila el juego para Windows (requiere MSYS2 en C:\msys64 con gcc, SDL2, glew, make, python3)
rem y "Super Mario 64 (USA).z64" en esta carpeta.
cd /d "%~dp0"
if not exist "Super Mario 64 (USA).z64" ( echo Falta la ROM & if not "%1"=="auto" pause & exit /b 1 )
copy /Y "Super Mario 64 (USA).z64" juego\baserom.us.z64 >nul
set MSYSTEM=MINGW64
C:\msys64\usr\bin\bash.exe -lc "cd '%CD:\=/%/juego' && python3 tools/gen_characters.py && make VERSION=us WINDOWS_BUILD=1 BETTERCAMERA=1 TEXTURE_FIX=1 NODRAWINGDISTANCE=1 -j8"
if errorlevel 1 ( echo FALLO la compilacion & if not "%1"=="auto" pause & exit /b 1 )
for %%D in (SDL2.dll glew32.dll libwinpthread-1.dll) do copy /Y C:\msys64\mingw64\bin\%%D juego\build\us_pc\ >nul
if not exist juego\build\us_pc\sm64config.txt copy config\sm64config.txt juego\build\us_pc\sm64config.txt >nul
if not exist juego\build\us_pc\online.txt copy config\online.txt juego\build\us_pc\online.txt >nul
echo Listo. Ejecuta JUGAR.bat
if not "%1"=="auto" pause

