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

## Reference

File format documentation:
* [Wav](http://tiny.systems/software/soundProgrammer/WavFormatDocs.pdf)

Used libraries:
* [ALSA](https://www.alsa-project.org/wiki/Main_Page), see [api](https://www.alsa-project.org/alsa-doc/alsa-lib/)
* [libFlac](https://www.xiph.org/flac/), see [api](https://www.xiph.org/flac/api/index.html)
* [libevent](https://libevent.org/), see [api](https://libevent.org/doc/) and [libevent-book](http://www.wangafu.net/~nickm/libevent-book/)
* [Nghttp2](http://nghttp2.org/), see [api](http://nghttp2.org/documentation/apiref.html)
* [OpenSSL](https://www.openssl.org/), see [api](https://www.openssl.org/docs/manmaster/man3/)
* [uriparser](https://uriparser.github.io/), see [api](https://uriparser.github.io/doc/api/latest/)

Development reference:
* [GNU C Manual](https://www.gnu.org/software/gnu-c-manual/gnu-c-manual.html)
* [Google C++ Style Guide](https://google.github.io/styleguide/cppguide.html)
* [GNU C Library](https://www.gnu.org/software/libc/manual/html_node/)
* [Man pages](https://www.man7.org/linux/man-pages/man2/poll.2.html)
* [CMake Reference](https://cmake.org/cmake/help/v3.22/index.html)
* [GoogleTest User's Guide](https://google.github.io/googletest/)

IDE reference:
* [CMake in Visual Studio Code](https://code.visualstudio.com/docs/cpp/cmake-linux)
