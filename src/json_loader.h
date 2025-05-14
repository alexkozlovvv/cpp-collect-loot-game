#pragma once

#include <filesystem>

#include "model.h"
#include "extra_data.h"

namespace json_loader {

model::Game LoadGame(const std::filesystem::path& json_path);

}  // namespace json_loader
