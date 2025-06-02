@echo off
setlocal
setlocal EnableDelayedExpansion

set BUILD_ROOT=%~dp0\..\build\win

set COMPILERS=clang msvc
set BUILD_TYPES=debug release relwithdebinfo minsizerel
set ARCHITECTURES=
if "%PROCESSOR_ARCHITECTURE%"=="AMD64" (
    set ARCHITECTURES=x86 x64
) else if "%PROCESSOR_ARCHITECTURE%"=="ARM64" (
    set ARCHITECTURES=arm arm64
) else (
    echo ERROR: Unknown host architecture %PROCESSOR_ARCHITECTURE%
)

:: Default to 10 minutes unless otherwise specified
set TIMEOUT=600
if "%1" NEQ "" (
    set TIMEOUT=%1
)

for %%c in (%COMPILERS%) do (
    for %%a in (%ARCHITECTURES%) do (
        for %%b in (%BUILD_TYPES%) do (
            call :fuzz %%c%%a%%b-fuzz
            if !ERRORLEVEL! NEQ 0 (
                exit /B !ERRORLEVEL!
            )
        )
    )
)

goto :eof

:fuzz
set BUILD_DIR=%BUILD_ROOT%\%1
if exist %BUILD_DIR% (
    pushd %BUILD_DIR%
    echo Fuzzing from !CD!
    call :fuzz-single test\fuzz\inflate fuzz-inflate.exe
    if !ERRORLEVEL! NEQ 0 (
        call :error %1 inflate
        popd
        goto :eof
    )

    call :fuzz-single test\fuzz\inflate64 fuzz-inflate64.exe
    if !ERRORLEVEL! NEQ 0 (
        call :error %1 inflate
        popd
        goto :eof
    )
    popd
)
goto :eof

:fuzz-single
if exist %1\%2 (
    mkdir %1\corpus-out > NUL 2>&1
    %1\%2 -max_total_time=%TIMEOUT% %1\corpus-out %1\corpus-in
)
goto :eof

:error
echo ERROR: Tests failed for %1 (%2)
goto :eof
