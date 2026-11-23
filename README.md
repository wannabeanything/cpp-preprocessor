# C++ Include Preprocessor

A simple preprocessor for `#include` directives written in C++17. It recursively inlines included files into a single output file.

## How it works

- Includes in quotes (`#include "file.h"`) are searched first in the directory of the current file, then in the given include directories.
- Includes in angle brackets (`#include <file.h>`) are searched in the given include directories.
- Found files are pasted in place and processed the same way, so nested includes work.
- If a file cannot be found, the program prints `unknown include file <name> at file <path> at line <n>` and stops.

## Build and run

```
g++ -std=c++17 main.cpp -o preprocessor
./preprocessor
```

`main.cpp` runs a built-in test: it creates a small `sources/` tree with nested includes, runs the preprocessor and compares the result with the expected output.

## Tech

C++17, `<filesystem>`, `<regex>`, `<fstream>`.
