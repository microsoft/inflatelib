@echo off
setlocal
setlocal EnableDelayedExpansion

set BUILD_ROOT=%~dp0\..\build\win

if "%Platform%"=="" (
    echo ERROR: The build-all.cmd script must be run from a Visual Studio command window
    exit /B 1
)

set COMPILERS=clang msvc
set BUILD_TYPES=debug release relwithdebinfo minsizerel
set SANITIZERS=none asan ubsan fuzz

for %%c in (%COMPILERS%) do (
    for %%b in (%BUILD_TYPES%) do (
        for %%s in (%SANITIZERS%) do (
            call :build %%c %%b %%s
            if !ERRORLEVEL! NEQ 0 (
                call :error %%c %%b
                exit /B 1
            )
        )
    )
)

echo All builds completed successfully!

goto :eof

:: build [compiler] [type]
:build
set SUFFIX=
if "%3" NEQ "none" (
    set SUFFIX=-%3
)

set BUILD_DIR=%BUILD_ROOT%\%1%Platform%%2%SUFFIX%
if not exist %BUILD_DIR% (
    goto :eof
)

pushd %BUILD_DIR%
echo Building from %CD%
ninja
set EXIT_CODE=%ERRORLEVEL%
popd
exit /B %EXIT_CODE%

:error
echo ERROR: Build failed for %1 %2
goto :eof
