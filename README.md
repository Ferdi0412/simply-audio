# simply-audio
The plan is to make a library that makes it easy to work with basic audio processing in C++.

The main principle of this library is readable and intuitive code, and as such I have focused on following the **RAII** principle in C++. 


## Building
The project is built using `cmake`, and docs will be generated using `doxygen`.


**Building:**

First, [download](https://cmake.org/download/) and install **cmake**, then:
```sh
# Define what and where to build + any flags
cmake -S . -B build

# To build examples, add -DBUILD_EXAMPLES=ON
cmake -S . -B build -DBUILD_EXAMPLES=ON

# Once tests are added, use -DBUILD_TESTS=ON
cmake -S . -B build -DBUILD_TESTS=ON

# After that, build the project
cmake --build build

# Installation will be dealt with once I have made further progress
```


**Documentation:**

First, [download](https://www.doxygen.nl/download.html) and install **doxygen**, then:
```sh
cd docs
doxygen Doxyfile
```


**Project structure:**
```txt
simply-audio/
 ├─ src/ 
 │  ├─ threads.hpp   Class for handling threads with priority
 │  └─ init.hpp      Classes for initializing/uninitailizing necessary libraries
 │
 ├─ docs/            This is where docs will be generated
 │
 └─ tests/           This is where tests will be added later
```


**Header files:**
```txt
threads.hpp - provides 
init.hpp