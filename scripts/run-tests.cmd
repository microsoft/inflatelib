@echo off
setlocal
setlocal EnableDelayedExpansion

set BUILD_ROOT=%~dp0\..\build\win

set COMPILERS=clang msvc
set BUILD_TYPES=debug release relwithdebinfo minsizerel
:: NOTE: We don't build tests with fuzzing enabled, so it's not included here
set SANITIZERS=none asan ubsan
set ARCHITECTURES=
if "%PROCESSOR_ARCHITECTURE%"=="AMD64" (
    set ARCHITECTURES=x86 x64
) else if "%PROCESSOR_ARCHITECTURE%"=="ARM64" (
    set ARCHITECTURES=arm arm64
) else (
    echo ERROR: Unknown host architecture %PROCESSOR_ARCHITECTURE%
)

for %%c in (%COMPILERS%) do (
    for %%a in (%ARCHITECTURES%) do (
        for %%b in (%BUILD_TYPES%) do (
            for %%s in (%SANITIZERS%) do (
                set SUFFIX=
                if "%%s" NEQ "none" (
                    set SUFFIX=-%%s
                )

                call :execute_tests %%c%%a%%b!SUFFIX!
                if !ERRORLEVEL! NEQ 0 (
                    call :error %%c%%a%%b!SUFFIX!
                    exit /B !ERRORLEVEL!
                )
            )
        )
    )
)

goto :eof

:execute_tests
set BUILD_DIR=%BUILD_ROOT%\%1
if not exist %BUILD_DIR%\test\cpp\cpptests.exe (
    goto :eof
)

pushd %BUILD_DIR%
echo Running tests from %CD%
test\cpp\cpptests.exe
goto :eof

:error
echo ERROR: Tests failed for %1
goto :eof
