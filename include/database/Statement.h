#pragma once

#include <sqlite3.h>
#include <string>

// RAII wrapper around a prepared sqlite3 statement.
// The statement is always finalized, even when an exception unwinds.
class Statement {
private:
    sqlite3_stmt* handle = nullptr;

public:
    Statement(sqlite3* connection, const std::string& sql);
    ~Statement();

    Statement(const Statement&) = delete;
    Statement& operator=(const Statement&) = delete;

    Statement(Statement&& other) noexcept;
    Statement& operator=(Statement&& other) noexcept;

    sqlite3_stmt* get() const { return handle; }

    void bindText(int index, const std::string& value) const;
    void bindInt(int index, int value) const;
    void bindInt64(int index, sqlite3_int64 value) const;
    void bindNull(int index) const;

    // Runs one sqlite3_step() and returns its result code
    // (SQLITE_ROW or SQLITE_DONE on success).
    int step() const;

    std::string columnText(int column) const;
    int columnInt(int column) const;
    sqlite3_int64 columnInt64(int column) const;
    double columnDouble(int column) const;
    bool isNull(int column) const;
};
