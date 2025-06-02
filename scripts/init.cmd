@echo off
setlocal
setlocal EnableDelayedExpansion

:: Globals
set BUILD_ROOT=%~dp0\..\build\win

goto :init

:usage
    echo USAGE:
    echo     init.cmd [--help] [-c^|--compiler ^<clang^|msvc^>] [-g^|--generator ^<ninja^|msbuild^>]
    echo         [-b^|--build-type ^<debug^|release^|relwithdebinfo^|minsizerel^>] [-s^|--sanitize ^<address^|undefined^>]
    echo         [-f^|--fuzz] [-p^|--vcpkg path/to/vcpkg/root]
    echo.
    echo ARGUMENTS
    echo     -c^|--compiler       Controls the compiler used, either 'clang' (the default) or 'msvc'
    echo     -g^|--generator      Controls the CMake generator used, either 'ninja' (the default) or 'msbuild'
    echo     -b^|--build-type     Controls the value of 'CMAKE_BUILD_TYPE', either 'debug' (the default), 'release',
    echo                         'relwithdebinfo', or 'minsizerel'
    echo     -s^|--sanitize       Specifies the sanitizer to use, either 'address' or 'ub'. If this argument is not
    echo                         specified, no sanitizer will be used. This switch is incompatible with '--fuzz'
    echo     -f^|--fuzz           Builds the fuzzing target. This switch is incompatible with '--sanitize'
    echo     -p^|--vcpkg          Specifies the path to the root of your local vcpkg clone. If this argument is not
    echo                         specified, then several attempts will be made to try and deduce it. The first attempt
    echo                         will be to check for the presence of the %%VCPKG_ROOT%% environment variable. If that
    echo                         variable does not exist, the 'where' command will be used to try and locate the
    echo                         vcpkg.exe executable. If that check fails, the path '\vcpkg' will be used to try and
    echo                         locate the vcpkg clone. If all those checks fail, initialization will fail.
    goto :eof

:init
    :: Initialize values as empty so that we can identify if we are using defaults later for error purposes
    set COMPILER=
    set GENERATOR=
    set BUILD_TYPE=
    set CMAKE_ARGS=
    set VCPKG_ROOT_PATH=
    set SANITIZER=
    set FUZZ=0

:parse
    if /I "%~1"=="" goto :execute

    if /I "%~1"=="--help" call :usage & goto :eof

    set COMPILER_SET=0
    if /I "%~1"=="-c" set COMPILER_SET=1
    if /I "%~1"=="--compiler" set COMPILER_SET=1
    if %COMPILER_SET%==1 (
        if "%COMPILER%" NEQ "" echo ERROR: Compiler already specified & call :usage & exit /B 1

        if /I "%~2"=="clang" set COMPILER=clang
        if /I "%~2"=="msvc" set COMPILER=msvc
        if "!COMPILER!"=="" echo ERROR: Unrecognized/missing compiler %~2 & call :usage & exit /B 1

        shift
        shift
        goto :parse
    )

    set GENERATOR_SET=0
    if /I "%~1"=="-g" set GENERATOR_SET=1
    if /I "%~1"=="--generator" set GENERATOR_SET=1
    if %GENERATOR_SET%==1 (
        if "%GENERATOR%" NEQ "" echo ERROR: Generator already specified & call :usage & exit /B 1

        if /I "%~2"=="ninja" set GENERATOR=ninja
        if /I "%~2"=="msbuild" set GENERATOR=msbuild
        if "!GENERATOR!"=="" echo ERROR: Unrecognized/missing generator %~2 & call :usage & exit /B 1

        shift
        shift
        goto :parse
    )

    set BUILD_TYPE_SET=0
    if /I "%~1"=="-b" set BUILD_TYPE_SET=1
    if /I "%~1"=="--build-type" set BUILD_TYPE_SET=1
    if %BUILD_TYPE_SET%==1 (
        if "%BUILD_TYPE%" NEQ "" echo ERROR: Build type already specified & call :usage & exit /B 1

        if /I "%~2"=="debug" set BUILD_TYPE=debug
        if /I "%~2"=="release" set BUILD_TYPE=release
        if /I "%~2"=="relwithdebinfo" set BUILD_TYPE=relwithdebinfo
        if /I "%~2"=="minsizerel" set BUILD_TYPE=minsizerel
        if "!BUILD_TYPE!"=="" echo ERROR: Unrecognized/missing build type %~2 & call :usage & exit /B 1

        shift
        shift
        goto :parse
    )

    set SANITIZER_SET=0
    if /I "%~1"=="-s" set SANITIZER_SET=1
    if /I "%~1"=="--sanitize" set SANITIZER_SET=1
    if %SANITIZER_SET%==1 (
        if "%SANITIZER%" NEQ "" echo ERROR: Sanitizer already specified & call :usage & exit /B 1
        if %FUZZ%==1 echo ERROR: '~1' is incompatible with fuzzing & call :usage & exit /B 1

        if /I "%~2"=="address" set SANITIZER=asan
        if /I "%~2"=="undefined" set SANITIZER=ubsan
        if "!SANITIZER!"=="" echo ERROR: Unrecognized/missing sanitizer %~2 & call :usage & exit /B 1

        shift
        shift
        goto :parse
    )

    set FUZZ_SET=0
    if /I "%~1"=="-f" set FUZZ_SET=1
    if /I "%~1"=="--fuzz" set FUZZ_SET=1
    if %FUZZ_SET%==1 (
        if %SANITIZER_SET%==1 echo ERROR: '~1' is incompatible with sanitizers & call :usage & exit /B 1
        if %FUZZ%==1 echo ERROR: Fuzzing already specified & call :usage & exit /B 1

        set FUZZ=1

        shift
        goto :parse
    )

    set VCPKG_ROOT_SET=0
    if /I "%~1"=="-p" set VCPKG_ROOT_SET=1
    if /I "%~1"=="--vcpkg" set VCPKG_ROOT_SET=1
    if %VCPKG_ROOT_SET%==1 (
        if "%VCPKG_ROOT_PATH%" NEQ "" echo ERROR: vcpkg root path already specified & call :usage & exit /B 1
        if /I "%~2"=="" echo ERROR: Path to vcpkg root missing & call :usage & exit /B 1

        set VCPKG_ROOT_PATH=%~2

        shift
        shift
        goto :parse
    )

    echo ERROR: Unrecognized argument %~1
    call :usage
    exit /B 1

