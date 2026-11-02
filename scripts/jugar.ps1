# Lanzador de Mario 64 Cooperativo: pregunta nombre, personaje y sala, y abre el juego.
$ErrorActionPreference = 'Stop'
$root = Split-Path -Parent $PSScriptRoot
$game = Join-Path $root 'juego\build\us_pc'
$exe  = Join-Path $game 'sm64.us.f3dex2e.exe'
$cfg  = Join-Path $game 'online.txt'
$defaultServer = 'wss://mario64-coop.mayacodegt.workers.dev'

if (-not (Test-Path $exe)) {
    Write-Host 'El juego aun no esta compilado. Ejecuta COMPILAR.bat (necesitas tu ROM y el entorno: INSTALAR_ENTORNO.bat).' -ForegroundColor Yellow
    Read-Host 'Pulsa Enter para salir'; exit 1
}

# valores guardados de la ultima vez
$saved = @{ url = $defaultServer; room = 'publica'; name = $env:USERNAME; char = '0' }
if (Test-Path $cfg) {
    foreach ($line in Get-Content $cfg) { if ($line -match '^(url|room|name|char)=(.*)$') { $saved[$Matches[1]] = $Matches[2].Trim() } }
}
$saved.name = ($saved.name -replace '[^\p{L}\p{N} _-]', '')
if (-not $saved.name) { $saved.name = 'Jugador' }

Write-Host ''
Write-Host '  MARIO 64 COOPERATIVO' -ForegroundColor Red
Write-Host '  --------------------'
$online = Read-Host '  Jugar en linea? [S/n]'
$offline = $online -match '^[nN]'

function Ask($label, $default) { $v = Read-Host ("  {0} [{1}]" -f $label, $default); if ($v) { $v } else { $default } }

$name = Ask 'Tu nombre (se ve sobre tu cabeza)' $saved.name
Write-Host '  Personajes: 1) Mario   2) Luigi   3) Wario   4) Waluigi'
$choice = Ask 'Personaje (1-4)' ([int]$saved.char + 1)
$char = [Math]::Min(3, [Math]::Max(0, ([int]$choice - 1)))

$lines = @()
if ($offline) {
    $lines += 'url='
} else {
    $room   = Ask 'Sala (comparte este nombre con tus amigos)' $saved.room
    $server = Ask 'Servidor' $saved.url
    $lines += "url=$server", "room=$room"
}
$lines += "name=$name", "char=$char"
Set-Content -Path $cfg -Value $lines -Encoding ascii

Write-Host ''
Write-Host '  Controles (como Project64): flechas = palanca, X = A, C = B, Z = Z, Enter = Start,' -ForegroundColor DarkGray
Write-Host '  A/S = L/R, I/J/K/L = botones C.' -ForegroundColor DarkGray
Start-Process -FilePath $exe -ArgumentList '--savepath', '!', '--skip-intro' -WorkingDirectory $game
