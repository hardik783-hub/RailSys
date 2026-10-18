#include "repositories/Repository.h"

#include "app/AppError.h"

Repository::Repository(sqlite3* connection)
    : connection(connection) {
    if (!connection) {
        throw DatabaseError("Repository received a null connection");
    }
}

Statement Repository::prepare(const char* sql) const {
    return Statement(connection, sql);
}
