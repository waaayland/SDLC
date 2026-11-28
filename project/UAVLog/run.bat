@echo off
setlocal
set "QT_DIR=C:\Qt\6.11.2\mingw_64"
set "QT_TOOLS=C:\Qt\Tools"
set "BUILD_DIR=%USERPROFILE%\QtBuilds\UAVLog"
set "PATH=%QT_TOOLS%\mingw1310_64\bin;%QT_DIR%\bin;%PATH%"

if not exist "%BUILD_DIR%\uavlog.exe" (
    call "%~dp0build.bat"
    if errorlevel 1 exit /b 1
)
start "" "%BUILD_DIR%\uavlog.exe" %*
endlocal
