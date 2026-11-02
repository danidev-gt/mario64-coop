@echo off
setlocal enabledelayedexpansion
chcp 65001 >nul
rem Partida en la nube (Worker de Cloudflare). Solo se sube el archivo de guardado (512 bytes), nunca el juego ni la ROM.
rem Tu codigo secreto queda en config\codigo_nube.txt: quien lo tenga puede leer y reemplazar esa partida.
set "SRV=https://mario64-coop.mayacodegt.workers.dev"
set "SAVE=%~dp0juego\build_dev\us_pc\sm64_save_file.bin"
set "CODEF=%~dp0config\codigo_nube.txt"
if not exist "%~dp0config" mkdir "%~dp0config"
if not exist "%CODEF%" powershell -NoProfile -Command "-join ((48..57)+(97..122) | Get-Random -Count 24 | %% {[char]$_})" > "%CODEF%"
set /p CODE=<"%CODEF%"

echo.
echo  PARTIDA EN LA NUBE   (codigo: %CODE%)
echo   1) Subir mi partida a la nube
echo   2) Descargar la partida de la nube (reemplaza la local, con copia de seguridad)
echo.
choice /c 12 /n /m " Elige 1 o 2: "
if errorlevel 2 goto bajar

:subir
curl -s -f -X PUT --data-binary "@%SAVE%" "%SRV%/save?code=%CODE%" && echo. && echo  Listo: partida subida.
goto fin

:bajar
tasklist /fi "imagename eq sm64.us.v6.exe" | find /i "sm64" >nul && (echo  Cierra el juego antes de descargar. & goto fin)
set "TS=%date:~-4%%date:~3,2%%date:~0,2%_%time:~0,2%%time:~3,2%"
set "TS=%TS: =0%"
copy /y "%SAVE%" "%~dp0config\respaldos\save_antes_de_nube_%TS%.bin" >nul
curl -s -f "%SRV%/save?code=%CODE%" -o "%SAVE%.nube" && move /y "%SAVE%.nube" "%SAVE%" >nul && echo  Listo: partida descargada (copia anterior en config\respaldos).
if errorlevel 1 echo  No hay partida con ese codigo en la nube.
:fin
echo.
pause
