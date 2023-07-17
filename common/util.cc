#include <iostream>
#include <optional>
#include "util.h"
#include <flutter/standard_method_codec.h>

namespace util {

std::optional<std::string> GetStringFromMap(const flutter::EncodableMap* m, const std::string key) {
    auto it = m->find(key);
    if (it != m->end() && !it->second.IsNull()) {
        return std::make_optional(std::get<std::string>(it->second));
    }

    return std::nullopt;
}

std::optional<std::int32_t> GetIntFromMap(const flutter::EncodableMap* m, const std::string key) {
    auto it = m->find(key);
    if (it != m->end() && !it->second.IsNull()) {
        return std::make_optional(std::get<std::int32_t>(it->second));
    }

    return std::nullopt;
}

}
