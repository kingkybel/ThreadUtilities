# ThreadUtilities

A C++23 library for lightweight thread helpers and priority-based task scheduling.

## Purpose

This repository provides a compact set of reusable threading building blocks for C++ projects that need:
- safer async execution with explicit exception propagation
- easy conversion of callables into runnable thread work
- simple priority-based scheduling with a bounded worker pool

It is intended as a utility library, not a full task-runtime framework.

## What It Provides

- `make_exception_safe_future`: wraps a callable in a `std::future` and stores thrown exceptions in the future.
- `ThreadFunction` and `make_thread_func_ptr`: type-erased wrappers that turn callables + arguments into executable `std::thread` work items.
- `PriorityThread`: thread work with an ID and a priority value.
- `ThreadScheduler`: a small scheduler that runs queued work from a priority queue with a bounded worker pool.

# Installation

The installation and build are tested on *Ubuntu 24.04 LTS*.

## dependencies

googletest:

```bash
# create a directory where you like to clone googletest, eg: ~/Repos and change to it
mkdir ~/Repos ; cd ~/Repos
git clone https://github.com/google/googletest.git
cd googletest
mkdir build
cd build
cmake ..
make -j $(nproc)
sudo make install
```

## build, test, and install with CMake

```bash
mkdir -p build
cd build
cmake -Wno-dev -DCMAKE_BUILD_TYPE=RelWithDebInfo ..
cmake --build . --parallel "$(nproc)"
ctest --output-on-failure
```

## Coverage

The project includes a `coverage` target (requires `gcovr`).

```bash
sudo apt install -y gcovr
cmake -S . -B build-coverage -Wno-dev -DCMAKE_BUILD_TYPE=Coverage
cmake --build build-coverage --parallel "$(nproc)"
cmake --build build-coverage --target coverage
```

Coverage artifacts are written to:
- `build-coverage/coverage/coverage.xml`
- `build-coverage/coverage/coverage.html`

Install:

```bash
# change the next line to change the install prefix to your liking
INSTALL_PREFIX=/usr
mkdir -p build
cd build
cmake -Wno-dev -DCMAKE_INSTALL_PREFIX=${INSTALL_PREFIX} ..
cmake --build . --parallel $(nproc)
sudo cmake --install .
```

This will install the headers from the include-folder to `${INSTALL_PREFIX}/dkyb`

To use the headers in your code, make sure that ${INSTALL_PREFIX} is in the include directories of your project.
Include the file in your code e.g:

```c++
#include <dkyb/threadutil.h>
```
