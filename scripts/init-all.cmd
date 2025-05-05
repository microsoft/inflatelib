@echo off
setlocal
setlocal EnableDelayedExpansion

set COMPILERS=clang msvc
set BUILD_TYPES=debug release relwithdebinfo minsizerel

for %%c in (%COMPILERS%) do (
    for %%b in (%BUILD_TYPES%) do (
        call %~dp0\init.cmd -c %%c -b %%b
        if !ERRORLEVEL! NEQ 0 (
            call :error %%c %%b
            exit /B 1
        )
    )
)

goto :eof

:error
echo ERROR: Configuration failed for %1 %2
goto :eof
