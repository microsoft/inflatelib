@echo off
setlocal
setlocal EnableDelayedExpansion

set COMPILERS=clang msvc
set BUILD_TYPES=debug release relwithdebinfo minsizerel
set SANITIZERS=none address undefined fuzz

for %%c in (%COMPILERS%) do (
    for %%b in (%BUILD_TYPES%) do (
        for %%s in (%SANITIZERS%) do (
            set ARGS=-c %%c -b %%b
            set SHOULD_INIT=1

            if %%s==address (
                set ARGS=!ARGS! -s address
                if %%c==clang (
                    if %%b==debug (
                        set SHOULD_INIT=0
                    )
                )
            ) else if %%s==undefined (
                set ARGS=!ARGS! -s undefined
                if %%c==msvc (
                    set SHOULD_INIT=0
                ) else if %%c==clang (
                    if %%b==debug (
                        set SHOULD_INIT=0
                    )
                )
            ) else if %%s==fuzz (
                set ARGS=!ARGS! -f
                if %%c==clang (
                    if %%b==debug (
                        set SHOULD_INIT=0
                    )
                )
            )

            if !SHOULD_INIT!==1 (
                call %~dp0\init.cmd !ARGS!
                if !ERRORLEVEL! NEQ 0 (
                    call :error %%c %%b %%s
                    exit /B 1
                )
            )
        )
    )
)

goto :eof

:error
set SUFFIX=
if "%3" NEQ "none" (
    set SUFFIX= (%3)
)
echo ERROR: Configuration failed for %1 %2%SUFFIX%
goto :eof
