# RailSys — Railway Reservation System

A college demo railway reservation web application built with **C++17**, the
**Crow** HTTP framework, **SQLite**, CMake, and a vanilla HTML/CSS/JS frontend.

> **Honest disclaimer:** all trains, schedules, fares, seats and bookings are
> **simulated demo data** stored in a local SQLite database. RailSys does *not*
> fetch live Indian Railways data, and no real ticket can be booked.

## Quick start

```bash
# 1. Configure and build (already configured in build/ if present)
cmake -S . -B build
cmake --build build -j4

# 2. Run the server from the project root
./build/railsys

# 3. Open the website
#    http://127.0.0.1:18080
```

The first run creates `data/railsys.db` and seeds the demo schedule
(6 trains across Wardha, Nagpur and Amravati; the next 10 days;
Sleeper and AC 3 Tier). Existing bookings are never overwritten —
seeding uses `INSERT OR IGNORE` only.

## Tests

```bash
# C++ unit tests: seat allocation, waitlisting, FIFO promotion,
# duplicate cancellation, validation  (61 checks)
./build/railsys_tests
# or: cd build && ctest --output-on-failure

# End-to-end API tests over HTTP. Spawns a *temporary* server with a
# fresh isolated database, so it never touches data/railsys.db. (37 checks)
python3 tests/e2e_api_test.py

# Frontend smoke test. Runs the real public/app.js in Node with a DOM
# shim and mocked API responses: boot, search, booking, confirmation,
# PNR lookup, cancellation dialog, promotion notice. (37 checks)
node tests/frontend_smoke_test.js
```

## Architecture

```
src/
  main.cpp                 # wiring only: database → repos → services → routes
  app/AppError.h           # typed errors (400/404/409/500) via inheritance
  database/                # Database, RAII Statement & Transaction
  repositories/            # SQL only: Train, Journey, Booking (+ base Repository)
  services/                # business logic: AvailabilityService, BookingService
  routes/                  # HTTP layer: ApiRoutes, StaticRoutes
include/                   # headers mirroring src/
public/                    # frontend: index.html, style.css, app.js
sql/schema.sql             # database schema reference
tests/                     # C++ unit tests + Python e2e tests
```

OOP concepts appear where they earn their place: encapsulation in the model
classes, inheritance in `Repository` and the `AppError` hierarchy, and runtime
polymorphism when routes catch `AppError` and map `virtual httpStatus()` to
HTTP codes. Transactions use RAII (`BEGIN IMMEDIATE … COMMIT`, rollback in the
destructor), and a process-wide mutex serializes database access.

## HTTP API

| Method | Path | Description |
| ------ | ---- | ----------- |
| GET | `/api/health` | Health check |
| GET | `/api/meta` | Stations, travel classes, train count |
| GET | `/api/trains` | All demo trains |
| GET | `/api/search?source=&destination=&journeyDate=&travelClass=` | Trains + availability + fare on a route (all filters optional except date) |
| GET | `/api/availability?trainNumber=&journeyDate=&travelClass=` | One journey's availability |
| POST | `/api/bookings` | Create booking `{trainNumber, journeyDate, travelClass, passengerName, passengerAge}` → 201 with PNR |
| GET | `/api/bookings/{PNR}` | Booking details by PNR |
| POST | `/api/bookings/{PNR}/cancel` | Cancel; promotes the earliest waitlisted booking (FIFO) |

Errors return `{"error": "..."}` with 400 (invalid input), 404 (not found) or
409 (already cancelled).

## Booking rules

- Lowest free seat number is allotted; seat uniqueness is enforced by a
  partial unique index on `passengers(seat_id)`.
- Full coach → booking is **WAITLISTED** (no seat).
- Cancelling a **CONFIRMED** booking frees the seat and promotes the
  **earliest** waitlisted booking for that journey.
- Cancelling twice returns `409`; cancelled bookings stay readable.
