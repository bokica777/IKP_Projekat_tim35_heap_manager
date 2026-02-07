@echo off
setlocal
cd /d "%~dp0"

for %%T in (basic fragmentation segments_limit stress_test) do (
  echo ===== %%T =====
  start "" cmd /c ".\hm_server.exe"
  timeout /t 1 >nul
  .\hm_client.exe tests\%%T.txt
  timeout /t 1 >nul
)

pause
