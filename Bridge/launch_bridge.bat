@echo off
REM NodeToCode MCP Bridge Launcher (Windows)
REM Finds Python using the same methods as UnrealVersionSelector:
REM   1. Windows Registry (Epic Games Launcher installations)
REM   2. Windows Registry (Custom/source builds)
REM   3. Common installation paths as fallback
REM   4. System Python as final fallback

setlocal EnableDelayedExpansion

REM Get the directory where this script is located
set "SCRIPT_DIR=%~dp0"
set "BRIDGE_SCRIPT=%SCRIPT_DIR%nodetocode_bridge.py"

REM Check if bridge script exists
if not exist "%BRIDGE_SCRIPT%" (
    echo [NodeToCode-Bridge] ERROR: Bridge script not found at %BRIDGE_SCRIPT% 1>&2
    exit /b 1
)

set "PYTHON_EXE="

REM === Method 1: Check Registry for Launcher-installed UE versions ===
REM These are registered at HKEY_LOCAL_MACHINE\SOFTWARE\EpicGames\Unreal Engine\<version>

for %%V in (5.7 5.6 5.5 5.4 5.3) do (
    if "!PYTHON_EXE!"=="" (
        for /f "tokens=2*" %%A in ('reg query "HKEY_LOCAL_MACHINE\SOFTWARE\EpicGames\Unreal Engine\%%V" /v InstalledDirectory 2^>nul') do (
            set "UE_DIR=%%B"
            if exist "!UE_DIR!\Engine\Binaries\ThirdParty\Python3\Win64\python.exe" (
                set "PYTHON_EXE=!UE_DIR!\Engine\Binaries\ThirdParty\Python3\Win64\python.exe"
                echo [NodeToCode-Bridge] Found UE %%V Python via registry: !PYTHON_EXE! 1>&2
            )
        )
    )
)

REM === Method 2: Check Registry for Custom/Source builds ===
REM These are registered at HKEY_CURRENT_USER\SOFTWARE\Epic Games\Unreal Engine\Builds

if "!PYTHON_EXE!"=="" (
    for /f "tokens=1,2*" %%A in ('reg query "HKEY_CURRENT_USER\SOFTWARE\Epic Games\Unreal Engine\Builds" 2^>nul') do (
        if "!PYTHON_EXE!"=="" (
            REM %%A is the GUID/name, %%B is REG_SZ, %%C is the path
            if "%%B"=="REG_SZ" (
                set "UE_DIR=%%C"
                if exist "!UE_DIR!\Engine\Binaries\ThirdParty\Python3\Win64\python.exe" (
                    set "PYTHON_EXE=!UE_DIR!\Engine\Binaries\ThirdParty\Python3\Win64\python.exe"
                    echo [NodeToCode-Bridge] Found custom UE build Python via registry: !PYTHON_EXE! 1>&2
                )
            )
        )
    )
)

REM === Method 3: Check LauncherInstalled.dat (Epic Games Launcher data file) ===

if "!PYTHON_EXE!"=="" (
    set "LAUNCHER_FILE=%PROGRAMDATA%\Epic\UnrealEngineLauncher\LauncherInstalled.dat"
    if exist "!LAUNCHER_FILE!" (
        REM Parse JSON to find InstallLocation - basic parsing with findstr
        for /f "tokens=2 delims=:," %%A in ('type "!LAUNCHER_FILE!" ^| findstr /C:"InstallLocation"') do (
            if "!PYTHON_EXE!"=="" (
                set "INSTALL_PATH=%%~A"
                REM Remove quotes and whitespace
                set "INSTALL_PATH=!INSTALL_PATH:"=!"
                set "INSTALL_PATH=!INSTALL_PATH: =!"
                if exist "!INSTALL_PATH!\Engine\Binaries\ThirdParty\Python3\Win64\python.exe" (
                    set "PYTHON_EXE=!INSTALL_PATH!\Engine\Binaries\ThirdParty\Python3\Win64\python.exe"
                    echo [NodeToCode-Bridge] Found UE Python via LauncherInstalled.dat: !PYTHON_EXE! 1>&2
                )
            )
        )
    )
)

REM === Method 4: Common installation paths as fallback ===

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
                    echo [NodeToCode-Bridge] Found UE Python at common path: !PYTHON_EXE! 1>&2
                )
            )
        )
    )
)

REM === Method 5: Fall back to system Python ===

if "!PYTHON_EXE!"=="" (
    where python >nul 2>&1
    if !ERRORLEVEL! EQU 0 (
        set "PYTHON_EXE=python"
        echo [NodeToCode-Bridge] Using system Python 1>&2
    )
)

if "!PYTHON_EXE!"=="" (
    where python3 >nul 2>&1
    if !ERRORLEVEL! EQU 0 (
        set "PYTHON_EXE=python3"
        echo [NodeToCode-Bridge] Using system Python3 1>&2
    )
)

REM === No Python found ===

if "!PYTHON_EXE!"=="" (
    echo [NodeToCode-Bridge] ERROR: Python not found. 1>&2
    echo [NodeToCode-Bridge] Please install Unreal Engine or Python. 1>&2
    exit /b 1
)

REM Run the bridge with all passed arguments
"!PYTHON_EXE!" "%BRIDGE_SCRIPT%" %*
