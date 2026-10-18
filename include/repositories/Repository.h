#pragma once

#include "database/Statement.h"

#include <sqlite3.h>

// Base class for every repository.
//
// It owns nothing but holds the shared SQLite connection and provides
// the prepare() helper, so derived repositories (TrainRepository,
// JourneyRepository, BookingRepository) only contain their own SQL.
// The virtual destructor makes deletion through a base pointer safe,
// which is the project's inheritance example.
class Repository {
protected:
    sqlite3* connection;

    explicit Repository(sqlite3* connection);

    Statement prepare(const char* sql) const;

public:
    virtual ~Repository() = default;

    // Needed by services that open transactions around several
    // repository calls.
    sqlite3* getConnection() const { return connection; }

    Repository(const Repository&) = delete;
    Repository& operator=(const Repository&) = delete;
};
