@echo off
rem ---------------------------------------------------------------------------
rem  Сборка проекта "Генератор паролей" (ЛР №1, вариант 15).
rem
rem  Каталог сборки намеренно вынесен в путь БЕЗ кириллицы:
rem  утилита moc из Qt 6 не умеет создавать файлы в каталогах,
rem  имена которых содержат кириллические символы.
rem ---------------------------------------------------------------------------

setlocal

set "QT_DIR=C:\Qt\6.11.2\mingw_64"
set "QT_TOOLS=C:\Qt\Tools"
set "BUILD_DIR=%USERPROFILE%\QtBuilds\PasswordGenerator"

if not exist "%QT_DIR%" (
    echo [ОШИБКА] Не найден Qt: %QT_DIR%
    echo Отредактируйте переменную QT_DIR в этом файле.
    exit /b 1
)

set "PATH=%QT_TOOLS%\mingw1310_64\bin;%QT_TOOLS%\CMake_64\bin;%QT_TOOLS%\Ninja;%QT_DIR%\bin;%PATH%"

echo === Конфигурирование ===
cmake -S "%~dp0." -B "%BUILD_DIR%" -G Ninja ^
      -DCMAKE_BUILD_TYPE=Release ^
      -DCMAKE_PREFIX_PATH="%QT_DIR%" ^
      -DCMAKE_CXX_COMPILER=g++
if errorlevel 1 exit /b 1

echo.
echo === Сборка ===
cmake --build "%BUILD_DIR%"
if errorlevel 1 exit /b 1

echo.
echo Готово: "%BUILD_DIR%\PasswordGenerator.exe"
echo Запуск: run.bat
endlocal
