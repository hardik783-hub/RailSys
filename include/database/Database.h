#pragma once

#include <sqlite3.h>
#include <string>

class Database {
private:
    sqlite3* connection = nullptr;

public:
    explicit Database(const std::string& path);
    ~Database();

    Database(const Database&) = delete;
    Database& operator=(const Database&) = delete;

    sqlite3* getConnection() const;

    void initializeSchema();
    void seedDemoData();
};