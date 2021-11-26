# Music player

Dependencies:
* developed in Visual Studio Code under Ubuntu
* dev tools: GCC, CMake, cpplint

Build it with
```
cmake -S . -B build
cmake --build build
```

## Bridge

Brige can be used to start player in server mode to receive commands from the network or select a file to play it via ALSA.

Start with playing single WAV file
```
./build/altBridge -f ~/Music/test/HotelCalifornia.wav -v --log-output=./build/output.txt
```
Verbose diagnostics will be written into the `./build/output.txt`.

See all parameters with
```
./build/altBridge --help
```

## Conventions



## Reference

Development reference:
* [GNU C Manual](https://www.gnu.org/software/gnu-c-manual/gnu-c-manual.html)
* [Google C++ Style Guide](https://google.github.io/styleguide/cppguide.html)
* [GNU C Library](https://www.gnu.org/software/libc/manual/html_node/)
* [Man pages](https://www.man7.org/linux/man-pages/man2/poll.2.html)
* [CMake Reference](https://cmake.org/cmake/help/v3.22/index.html)
* [GoogleTest User's Guide](https://google.github.io/googletest/)
* [ALSA](https://www.alsa-project.org/wiki/Main_Page)

IDE reference:
* [CMake in Visual Studio Code](https://code.visualstudio.com/docs/cpp/cmake-linux)
