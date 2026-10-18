#pragma once

#include "crow.h"

#include <filesystem>

// Finds the public/ directory containing the frontend files.
// Checked locations, in order:
//   1. ./public          (server started from the project root)
//   2. <exe dir>/public  (server started elsewhere, e.g. build/)
//   3. <exe dir>/../public
std::filesystem::path findPublicDirectory(
    const std::filesystem::path& executablePath);

// Serves the frontend files from the given public/ directory.
void registerStaticRoutes(crow::SimpleApp& app,
                          const std::filesystem::path& publicDirectory);
