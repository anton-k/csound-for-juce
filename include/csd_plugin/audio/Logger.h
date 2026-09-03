#pragma once
#include <string>
#include <functional>

namespace csd_plugin {

enum class LogLevel { Info, Warning, Error };
using LogCallback = std::function<void(LogLevel, const char*)>;

}

