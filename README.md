# Music player

Dependencies:
* developed in Visual Studio code under Ubuntu
* dev tools: GCC, CMake, cpplint

Build it with
```
cmake -S . -B build
cmake --build build
```

## Bridge

Brige can be used to start player in server mode to receive commands from the network or select a file to play it via ALSA.

Start with
```
./build/altBridge --help
```

## Reference

* [GNU C Manual](https://www.gnu.org/software/gnu-c-manual/gnu-c-manual.html)
* [CMake in Visual Studio Code](https://code.visualstudio.com/docs/cpp/cmake-linux)
* [GNU C Library](https://www.gnu.org/software/libc/manual/html_node/)
* [CMake Reference](https://cmake.org/cmake/help/v3.22/index.html)
