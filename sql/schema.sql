PRAGMA foreign_keys = ON;

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
    base_fare_paise INTEGER NOT NULL CHECK(base_fare_paise >= 0),
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
    total_fare_paise INTEGER NOT NULL CHECK(total_fare_paise >= 0),
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