@echo off
setlocal

set "ROOT=%~dp0"
set "EXE=%ROOT%Bin\x64\Debug\Server\YunoServer.exe"

if not exist "%EXE%" (
    echo [ERROR] "%EXE%" not found.
    echo Build YunoServer first ^(Debug x64^).
    exit /b 1
)

set "LOGIN_HOST=%~1"
if "%LOGIN_HOST%"=="" set "LOGIN_HOST=127.0.0.1"

set "LOGIN_PORT=%~2"
if "%LOGIN_PORT%"=="" set "LOGIN_PORT=7000"

set "GAME_HOST=%~3"
if "%GAME_HOST%"=="" set "GAME_HOST=127.0.0.1"

set "GAME_PORT=%~4"
if "%GAME_PORT%"=="" set "GAME_PORT=9000"

set "BOT_COUNT=%~5"
if "%BOT_COUNT%"=="" set "BOT_COUNT=200"

set "DURATION_SEC=%~6"
if "%DURATION_SEC%"=="" set "DURATION_SEC=600"

echo [BOT] login=%LOGIN_HOST%:%LOGIN_PORT% game=%GAME_HOST%:%GAME_PORT% bots=%BOT_COUNT% duration=%DURATION_SEC%s
pushd "%ROOT%Bin\x64\Debug\Server" >nul
"%EXE%" --bot %LOGIN_HOST% %LOGIN_PORT% %GAME_HOST% %GAME_PORT% %BOT_COUNT% %DURATION_SEC%
popd >nul

endlocal
