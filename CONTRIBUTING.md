
# Development

## Requirements

1. A C compiler with C11 support
1. A C++ compiler & standard library with C++23 support (only required for building the tests)
1. [CMake](https://cmake.org/)
1. [Ninja](https://ninja-build.org/) (optional, but highly suggested)
1. [vcpkg](https://vcpkg.io/en/) (only required for building the tests). You also likely want to add `VCPKG_ROOT` as an environment variable
1. An up to date distribution of clang-format

As for the compilers, we highly suggest the most recent release of either MSVC, GCC, or Clang.
For development on Windows, we highly suggest [Windows Subsystem for Linux (WSL)](https://learn.microsoft.com/en-us/windows/wsl/install) for local validation on Linux.

## Setup

InflateLib uses CMake for generating its buildsystem files.
The simplest way to initialize CMake is by using one of the provided initialization scripts:

On Windows:
```cmd
> .\scripts\init.cmd
```

On Unix:
```bash
$ ./scripts/init.sh
```

You can run `init.cmd --help` or `init.sh -h` to view the set of arguments that can be provided to the initialization scripts.
This will show how to set the compiler, build type, generator, etc.

On Windows, the init script places build output in the `.\build\win\<config>` directory.
On Unix, build output is placed in the `./build/<config>` directory with the exception of WSL where output is placed in `./build/wsl/<config>`.
This difference in naming allows validation on WSL without build output overlapping.

> [!IMPORTANT]
> A number of heuristics are used to locate vcpkg on your system, the simplest being setting the `VCPKG_ROOT` environment variable.
> If vcpkg cannot be located, you must manually supply the path to the root of your vcpkg clone when running the initialization script.

> [!IMPORTANT]
> Initialization and building on Windows must be done from a Visual Studio "Native Tools Command Prompt" (i.e. `vcvarsall.cmd`).
> This will most commonly be found as a shortcut named something like "x64 Native Tools Command Prompt for VS 2022" in the start menu.

> [!WARNING]
> Not all compilers support all configurations.
> E.g. MSVC does not support UBSan and GCC does not support libFuzzer.
> The init scripts will produce an error if you try and use an invalid combination.

> [!TIP]
> You can leverage the `scripts/init-all.cmd` and `scripts/init-all.sh` scripts to run the initialization script for all valid configurations.
> Note that this requires both MSVC and Clang to be installed and up to date on Windows and both GCC and Clang to be installed and up to date on Linux.

> [!NOTE]
> The initialization scripts are geared towards local development.
> If you just want to build the library for consumption and ignore the tests, you will want to run CMake manually and pass `-DINFLATELIB_TEST=OFF` in addition to any other desired arguments, such as `-DCMAKE_INSTALL_PREFIX`.

## Building

Once CMake initialization is complete, you can build by either invoking the target generator's command or with `cmake --build <path>`. E.g.

```bash
$ ninja -C ./build/gccx64debug

$ cd ./build/gccx64debug
$ ninja

$ cmake --build ./build/gccx64debug
```

> [!TIP]
> You can also leverage the `scripts/build-all.cmd` and `scripts/build-all.sh` scripts for building all (compatible) configurations that were initialized with the init script.

## Testing

There are four primary configurations for testing: "normal", with Address Sanitizer enabled, with Undefined Behavior Sanitizer enabled, and with libFuzzer enabled.
These configurations are set at CMake initialization time using the init script's `-s` argument for enabling ASan or UBSan, `-f` for enabling fuzzing, or no additional argument for "normal" configuration.
Each configuration serves a separate purpose:

Configuration|Test Binaries Produced
-|-
"Normal"|`test/cpp/cpptests`<br/>`test/perf/perftests`
ASan|`test/cpp/cpptests`
UBSan|`test/cpp/cpptests`
Fuzz|`fuzz/fuzz-inflate`<br/>`fuzz/fuzz-inflate64`

The `cpptests` executable is a combination of unit tests and functional tests that covers many edge cases, failure cases, and code paths internal to the implementation.
This executable uses [Catch2](https://github.com/catchorg/Catch2) as its testing framework and accepts the standard Catch2 command-line arguments.

> [!TIP]
> You can leverage the `scripts/run-tests.cmd` and `scripts/run-tests.sh` scripts for running the `cpptests` executables for all compatible configurations that have been compiled.

The `perftests` executable is a very barebones benchmarking application that attempts to visualize how long the inflatelib library takes to decompress a set of test inputs.
Like all benchmarking applications, these results are only useful for comparisons, both with itself before and after changes are made, as well as against other libraries such as zlib, which it includes in its measurements by default.
This executable accepts a number of inputs that can be combined together:

Libraries/inputs to test against.
If none are specified, all of the following are included in the run.
* `inflatelib` - measure InflateLib runtimes with Deflate input.
* `inflatelib64` - measure InflateLib runtimes with Deflate64 input.
* `zlib` - measure zlib runtimes with Deflate input.

Output to display. If none are specified, all output is displayed to the console.
* `quiet` - does not display output to the console.
* `histogram` - displays a histogram for all desired outputs.
* `table` - displays a table of min, max, average, and median for all desired outputs.
* `totals` - displays a summary for total runtime of all inputs.
* `files` - displays summaries for each input.

> [!TIP]
> These arguments are particularly useful when paired with profiling applications such as `perf`.
> For example, you can use `quiet` to skip all of the output calculation logic and you can use library-specific options such as `inflatelib` to only test a single code path at a time.

Finally, the `fuzz-inflate*` executables are the libFuzzer instrumented targets.
These executables are compiled and submitted daily to a cloud service that will run them continuously, however you can also run them locally.
See the [libFuzzer documentation](https://llvm.org/docs/LibFuzzer.html#options) for a list of valid command-line arguments, the most useful being `-max_total_time` to control execution duration and the unnamed arguments for specifying input/output corpus directories.
It is generally a good idea to run one of these fuzzing binaries for at least 10 minutes prior to creating a PR.

> [!TIP]
> You can leverage the `scripts/fuzz-all.cmd` and `scripts/fuzz-all.sh` scripts to run all (compatible) compiled fuzzing binaries.
> This script will also take care of passing input and output corpus directories.
> You can also provide a single argument, which wil be used as the `-max_total_time` argument.

## Formatting

This project uses `clang-format` to ensure consistent formatting of all C and C++ source code.
Prior to issuing a PR, it is required that you format just your changes.

> [!WARNING]
> Different versions of `clang-format` will format code differently.
> This has two primary consequences.
> The first is that if you were to locally format *all* files in this repository, there's a chance you will be formatting many lines unrelated to your change.
> To make PRs manageable, do not submit PRs that touch lines you yourself did not modify.
> It is for this reason that we suggest you use [`git clang-format` to format your changes](https://clang.llvm.org/docs/ClangFormat.html#git-integration).
> The second consequence is that your local version of `clang-format` might format your changes differently than the version on the CI machine.
> If this is the case, comment on the PR that you ran `clang-format` and this requirement can be overridden.

## Tools

To aid the authoring of tests, several tools have been written and are included under the [test/tools](./test/tools) directory.

### The `bin-write` Tool

This is a tool that "compiles" a textual description of bits and bytes into a binary file.
The grammar is designed around the binary format of Deflate/Deflate64 and is described in [grammar.md](./test/tools/bin-write/grammar.md).
In particular, it supports writing binary as a "stream of bits" (left to right) in "chunks" that are not necessarily a multiple of a byte in length.
This is the only binary that runs as a part of the build, in particular it is used to "compile" the test inputs and outputs located in the [test/data](./test/data) directory.

### The `block-encode` Tool

This is a tool that converts an input description into either a static or dynamic compressed Deflate or Deflate64 block.
The output is in the format expected by the `bin-write` tool.
While this tool _does_ compute the Huffman tree for the given input, it does _not_ compress the data at all.
I.e. it will not produce length/distance pairs; these need to be specified manually.
This is because we want control over how the data is encoded so that we can write more interesting tests.

### The `byte-view` Tool

This is a very simple tool that reads a file as input and outputs its bytes in the format expected by the `bin-write` tool.
This is particularly useful when creating "real world" tests where a 3rd party application is used to compress a file that is then consumed by our tests.

### The `huffman-encode` Tool

This tool was used for writing the [tests](./test/cpp/HuffmanTreeTests.cpp) for the [`huffman_tree`](./src/lib/huffman_tree.c) type.
Given a Huffman tree described as a sequence of code lengths and some data described as a sequence of byes, encodes the data using the corresponding Huffman tree.
The output is an array of bytes that can be copied into the tests.

### The `zip-extract` Tool

Given the path to a zip file, this tool extracts each file from the zip file, writing its compressed bytes in the format expected by the `bin-write` tool.
This tool is the counterpart to the `byte-view` tool for creating "real world" tests.
