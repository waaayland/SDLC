@echo off
rem Запуск собранного приложения (см. build.bat).

setlocal

set "QT_DIR=C:\Qt\6.11.2\mingw_64"
set "BUILD_DIR=%USERPROFILE%\QtBuilds\PasswordGenerator"

if not exist "%BUILD_DIR%\PasswordGenerator.exe" (
    echo [ОШИБКА] Приложение не собрано. Сначала запустите build.bat
    exit /b 1
)

set "PATH=%QT_DIR%\bin;C:\Qt\Tools\mingw1310_64\bin;%PATH%"
start "" "%BUILD_DIR%\PasswordGenerator.exe"

endlocal
