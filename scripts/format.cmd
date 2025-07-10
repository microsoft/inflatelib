@echo off
setlocal
setlocal EnableDelayedExpansion

set ROOT=%~dp0\..

set EXTS=.h .hpp .c .cpp
set DIRS=src test

for %%d in (%DIRS%) do call :format_files %ROOT%\%%d
goto :eof

:format_files
    :: Format all desired files in the target directory
    for %%e in (%EXTS%) do (
        for %%f in (%1\*%%e) do call :run_clang_format %%f
    )

    :: Recursively format all files in subdirectories
    for /d %%d in (%1\*) do call :format_files %%d
    goto :eof

:run_clang_format
    clang-format -style=file -i "%1"
    goto :eof
