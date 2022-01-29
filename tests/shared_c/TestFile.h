#pragma once
#include <fstream>
#include "SharedTestFixture.h"

extern "C" {
  #include <stdio.h>
}

class TestFile {
  char* _path;

public:
  TestFile(const char* prefix, const char* content) {
    _path = tempnam(NULL, prefix);
    assert(_path != NULL);

    std::ofstream testFile;
    testFile.open(_path);
    testFile << content;
    testFile.close();
  }

  ~TestFile() {
    free(_path);
  }

  const char* GetPath() {
    return _path;
  }
};
