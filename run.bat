@echo off
rem ==========================================================================
rem  Fairy Chess - build and run
rem    run.bat         build the C++ engine and start the game
rem    run.bat debug   same, but Python runs in this console (errors are visible)
rem    run.bat test    build and run the engine self-tests
rem ==========================================================================
setlocal
cd /d "%~dp0"
chcp 65001 >nul

rem --- 1. Close a running copy of the game, otherwise chess_engine.exe is locked
taskkill /F /FI "WINDOWTITLE eq Fairy Chess" >nul 2>&1
taskkill /F /IM chess_engine.exe >nul 2>&1

rem --- 2. Set up the MSVC compiler (cl.exe needs the INCLUDE/LIB variables from vcvars64.bat)
if not defined INCLUDE goto :find_vs
where cl >nul 2>&1 && goto :have_msvc
:find_vs
set "VSWHERE=%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe"
if not exist "%VSWHERE%" goto :no_msvc
set "VS_PATH="
for /f "usebackq tokens=*" %%i in (`"%VSWHERE%" -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath`) do set "VS_PATH=%%i"
if not defined VS_PATH for /f "usebackq tokens=*" %%i in (`"%VSWHERE%" -latest -products * -property installationPath`) do set "VS_PATH=%%i"
if not defined VS_PATH goto :no_msvc
if not exist "%VS_PATH%\VC\Auxiliary\Build\vcvars64.bat" goto :no_msvc
call "%VS_PATH%\VC\Auxiliary\Build\vcvars64.bat" >nul
if not defined INCLUDE goto :no_msvc
where cl >nul 2>&1 || goto :no_msvc
:have_msvc

if not exist "cpp_engine\build" mkdir "cpp_engine\build"
rem Explicit list (not *.cpp), so stray old files in cpp_engine never break the build
set "ENGINE_SRC=cpp_engine\Piece.cpp cpp_engine\PieceFactory.cpp cpp_engine\Board.cpp cpp_engine\Game.cpp cpp_engine\Json.cpp cpp_engine\EngineServer.cpp"
if /i "%~1"=="test" goto :tests

rem --- 3. Build the engine (compiler output goes to cpp_engine\build\build.log)
echo Building chess engine...
cl /nologo /EHsc /std:c++17 /utf-8 /O2 /W4 /Focpp_engine\build\ /Fecpp_engine\chess_engine.exe %ENGINE_SRC% cpp_engine\main.cpp > "cpp_engine\build\build.log" 2>&1
if errorlevel 1 goto :build_failed

rem --- 4. Find Python 3 with PyQt6
set "PY="
for /f "delims=" %%i in ('py -3 -c "import sys; print(sys.executable)" 2^>nul') do set "PY=%%i"
if not defined PY for /f "delims=" %%i in ('python -c "import sys; print(sys.executable)" 2^>nul') do set "PY=%%i"
if not defined PY if exist "%LocalAppData%\Programs\Python\Python312\python.exe" set "PY=%LocalAppData%\Programs\Python\Python312\python.exe"
if not defined PY goto :no_python
if not exist "%PY%" goto :no_python

"%PY%" -c "import PyQt6.QtSvg" >nul 2>&1
if errorlevel 1 goto :no_pyqt

rem --- 5. Start the GUI (pythonw = no console window)
if /i "%~1"=="debug" goto :run_debug
set "PYW=%PY:python.exe=pythonw.exe%"
if not exist "%PYW%" set "PYW=%PY%"
start "" "%PYW%" python_ui\main.py
exit /b 0

:run_debug
"%PY%" python_ui\main.py
pause
exit /b 0

:tests
echo Building engine tests...
cl /nologo /EHsc /std:c++17 /utf-8 /O2 /Focpp_engine\build\ /Fecpp_engine\build\engine_tests.exe %ENGINE_SRC% cpp_engine\tests\engine_tests.cpp > "cpp_engine\build\tests.log" 2>&1
if errorlevel 1 (
    type "cpp_engine\build\tests.log"
    pause
    exit /b 1
)
"cpp_engine\build\engine_tests.exe"
pause
exit /b %errorlevel%

:no_msvc
echo [ERROR] The MSVC compiler (cl.exe) was not found.
echo Install Visual Studio or "Build Tools for Visual Studio" with the
echo "Desktop development with C++" workload.
pause
exit /b 1

:build_failed
echo [ERROR] Engine compilation failed:
echo.
type "cpp_engine\build\build.log"
pause
exit /b 1

:no_python
echo [ERROR] Python 3 was not found.
echo Install it from https://www.python.org and tick "Add python.exe to PATH".
pause
exit /b 1

:no_pyqt
echo [ERROR] PyQt6 is not installed for %PY%
echo Run:  "%PY%" -m pip install PyQt6
pause
exit /b 1
