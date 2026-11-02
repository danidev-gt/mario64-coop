@echo off
chcp 65001 >nul
rem INSTALADOR de Mario 64 Cooperativo: prepara el compilador, compila el juego con TU ROM y crea un acceso directo en el escritorio.
rem El juego (.exe) se genera en tu PC a partir de tu ROM: contiene material de Nintendo y por eso no se distribuye ya hecho.
cd /d "%~dp0"

if not exist "Super Mario 64 (USA).z64" (
  echo.
  echo  Falta tu ROM. Copia "Super Mario 64 (USA).z64" ^(version USA^) en esta carpeta:
  echo  %CD%
  echo  y vuelve a ejecutar INSTALAR.bat
  echo.
  pause & exit /b 1
)

if not exist C:\msys64\usr\bin\bash.exe (
  echo  [1/3] Instalando el compilador ^(solo la primera vez, tarda unos minutos^)...
  call INSTALAR_ENTORNO.bat
  if not exist C:\msys64\usr\bin\bash.exe ( echo No se pudo instalar MSYS2. & pause & exit /b 1 )
) else (
  echo  [1/3] Compilador ya instalado.
)

echo  [2/3] Compilando el juego con tu ROM ^(10 a 20 minutos la primera vez^)...
call COMPILAR.bat
if not exist juego\build\us_pc\sm64.us.f3dex2e.exe ( echo La compilacion fallo. & pause & exit /b 1 )

echo  [3/3] Creando acceso directo en el escritorio...
powershell -NoProfile -Command "$s=(New-Object -ComObject WScript.Shell).CreateShortcut([Environment]::GetFolderPath('Desktop')+'\Mario 64 Cooperativo.lnk'); $s.TargetPath='%CD%\JUGAR.bat'; $s.WorkingDirectory='%CD%'; $s.IconLocation='%CD%\juego\build\us_pc\sm64.us.f3dex2e.exe'; $s.Save()"
echo.
echo  Listo. Abre "Mario 64 Cooperativo" desde el escritorio.
pause
