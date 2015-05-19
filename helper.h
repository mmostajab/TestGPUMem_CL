#ifndef __HELPER_H__
#define __HELPER_H__

#include <string>
#include <iostream>
#include <fstream>
#include <vector>



static std::string convertFileToString(const std::string& filename);

// read the file content and generate a string from it.
static std::string convertFileToString(const std::string& filename) {
    std::ifstream ifile(filename);
    if (!ifile){
        return std::string("");
    }

    return std::string(std::istreambuf_iterator<char>(ifile), (std::istreambuf_iterator<char>()));

}

static std::string convertNumToString(const int& num){
  char buf[11];
  _itoa_s(num, buf, 10);
  return std::string(buf);
}

static std::string convertNumToString(const unsigned int& num){
  return convertNumToString(static_cast<int>(num));
}

static bool findString(const std::string& all_string, const std::string& search_string){
  size_t index = all_string.find(search_string);
  if (std::string::npos == index){
    return false;
  } else {
    return true;
  }
}

#endif