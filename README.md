## Overview
Pthread-based program that implements the parallel sorting by regular sampling
algorithm.

## Dependency
* C Compiler with ISO C99 Support (GCC **4.3+**).
* [CMake](https://cmake.org/) build system (**3.5+**).
* POSIX-Threading library with POSIX.1-2008 Support.
* [Python](https://www.python.org/) interpreter (**3.5+**).
* [Matplotlib](http://matplotlib.org/) plotting library.

**Ubuntu** (>= 16.04)  
```bash
sudo apt-get install build-essential cmake cmake-extras extra-cmake-modules python3-matplotlib
```

## Build Instructions
Change working directory to where the source directory resides and then issue:
```bash
mkdir build && cd build
cmake ..
make
```

## Getting Started
To get usage help from the compiled program obtained from the last section:
```bash
cd src
./psrs -h
```
For example, to obtain the running time of one thread quick sort to be used as
a baseline for comparison:
```bash
./psrs -l 10000000 -r 5 -t 1 -s 10
```
The *10000000* followed by the *-l* flag stands for the length of the generated
array; the *5* that comes after *-r* stands for how many runs are required in
order to obtain the average; the *1* followed by the *-t* flag stands for how
many threads to be launched; the last *10* is the seed value.


## Speedup Comparison
In order to get the speedup comparison graph using both array length and number
of threads as independent variables, run the python script resides in *tools*
subdirectory:
```bash
python3 tools/plot.py -p build/src/psrs -o speedup.png
```
The argument for *-p* flag is the path pointing to the *psrs* executable
compiled previously; *speedup.png*  is name of the speedup graph.

The result is obtained from the following input table:

| Array Size | Number of Threads |
|:----------:|:-----------------:|
| 2²⁰        | 1 2 4 8 16        |
| 2²²        | 1 2 4 8 16        |
| 2²⁴        | 1 2 4 8 16        |
| 2²⁶        | 1 2 4 8 16        |
| 2²⁸        | 1 2 4 8 16        |

Note that speedup values are calculated based on the average of 5 runs.


## Credit
* The Matplotlib CMake module is borrowed from the source repository of
[FreeCAD](
https://github.com/FreeCAD/FreeCAD/blob/master/cMake/FindMatplotlib.cmake).


## License
Copyright © 2016 Jiahui Xie  
Licensed under the [BSD 2-Clause License][BSD2].  
Distributed under the [BSD 2-Clause License][BSD2].  

[BSD2]: https://opensource.org/licenses/BSD-2-Clause
