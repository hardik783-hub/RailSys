#include "routes/StaticRoutes.h"

#include <fstream>
#include <sstream>
#include <string>
#include <vector>

namespace {

// Reads a file from the public directory and returns it as a Crow
// response with the correct content type. Missing files produce a
// helpful error message.
crow::response serveFile(const std::filesystem::path& publicDirectory,
                         const std::string& relativePath,
                         const std::string& contentType) {
    const auto fullPath = publicDirectory / relativePath;

    std::ifstream file(fullPath, std::ios::binary);

    if (!file) {
        return crow::response(
            500,
            "Could not open " + fullPath.string() +
                " - the RailSys frontend files were not found."
        );
    }

    std::ostringstream contents;
    contents << file.rdbuf();

    crow::response response(contents.str());
    response.set_header("Content-Type", contentType);
    return response;
}

}  // namespace

std::filesystem::path findPublicDirectory(
    const std::filesystem::path& executablePath) {
    std::vector<std::filesystem::path> candidates;
    candidates.emplace_back("public");

    std::error_code error;
    const auto absoluteExecutable =
        std::filesystem::absolute(executablePath, error);

    if (!error) {
        const auto executableDirectory = absoluteExecutable.parent_path();
        candidates.push_back(executableDirectory / "public");
        candidates.push_back(executableDirectory / ".." / "public");
    }

    for (const auto& candidate : candidates) {
        std::error_code checkError;
        if (std::filesystem::exists(candidate / "index.html",
                                    checkError)) {
            return candidate;
        }
    }

    return candidates.front();
}

void registerStaticRoutes(crow::SimpleApp& app,
                          const std::filesystem::path& publicDirectory) {
    CROW_ROUTE(app, "/")
    ([publicDirectory] {
        return serveFile(publicDirectory, "index.html",
                         "text/html; charset=utf-8");
    });

    CROW_ROUTE(app, "/style.css")
    ([publicDirectory] {
        return serveFile(publicDirectory, "style.css",
                         "text/css; charset=utf-8");
    });

    CROW_ROUTE(app, "/app.js")
    ([publicDirectory] {
        return serveFile(publicDirectory, "app.js",
                         "application/javascript; charset=utf-8");
    });
}
