#ifndef COMMON_UTIL_H_
#define COMMON_UTIL_H_
#pragma once

#include <flutter/standard_method_codec.h>

namespace util {

std::optional<std::string> GetStringFromMap(const flutter::EncodableMap* m, const std::string key);
std::optional<std::int32_t> GetIntFromMap(const flutter::EncodableMap* m, const std::string key);
std::optional<bool> GetBoolFromMap(const flutter::EncodableMap* m, const std::string key);

}
#endif // COMMON_UTIL_H_
