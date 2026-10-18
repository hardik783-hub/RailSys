#include "database/Transaction.h"

#include "app/AppError.h"

Transaction::Transaction(sqlite3* connection)
    : connection(connection) {
    if (!connection) {
        throw DatabaseError("Transaction needs a database connection");
    }

    if (sqlite3_exec(
            connection, "BEGIN IMMEDIATE;", nullptr, nullptr, nullptr
        ) != SQLITE_OK) {
        throw DatabaseError(
            std::string("Could not start transaction: ") +
            sqlite3_errmsg(connection)
        );
    }

    active = true;
}

Transaction::~Transaction() {
    if (active) {
        sqlite3_exec(connection, "ROLLBACK;", nullptr, nullptr, nullptr);
    }
}

void Transaction::commit() {
    if (!active) {
        return;
    }

    if (sqlite3_exec(
            connection, "COMMIT;", nullptr, nullptr, nullptr
        ) != SQLITE_OK) {
        // Leave active = true so the destructor rolls back.
        throw DatabaseError(
            std::string("Could not commit transaction: ") +
            sqlite3_errmsg(connection)
        );
    }

    active = false;
}
