#include "database/Statement.h"

#include "app/AppError.h"

Statement::Statement(sqlite3* connection, const std::string& sql) {
    if (sqlite3_prepare_v2(
            connection, sql.c_str(), -1, &handle, nullptr
        ) != SQLITE_OK) {
        throw DatabaseError(
            std::string("Could not prepare statement: ") +
            sqlite3_errmsg(connection)
        );
    }
}

Statement::~Statement() {
    if (handle) {
        sqlite3_finalize(handle);
    }
}

Statement::Statement(Statement&& other) noexcept
    : handle(other.handle) {
    other.handle = nullptr;
}

Statement& Statement::operator=(Statement&& other) noexcept {
    if (this != &other) {
        if (handle) {
            sqlite3_finalize(handle);
        }
        handle = other.handle;
        other.handle = nullptr;
    }

    return *this;
}

void Statement::bindText(
    int index,
    const std::string& value
) const {
    if (sqlite3_bind_text(
            handle, index, value.c_str(), -1, SQLITE_TRANSIENT
        ) != SQLITE_OK) {
        throw DatabaseError("Could not bind text parameter");
    }
}

void Statement::bindInt(int index, int value) const {
    if (sqlite3_bind_int(handle, index, value) != SQLITE_OK) {
        throw DatabaseError("Could not bind integer parameter");
    }
}

void Statement::bindInt64(int index, sqlite3_int64 value) const {
    if (sqlite3_bind_int64(handle, index, value) != SQLITE_OK) {
        throw DatabaseError("Could not bind integer parameter");
    }
}

void Statement::bindNull(int index) const {
    if (sqlite3_bind_null(handle, index) != SQLITE_OK) {
        throw DatabaseError("Could not bind NULL parameter");
    }
}

int Statement::step() const {
    const int result = sqlite3_step(handle);

    if (result != SQLITE_ROW && result != SQLITE_DONE) {
        throw DatabaseError("Statement execution failed");
    }

    return result;
}

std::string Statement::columnText(int column) const {
    const auto* value = sqlite3_column_text(handle, column);

    return value
        ? reinterpret_cast<const char*>(value)
        : "";
}

int Statement::columnInt(int column) const {
    return sqlite3_column_int(handle, column);
}

sqlite3_int64 Statement::columnInt64(int column) const {
    return sqlite3_column_int64(handle, column);
}

double Statement::columnDouble(int column) const {
    return sqlite3_column_double(handle, column);
}

bool Statement::isNull(int column) const {
    return sqlite3_column_type(handle, column) == SQLITE_NULL;
}
