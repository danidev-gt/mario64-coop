@echo off
chcp 65001 >nul
rem INSTALADOR de Mario 64 Cooperativo: prepara el compilador, compila el juego con TU ROM y crea un acceso directo en el escritorio.
rem El .exe se genera en tu PC a partir de tu ROM (contiene material de Nintendo y por eso no se distribuye ya hecho).
rem Consejo: descomprime esta carpeta en una ruta CORTA, por ejemplo C:\Mario64Coop
cd /d "%~dp0"
set "ROM=Super Mario 64 (USA).z64"
set "SHA1OK=9bef1128717f958171a4afac3ed78ee2bb4e86ce"

rem --- 1. ROM: buscarla si no esta aqui ---
if not exist "%ROM%" (
  for %%D in ("%USERPROFILE%\Downloads" "%USERPROFILE%\Desktop" "%USERPROFILE%\Documents") do (
    if exist "%%~D\%ROM%" copy /y "%%~D\%ROM%" "%ROM%" >nul
  )
)
if not exist "%ROM%" (
  echo.
  echo  Falta tu ROM. Copia el archivo "%ROM%" en esta carpeta:
  echo    %CD%
  echo  y vuelve a ejecutar INSTALAR.bat
  echo.
  pause & exit /b 1
)
set "SHA1="
for /f "skip=1 tokens=*" %%H in ('certutil -hashfile "%ROM%" SHA1') do if not defined SHA1 set "SHA1=%%H"
set "SHA1=%SHA1: =%"
if /i not "%SHA1%"=="%SHA1OK%" (
  echo.
  echo  Esa ROM no es la correcta ^(version USA, formato .z64^).
  echo    Esperada: %SHA1OK%
  echo    Tuya:     %SHA1%
  echo  Si termina en .n64 o .v64 hay que convertirla a .z64 ^(big-endian^).
  echo.
  pause & exit /b 1
)
echo  [OK] ROM verificada.

rem --- 2. Compilador (MSYS2) ---
if not exist C:\msys64\usr\bin\bash.exe (
  echo  [1/3] Instalando el compilador ^(solo la primera vez, tarda unos minutos^)...
  call "%~dp0INSTALAR_ENTORNO.bat"
)
if not exist C:\msys64\usr\bin\bash.exe ( echo  No se pudo instalar MSYS2. Instalalo desde https://www.msys2.org y repite. & pause & exit /b 1 )
C:\msys64\usr\bin\bash.exe -lc "pacman -Q mingw-w64-x86_64-SDL2 mingw-w64-x86_64-glew mingw-w64-x86_64-gcc mingw-w64-x86_64-python-pillow make python3" >nul 2>nul
if errorlevel 1 (
  echo  Faltan paquetes del compilador, instalando...
  call "%~dp0INSTALAR_ENTORNO.bat"
)

rem --- 3. Compilar ---
echo  [2/3] Compilando el juego con tu ROM ^(10 a 20 minutos la primera vez^)...
call "%~dp0COMPILAR.bat" auto > instalacion.log 2>&1
if not exist juego\build\us_pc\sm64.us.f3dex2e.exe (
  echo.
  echo  La compilacion fallo. Las ultimas lineas de instalacion.log:
  powershell -NoProfile -Command "Get-Content instalacion.log -Tail 15"
  echo.
  echo  Si no lo entiendes, manda el archivo instalacion.log.
  pause & exit /b 1
)

rem --- 4. Acceso directo ---
echo  [3/3] Creando acceso directo en el escritorio...
powershell -NoProfile -Command "$s=(New-Object -ComObject WScript.Shell).CreateShortcut([Environment]::GetFolderPath('Desktop')+'\Mario 64 Cooperativo.lnk'); $s.TargetPath='%CD%\JUGAR.bat'; $s.WorkingDirectory='%CD%'; $s.IconLocation='%CD%\juego\build\us_pc\sm64.us.f3dex2e.exe'; $s.Save()"
echo.
echo  Listo. Abre "Mario 64 Cooperativo" desde el escritorio.
pause
