## Overview
Pthread-based program that implements the parallel sorting by regular sampling
algorithm.

## Dependency
* C Compiler with ISO C99 Support (GCC **4.3+**).
* [CMake](https://cmake.org/) build system (**2.8.12+**).
* POSIX-Threading library with POSIX.1-2008 Support.

**Ubuntu**  
```bash
sudo apt-get install build-essential cmake
```

## Build Instructions
Change working directory to where the source directory resides and then issue:
```bash
mkdir build && cd build
cmake ..
make
```

## License
Copyright © 2016 Jiahui Xie
Licensed under the [BSD 2-Clause License][BSD2].  
Distributed under the [BSD 2-Clause License][BSD2].  

[BSD2]: https://opensource.org/licenses/BSD-2-Clause
