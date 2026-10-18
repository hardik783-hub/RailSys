#include "database/Database.h"

#include <stdexcept>
#include <string>

namespace {
void execute(sqlite3* db, const char* sql) {
    char* error = nullptr;

    if (sqlite3_exec(db, sql, nullptr, nullptr, &error) != SQLITE_OK) {
        std::string message = error ? error : sqlite3_errmsg(db);
        sqlite3_free(error);
        throw std::runtime_error(message);
    }
}
}

Database::Database(const std::string& path) {
    if (sqlite3_open(path.c_str(), &connection) != SQLITE_OK) {
        std::string message =
            connection ? sqlite3_errmsg(connection)
                       : "Unable to allocate SQLite connection";

        if (connection) {
            sqlite3_close(connection);
            connection = nullptr;
        }

        throw std::runtime_error(message);
    }

    sqlite3_busy_timeout(connection, 5000);

    // SQLite foreign-key enforcement is enabled per connection.
    execute(connection, "PRAGMA foreign_keys = ON;");
}

Database::~Database() {
    if (connection) {
        sqlite3_close(connection);
    }
}

sqlite3* Database::getConnection() const {
    return connection;
}

void Database::initializeSchema() {
    execute(connection, R"SQL(
        CREATE TABLE IF NOT EXISTS trains (
            id INTEGER PRIMARY KEY,
            train_number TEXT NOT NULL UNIQUE,
            name TEXT NOT NULL,
            source TEXT NOT NULL,
            destination TEXT NOT NULL
        );

        CREATE TABLE IF NOT EXISTS journeys (
            id INTEGER PRIMARY KEY,
            train_id INTEGER NOT NULL REFERENCES trains(id),
            journey_date TEXT NOT NULL,
            travel_class TEXT NOT NULL,
            capacity INTEGER NOT NULL CHECK(capacity >= 0),
            base_fare_paise INTEGER NOT NULL
                CHECK(base_fare_paise >= 0),
            UNIQUE(train_id, journey_date, travel_class)
        );

        CREATE TABLE IF NOT EXISTS seats (
            id INTEGER PRIMARY KEY,
            journey_id INTEGER NOT NULL REFERENCES journeys(id),
            seat_number INTEGER NOT NULL,
            UNIQUE(journey_id, seat_number)
        );

        CREATE TABLE IF NOT EXISTS bookings (
            id INTEGER PRIMARY KEY,
            pnr TEXT NOT NULL UNIQUE,
            journey_id INTEGER NOT NULL REFERENCES journeys(id),
            status TEXT NOT NULL CHECK(
                status IN ('CONFIRMED', 'WAITLISTED', 'CANCELLED')
            ),
            total_fare_paise INTEGER NOT NULL
                CHECK(total_fare_paise >= 0),
            created_at TEXT NOT NULL DEFAULT CURRENT_TIMESTAMP
        );

        CREATE TABLE IF NOT EXISTS passengers (
            id INTEGER PRIMARY KEY,
            booking_id INTEGER NOT NULL REFERENCES bookings(id),
            name TEXT NOT NULL,
            age INTEGER NOT NULL CHECK(age BETWEEN 0 AND 120),
            seat_id INTEGER REFERENCES seats(id),
            status TEXT NOT NULL CHECK(
                status IN ('CONFIRMED', 'WAITLISTED', 'CANCELLED')
            )
        );

        CREATE UNIQUE INDEX IF NOT EXISTS unique_confirmed_seat
        ON passengers(seat_id)
        WHERE seat_id IS NOT NULL AND status = 'CONFIRMED';
    )SQL");
}

// Seeds demo data. Every statement is INSERT OR IGNORE, so running it
// again never touches rows that already exist - existing bookings and
// journeys are preserved.
//
// NOTE: this is simulated railway data for a college demo. It is not
// live Indian Railways schedule, fare or inventory data.
void Database::seedDemoData() {
    execute(connection, R"SQL(
        INSERT OR IGNORE INTO trains
            (train_number, name, source, destination)
        VALUES
            ('12001', 'Demo Express', 'Wardha', 'Nagpur'),
            ('12002', 'Campus Express', 'Nagpur', 'Wardha'),
            ('12003', 'Heritage Link', 'Wardha', 'Amravati'),
            ('12004', 'Valley Rider', 'Amravati', 'Wardha'),
            ('12005', 'Deccan Express', 'Nagpur', 'Amravati'),
            ('12006', 'City Commuter', 'Amravati', 'Nagpur');

        -- Original demo journeys (small Sleeper coach, kept as-is so
        -- the full-train / waitlist scenario stays available).
        INSERT OR IGNORE INTO journeys
            (train_id, journey_date, travel_class, capacity,
             base_fare_paise)
        SELECT id, '2026-11-01', 'Sleeper', 3, 45000
        FROM trains
        WHERE train_number IN ('12001', '12002');

        -- Rolling schedule: every train runs on each of the next 10
        -- days in Sleeper and AC 3 Tier.
        WITH RECURSIVE
            days(n) AS (
                SELECT 1
                UNION ALL
                SELECT n + 1 FROM days WHERE n < 10
            ),
            classes(travel_class, capacity, fare_paise) AS (
                SELECT 'Sleeper', 6, 45000
                UNION ALL
                SELECT 'AC 3 Tier', 4, 85000
            )
        INSERT OR IGNORE INTO journeys
            (train_id, journey_date, travel_class, capacity,
             base_fare_paise)
        SELECT t.id,
               date('now', '+' || d.n || ' day'),
               c.travel_class,
               c.capacity,
               c.fare_paise
        FROM trains AS t
        JOIN days AS d
        JOIN classes AS c;

        -- One seat row per capacity slot for every journey.
        INSERT OR IGNORE INTO seats (journey_id, seat_number)
        WITH RECURSIVE numbers(n) AS (
            SELECT 1
            UNION ALL
            SELECT n + 1 FROM numbers WHERE n < 100
        )
        SELECT j.id, numbers.n
        FROM journeys AS j
        JOIN numbers ON numbers.n <= j.capacity;
    )SQL");
}
