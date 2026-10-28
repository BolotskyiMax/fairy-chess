@echo off
chcp 65001 >nul

taskkill /F /IM chess_engine.exe >nul 2>&1
taskkill /F /IM python.exe >nul 2>&1
taskkill /F /IM pythonw.exe >nul 2>&1

for /f "usebackq tokens=*" %%i in (`"%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe" -latest -property installationPath`) do set VS_PATH=%%i
call "%VS_PATH%\VC\Auxiliary\Build\vcvars64.bat" >nul 2>&1

cl /nologo /EHsc /std:c++17 /utf-8 engine.cpp /Fe:chess_engine.exe >nul
if errorlevel 1 (
    echo COMPILATION FAILED
    pause
    exit /b 1
)

start "" "%LocalAppData%\Programs\Python\Python312\pythonw.exe" main.py
exit