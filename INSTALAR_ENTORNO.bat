@echo off
chcp 65001 >nul
rem Instala el compilador (MSYS2 + gcc + SDL2 + GLEW + libcurl) necesario para COMPILAR.bat.
rem Solo hace falta una vez. Requiere winget (Windows 10/11).
where winget >nul 2>nul || ( echo Necesitas winget ^(App Installer de Microsoft Store^). & pause & exit /b 1 )
if not exist C:\msys64\usr\bin\bash.exe (
  winget install --id MSYS2.MSYS2 -e --silent --accept-package-agreements --accept-source-agreements
)
C:\msys64\usr\bin\bash.exe -lc "pacman -Syu --noconfirm"
C:\msys64\usr\bin\bash.exe -lc "pacman -S --noconfirm --needed make python3 git diffutils mingw-w64-x86_64-gcc mingw-w64-x86_64-SDL2 mingw-w64-x86_64-glew mingw-w64-x86_64-pkg-config mingw-w64-x86_64-curl-winssl mingw-w64-x86_64-python-pillow"
echo.
echo Listo. Ahora pon tu ROM "Super Mario 64 (USA).z64" en esta carpeta y ejecuta COMPILAR.bat
pause