:execute
    :: Check for conflicting arguments
    if "%GENERATOR%"=="msbuild" (
        if "%COMPILER%"=="clang" echo ERROR: Cannot use Clang with MSBuild & exit /B 1
    )

    :: Select defaults
    if "%GENERATOR%"=="" set GENERATOR=ninja
    if %GENERATOR%==msbuild set COMPILER=msvc

    if "%COMPILER%"=="" set COMPILER=clang

    if "%BUILD_TYPE%"=="" set BUILD_TYPE=debug

    if "%VCPKG_ROOT_PATH%"=="" (
        :: First check for %VCPKG_ROOT% variable
        if defined VCPKG_ROOT (
            set VCPKG_ROOT_PATH=%VCPKG_ROOT%
        ) else (
            :: Next check the PATH for vcpkg.exe
            for %%i in (vcpkg.exe) do set VCPKG_ROOT_PATH=%%~dp$PATH:i

            if "!VCPKG_ROOT_PATH!"=="" (
                :: Finally, check the root of the drive for a clone of the name 'vcpkg'
                if exist \vcpkg\vcpkg.exe (
                    for %%i in (%cd%) do set VCPKG_ROOT_PATH=%%~di\vcpkg
                )
            )
        )
    )
    if "%VCPKG_ROOT_PATH%"=="" (
        echo ERROR: Unable to locate the root path of your local vcpkg installation.
        exit /B 1
    )

    :: These errors cannot be determined until after we select the compiler
    if %COMPILER%==msvc (
        if "%SANITIZER%"=="ubsan" echo ERROR: MSVC does not support Undefined Behavior Sanitizer & exit /B 1
    )

    if %COMPILER%==clang (
        set DEBUG_ERROR=0
        if "%SANITIZER%"=="asan" set DEBUG_ERROR=1
        if "%SANITIZER%"=="ubsan" set DEBUG_ERROR=1
        if %FUZZ%==1 set DEBUG_ERROR=1
        if !DEBUG_ERROR!==1 (
            if %BUILD_TYPE%==debug (
                echo ERROR: Clang does not currently support linking with debug runtime libraries with Address Sanitizer or Undefined Behavior Sanitizer enabled & exit /B 1
            )
        )
    )

    :: Formulate CMake arguments
    if %GENERATOR%==ninja set CMAKE_ARGS=%CMAKE_ARGS% -G Ninja

    :: TODO: Consider targeting clang++ instead of clang-cl
    if %COMPILER%==clang set CMAKE_ARGS=%CMAKE_ARGS% -DCMAKE_C_COMPILER=clang-cl -DCMAKE_CXX_COMPILER=clang-cl
    if %COMPILER%==msvc set CMAKE_ARGS=%CMAKE_ARGS% -DCMAKE_C_COMPILER=cl -DCMAKE_CXX_COMPILER=cl

    if %GENERATOR% NEQ msbuild (
        if %BUILD_TYPE%==debug set CMAKE_ARGS=%CMAKE_ARGS% -DCMAKE_BUILD_TYPE=Debug
        if %BUILD_TYPE%==release set CMAKE_ARGS=%CMAKE_ARGS% -DCMAKE_BUILD_TYPE=Release
        if %BUILD_TYPE%==relwithdebinfo set CMAKE_ARGS=%CMAKE_ARGS% -DCMAKE_BUILD_TYPE=RelWithDebInfo
        if %BUILD_TYPE%==minsizerel set CMAKE_ARGS=%CMAKE_ARGS% -DCMAKE_BUILD_TYPE=MinSizeRel
    )

    set SUFFIX=
    if "%SANITIZER%"=="asan" (
        set CMAKE_ARGS=%CMAKE_ARGS% -DINFLATELIB_ASAN=ON -DVCPKG_OVERLAY_TRIPLETS=..\..\..\vcpkg\triplets\asan
        set SUFFIX=-asan
    ) else if "%SANITIZER%"=="ubsan" (
        set CMAKE_ARGS=%CMAKE_ARGS% -DINFLATELIB_UBSAN=ON
        set SUFFIX=-ubsan
    ) else if %FUZZ%==1 (
        set CMAKE_ARGS=%CMAKE_ARGS% -DINFLATELIB_FUZZ=ON
        set SUFFIX=-fuzz
    )

    set CMAKE_ARGS=%CMAKE_ARGS% -DCMAKE_TOOLCHAIN_FILE="%VCPKG_ROOT_PATH%\scripts\buildsystems\vcpkg.cmake" -DCMAKE_EXPORT_COMPILE_COMMANDS=ON

    :: Figure out the platform
    set CLANG_ARCH=
    if "%Platform%"=="" (
        echo ERROR: The init.cmd script must be run from a Visual Studio command window & exit /B 1
    ) else if "%Platform%"=="x64" (
        set CLANG_ARCH=x86_64
    ) else if "%Platform%"=="x86" (
        set CLANG_ARCH=i386
        if %COMPILER%==clang set CFLAGS=-m32 & set CXXFLAGS=-m32
    ) else if "%Platform%"=="arm" (
        :: TODO: Figure out the clang architecture string for ARM
        if "%COMPILER%"=="clang" set CFLAGS=-target arm-win32-msvc & set CXXFLAGS=-target arm-win32-msvc
    ) else if "%Platform%"=="arm64" (
        :: TODO: Figure out the clang architecture string for ARM64
        if "%COMPILER%"=="clang" set CFLAGS=-target aarch64-win32-msvc & set CXXFLAGS=-target aarch64-win32-msvc
    )

    if "%SANITIZER%"=="asan" (
        :: If we're using ASAN, we need to build our dependencies with ASAN enabled as well.
        set CMAKE_ARGS=%CMAKE_ARGS% -DVCPKG_OVERLAY_TRIPLETS=..\..\..\vcpkg\triplets\asan
        if %COMPILER%==clang (
            set CMAKE_ARGS=%CMAKE_ARGS% -DVCPKG_TARGET_TRIPLET=%Platform%-windows-llvm
        )
    ) else if "%SANITIZER%"=="ubsan" (
        :: UBSan libs are built with static CRT linkage, so our dependencies need to do the same
        set CMAKE_ARGS=%CMAKE_ARGS% -DVCPKG_TARGET_TRIPLET=%Platform%-windows-static
    )

    :: Set up the build directory
    set BUILD_DIR=%BUILD_ROOT%\%COMPILER%%Platform%%BUILD_TYPE%%SUFFIX%
    mkdir %BUILD_DIR% > NUL 2>&1

    :: Run CMake
    pushd %BUILD_DIR%
    echo Using compiler....... %COMPILER%
    echo Using architecture... %Platform%
    echo Using build type..... %BUILD_TYPE%
    echo Using build root..... %CD%
    echo.

    cmake %CMAKE_ARGS% ..\..\..
    if %ERRORLEVEL% NEQ 0 (
        popd
        exit /B %ERRORLEVEL%
    )

    :: Clang will prefer linking with its own ASan libs when present (typically when the host architecture matches the
    :: target architecture). This is problematic because by default when we try and run the tests, it'll try and pick up
    :: the ASan DLL build for MSVC, which is not compatible with the one build for Clang. To resolve this issue, copy
    :: the ASan DLL to the directory where the executable gets written to.
    set TRY_COPY_ASAN_DLL=0
    if %COMPILER%==clang (
        if "%SANITIZER%"=="asan" set TRY_COPY_ASAN_DLL=1
        if %FUZZ%==1 set TRY_COPY_ASAN_DLL=1

        :: We don't know what DLL we're looking for, so don't even try
        if "%CLANG_ARCH%"=="" set TRY_COPY_ASAN_DLL=0
    )
    if %TRY_COPY_ASAN_DLL%==1 (
        for /f "delims=" %%c in ('where clang-cl 2^> NUL') do (
            set CLANG_CL_PATH=%%~dpc
        )
        if "!CLANG_CL_PATH!"=="" (
            echo WARNING: Unable to locate clang-cl. This may result in later errors when running tests
        ) else (
            for /f "delims=" %%d in ('where /R "!CLANG_CL_PATH!\..\lib" clang_rt.asan_dynamic-%CLANG_ARCH%.dll 2^> NUL') do (
                set ASAN_DLL_PATH=%%d
            )
            if "!ASAN_DLL_PATH!" NEQ "" (
                if "%SANITIZER%"=="asan" (
                    copy "!ASAN_DLL_PATH!" test\cpp\ > NUL
                ) else (
                    copy "!ASAN_DLL_PATH!" test\fuzz\inflate\ > NUL
                )
            )
        )
    )
    popd

    goto :eof