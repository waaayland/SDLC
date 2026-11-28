@echo off
setlocal
set "QT_DIR=C:\Qt\6.11.2\mingw_64"
set "QT_TOOLS=C:\Qt\Tools"
set "BUILD_DIR=%USERPROFILE%\QtBuilds\UAVLog"

if not exist "%QT_DIR%" (
    echo [ERROR] Qt 6.11.2 not found at %QT_DIR%
    exit /b 1
)

set "PATH=%QT_TOOLS%\mingw1310_64\bin;%QT_TOOLS%\CMake_64\bin;%QT_TOOLS%\Ninja;%QT_DIR%\bin;%PATH%"

echo === Configuring UAVLog 1.0 ===
cmake -S "%~dp0." -B "%BUILD_DIR%" -G Ninja ^
      -DCMAKE_BUILD_TYPE=Release ^
      -DCMAKE_PREFIX_PATH="%QT_DIR%" ^
      -DCMAKE_CXX_COMPILER=g++
if errorlevel 1 exit /b 1

echo === Building UAVLog 1.0 ===
cmake --build "%BUILD_DIR%" -j 8
if errorlevel 1 exit /b 1

echo === Running CTest ===
ctest --test-dir "%BUILD_DIR%" --output-on-failure
if errorlevel 1 exit /b 1

echo Done: "%BUILD_DIR%\uavlog.exe"
endlocal
