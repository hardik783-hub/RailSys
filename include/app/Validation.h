#pragma once

#include <cctype>
#include <string>

// Shared input validation used by the service layer.
namespace validation {

inline std::string trim(const std::string& value) {
    const auto start = value.find_first_not_of(" \t\r\n");
    if (start == std::string::npos) {
        return "";
    }

    const auto end = value.find_last_not_of(" \t\r\n");
    return value.substr(start, end - start + 1);
}

// Accepts YYYY-MM-DD with real month/day ranges.
inline bool isDate(const std::string& value) {
    if (value.size() != 10 ||
        value[4] != '-' || value[7] != '-') {
        return false;
    }

    for (std::size_t i = 0; i < value.size(); ++i) {
        if (i != 4 && i != 7 &&
            !std::isdigit(static_cast<unsigned char>(value[i]))) {
            return false;
        }
    }

    const int month = std::stoi(value.substr(5, 2));
    const int day = std::stoi(value.substr(8, 2));

    return month >= 1 && month <= 12 && day >= 1 && day <= 31;
}

// PNRs look like RS384B3B9B (prefix + 8 hex characters).
inline bool isPnr(const std::string& value) {
    if (value.size() != 10 || value[0] != 'R' || value[1] != 'S') {
        return false;
    }

    for (std::size_t i = 2; i < value.size(); ++i) {
        if (!std::isxdigit(static_cast<unsigned char>(value[i]))) {
            return false;
        }
    }

    return true;
}

}  // namespace validation
