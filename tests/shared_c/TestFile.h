#pragma once
#include <filesystem>
#include <fstream>
#include "SharedTestFixture.h"

namespace fs = std::filesystem;

extern "C" {
  #include <stdio.h>
}

class TestFile {
  char* _path;

public:
  static char* GetTempFilePath(const char* prefix) {
    char *result = tempnam(NULL, prefix);
    assert(result != NULL);
    return result;
  }

  static std::string ReadContent(const char* filePath) {
    fs::permissions(filePath, fs::perms::owner_read, fs::perm_options::add);

    std::ifstream f1(filePath);
    std::istreambuf_iterator<char> i;
    std::string c1(std::istreambuf_iterator<char>(f1), i);
    return c1;
  }

  static bool CompareFiles(const char* p1, const char* p2) {
    fs::permissions(p1, fs::perms::owner_read, fs::perm_options::add);
    fs::permissions(p2, fs::perms::owner_read, fs::perm_options::add);

    std::ifstream f1(p1, std::ifstream::binary|std::ifstream::ate);
    std::ifstream f2(p2, std::ifstream::binary|std::ifstream::ate);

    if (f1.fail() || f2.fail()) {
      return false; //file problem
    }

    if (f1.tellg() != f2.tellg()) {
      return false; //size mismatch
    }

    f1.seekg(0, std::ifstream::beg);
    f2.seekg(0, std::ifstream::beg);
    return std::equal(
      std::istreambuf_iterator<char>(f1.rdbuf()),
      std::istreambuf_iterator<char>(),
      std::istreambuf_iterator<char>(f2.rdbuf()));
  }

  TestFile(const char* prefix, const char* content) {
    _path = GetTempFilePath(prefix);

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

  std::string GetContent() {
    return ReadContent(_path);
  }
};

