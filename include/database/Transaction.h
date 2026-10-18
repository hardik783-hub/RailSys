#pragma once

#include <sqlite3.h>

// RAII wrapper around a SQLite transaction.
//
// The constructor starts BEGIN IMMEDIATE (so concurrent writers queue
// up instead of failing halfway), commit() ends the transaction, and
// the destructor rolls back anything still open. Services therefore
// never need manual ROLLBACK paths.
class Transaction {
private:
    sqlite3* connection = nullptr;
    bool active = false;

public:
    explicit Transaction(sqlite3* connection);
    ~Transaction();

    Transaction(const Transaction&) = delete;
    Transaction& operator=(const Transaction&) = delete;

    void commit();
    bool isActive() const { return active; }
};
