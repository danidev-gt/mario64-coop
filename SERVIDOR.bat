@echo off
rem Servidor local (mismo codigo que correra en Cloudflare). Escucha en ws://localhost:8787/ws?room=SALA&name=APODO
cd /d "%~dp0servidor"
if not exist node_modules call npm install
call npx wrangler dev --port 8787
