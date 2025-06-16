@echo off
setlocal
setlocal EnableDelayedExpansion

set COMPILERS=clang msvc
set BUILD_TYPES=debug release relwithdebinfo minsizerel
set SANITIZERS=none address undefined fuzz

REM A few variables useful when testing host and target architectures
REM TODO: It would probably be more useful to actually check lib availability, however that logic would likely have to
REM be heuristic at best, and it's not necessarily clear if it would be better than this
set HOST_TARGET_SAME=0
set TARGET_NATIVE=1
set TARGET_ARM=0
if %Platform%==x86 (
    if /I %PROCESSOR_ARCHITECTURE%==x86 (
        set HOST_TARGET_SAME=1
    ) else if /I %PROCESSOR_ARCHITECTURE% NEQ AMD64 (
        set TARGET_NATIVE=0
    )
) else if %Platform%==x64 (
    if /I %PROCESSOR_ARCHITECTURE%==AMD64 (
        set HOST_TARGET_SAME=1
    ) else (
        REM NOTE: x64 is not considered native on x86
        set TARGET_NATIVE=0
    )
) else if %Platform%==arm (
    set TARGET_ARM=1
    if /I %PROCESSOR_ARCHITECTURE%==ARM (
        set HOST_TARGET_SAME=1
    ) else if /I %PROCESSOR_ARCHITECTURE% NEQ ARM64 (
        set TARGET_NATIVE=0
    )
) else if %Platform%==arm64 (
    set TARGET_ARM=1
    if /I %PROCESSOR_ARCHITECTURE%==ARM64 (
        set HOST_TARGET_SAME=1
    ) else (
        REM NOTE: arm64 is not considered native on arm
        set TARGET_NATIVE=0
    )
)

for %%c in (%COMPILERS%) do (
    for %%b in (%BUILD_TYPES%) do (
        for %%s in (%SANITIZERS%) do (
            set ARGS=-c %%c -b %%b
            set SHOULD_INIT=1
            set NEEDS_ASAN=0

            if %%s==address (
                set ARGS=!ARGS! -s address
                set NEEDS_ASAN=1
                if %TARGET_ARM%==1 (
                    set SHOULD_INIT=0
                )
            ) else if %%s==undefined (
                set ARGS=!ARGS! -s undefined
                if %%c==msvc (
                    set SHOULD_INIT=0
                ) else if %%c==clang (
                    if %%b==debug (
                        set SHOULD_INIT=0
                    )

                    REM Clang only ships native UBSan libraries
                    if %HOST_TARGET_SAME%==0 set SHOULD_INIT=0
                )
                if %TARGET_ARM%==1 (
                    set SHOULD_INIT=0
                )
            ) else if %%s==fuzz (
                set ARGS=!ARGS! -f
                set NEEDS_ASAN=1
                if %TARGET_ARM%==1 (
                    set SHOULD_INIT=0
                )
            )

            if !NEEDS_ASAN!==1 (
                if %%c==clang (
                    if %%b==debug (
                        set SHOULD_INIT=0
                    )
                )

                REM Neither Clang nor Visual Studio seem to ship ARM ASan libraries on x64 (& presumably vice-versa)
                REM TODO: Actually verify what ships on ARM64
                if %TARGET_NATIVE%==0 (
                    set SHOULD_INIT=0
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
    set "SUFFIX= (%3)"
)
echo ERROR: Configuration failed for %1 %2%SUFFIX%
goto :eof
