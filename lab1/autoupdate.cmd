@echo off
setlocal

rem Настройте переменные
set REPO_DIR=C:\Users\asgard\oslabs
set BUILD_DIR=C:\Users\asgard\oslabs\build

cd /d %REPO_DIR%

echo Updating repository...
git pull origin main

if ERRORLEVEL 1 (
    echo Error updating repository. Exiting.
    exit /b 1
)

cd /d %BUILD_DIR%
cmake %REPO_DIR% . 
cmake --build .

if ERRORLEVEL 1 (
    echo Build failed. Exiting.
    exit /b 1
)

echo Build completed successfully.
endlocal
exit /b 0