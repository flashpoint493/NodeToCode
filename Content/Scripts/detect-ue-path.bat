@echo off
REM NodeToCode UE Path Detector (Windows)
REM Finds Unreal Engine installation and updates AGENTS.md
REM Uses same Python detection methods as launch_bridge.bat

setlocal EnableDelayedExpansion

REM Get the directory where this script is located
set "SCRIPT_DIR=%~dp0"
set "PYTHON_SCRIPT=%SCRIPT_DIR%update_agents_ue_path.py"

REM Check if Python script exists
if not exist "%PYTHON_SCRIPT%" (
    echo [detect-ue-path] ERROR: Python script not found at %PYTHON_SCRIPT%
    exit /b 1
)

set "PYTHON_EXE="

REM === Method 1: Check Registry for Launcher-installed UE versions ===
for %%V in (5.7 5.6 5.5 5.4 5.3) do (
    if "!PYTHON_EXE!"=="" (
        for /f "tokens=2*" %%A in ('reg query "HKEY_LOCAL_MACHINE\SOFTWARE\EpicGames\Unreal Engine\%%V" /v InstalledDirectory 2^>nul') do (
            set "UE_DIR=%%B"
            if exist "!UE_DIR!\Engine\Binaries\ThirdParty\Python3\Win64\python.exe" (
                set "PYTHON_EXE=!UE_DIR!\Engine\Binaries\ThirdParty\Python3\Win64\python.exe"
                echo [detect-ue-path] Found UE %%V Python via registry
            )
        )
    )
)

REM === Method 2: Check Registry for Custom/Source builds ===
if "!PYTHON_EXE!"=="" (
    for /f "tokens=1,2*" %%A in ('reg query "HKEY_CURRENT_USER\SOFTWARE\Epic Games\Unreal Engine\Builds" 2^>nul') do (
        if "!PYTHON_EXE!"=="" (
            if "%%B"=="REG_SZ" (
                set "UE_DIR=%%C"
                if exist "!UE_DIR!\Engine\Binaries\ThirdParty\Python3\Win64\python.exe" (
                    set "PYTHON_EXE=!UE_DIR!\Engine\Binaries\ThirdParty\Python3\Win64\python.exe"
                    echo [detect-ue-path] Found custom UE build Python via registry
                )
            )
        )
    )
)

REM === Method 3: Check LauncherInstalled.dat ===
if "!PYTHON_EXE!"=="" (
    set "LAUNCHER_FILE=%PROGRAMDATA%\Epic\UnrealEngineLauncher\LauncherInstalled.dat"
    if exist "!LAUNCHER_FILE!" (
        for /f "tokens=2 delims=:," %%A in ('type "!LAUNCHER_FILE!" ^| findstr /C:"InstallLocation"') do (
            if "!PYTHON_EXE!"=="" (
                set "INSTALL_PATH=%%~A"
                set "INSTALL_PATH=!INSTALL_PATH:"=!"
                set "INSTALL_PATH=!INSTALL_PATH: =!"
                if exist "!INSTALL_PATH!\Engine\Binaries\ThirdParty\Python3\Win64\python.exe" (
                    set "PYTHON_EXE=!INSTALL_PATH!\Engine\Binaries\ThirdParty\Python3\Win64\python.exe"
                    echo [detect-ue-path] Found UE Python via LauncherInstalled.dat
                )
            )
        )
    )
)

REM === Method 4: Common installation paths ===
if "!PYTHON_EXE!"=="" (
    set "UE_PATHS=C:\Program Files\Epic Games;D:\Program Files\Epic Games;E:\Program Files\Epic Games"
    set "UE_PATHS=!UE_PATHS!;C:\Epic Games;D:\Epic Games;E:\Epic Games"
    set "UE_PATHS=!UE_PATHS!;C:\UE;D:\UE;E:\UE;F:\UE;G:\UE"

    set "UE_VERSIONS=UE_5.7 UE_5.6 UE_5.5 UE_5.4 UE_5.3 UE_5.2 UE_5.1 UE_5.0"

    for %%P in (!UE_PATHS!) do (
        for %%V in (!UE_VERSIONS!) do (
            if "!PYTHON_EXE!"=="" (
                set "TEST_PATH=%%P\%%V\Engine\Binaries\ThirdParty\Python3\Win64\python.exe"
                if exist "!TEST_PATH!" (
                    set "PYTHON_EXE=!TEST_PATH!"
                    echo [detect-ue-path] Found UE Python at common path
                )
            )
        )
    )
)

REM === Method 5: System Python ===
if "!PYTHON_EXE!"=="" (
    where python >nul 2>&1
    if !ERRORLEVEL! EQU 0 (
        set "PYTHON_EXE=python"
        echo [detect-ue-path] Using system Python
    )
)

if "!PYTHON_EXE!"=="" (
    where python3 >nul 2>&1
    if !ERRORLEVEL! EQU 0 (
        set "PYTHON_EXE=python3"
        echo [detect-ue-path] Using system Python3
    )
)

REM === No Python found ===
if "!PYTHON_EXE!"=="" (
    echo [detect-ue-path] ERROR: Python not found.
    echo [detect-ue-path] Please install Unreal Engine or Python.
    exit /b 1
)

REM Run the Python script to update AGENTS.md
echo [detect-ue-path] Running UE path detection...
"!PYTHON_EXE!" "%PYTHON_SCRIPT%" %*
