@echo off
setlocal
setlocal EnableDelayedExpansion

set BUILD_ROOT=%~dp0\..\build

set COMPILERS=clang msvc
set BUILD_TYPES=debug release relwithdebinfo minsizerel
set ARCHITECTURES=32 64

for %%c in (%COMPILERS%) do (
    for %%a in (%ARCHITECTURES%) do (
        for %%b in (%BUILD_TYPES%) do (
            call :execute_tests %%c%%a%%b
            if !ERRORLEVEL! NEQ 0 (
                call :error %%c%%a%%b
                exit /B !ERRORLEVEL!
            )
        )
    )
)

goto :eof

:execute_tests
set BUILD_DIR=%BUILD_ROOT%\%1
if not exist %BUILD_DIR% (
    goto :eof
)

pushd %BUILD_DIR%
echo Running tests from %CD%
test\cpp\cpptests.exe
goto :eof

:error
echo ERROR: Tests failed for %1
goto :eof
