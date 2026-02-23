#include <fstream>
#include <string>
#include <vector>
#include "value.h"

typedef RyValue (*RawNativeFn)(std::vector<RyValue>);
typedef void (*RegisterFn)(const char *, RawNativeFn, void *);

RyValue file_read_raw(std::vector<RyValue> args) {
  // Guard against empty args to prevent Segfaults
  if (args.empty() || !args[0].isString()) return RyValue();

  std::string path = args[0].asString();
  std::ifstream file(path);
  
  if (!file.is_open()) return RyValue(); // Return nil if file missing

  std::string content((std::istreambuf_iterator<char>(file)),
                       std::istreambuf_iterator<char>());
  return RyValue(content);
}

RyValue file_write_raw(std::vector<RyValue> args) {
  // Expecting: write_raw(path, content)
  if (args.size() < 2 || !args[0].isString() || !args[1].isString()) 
    return RyValue(false);

  std::ofstream file(args[0].asString());
  if (!file.is_open()) return RyValue(false);

  file << args[1].asString();
  return RyValue(true);
}

extern "C" void register_ry_module(RegisterFn register_fn, void *target) {
  register_fn("read", file_read_raw, target);
  register_fn("write", file_write_raw, target);
}