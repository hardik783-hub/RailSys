#include "repositories/BookingRepository.h"

namespace {

// Builds a Booking from the shared detail query:
//   0 b.id, 1 b.pnr, 2 b.status, 3 b.total_fare_paise,
//   4 b.created_at, 5 b.journey_id, 6 j.journey_date,
//   7 j.travel_class, 8 t.train_number, 9 t.name,
//   10 t.source, 11 t.destination, 12 p.name, 13 p.age,
//   14 p.status, 15 s.seat_number
Booking readBooking(const Statement& statement) {
    Booking booking;
    booking.setId(statement.columnInt64(0));
    booking.setPnr(statement.columnText(1));
    booking.setStatus(statement.columnText(2));
    booking.setTotalFarePaise(statement.columnInt64(3));
    booking.setCreatedAt(statement.columnText(4));
    booking.setJourneyId(statement.columnInt64(5));
    booking.setJourneyDate(statement.columnText(6));
    booking.setTravelClass(statement.columnText(7));
    booking.setTrainNumber(statement.columnText(8));
    booking.setTrainName(statement.columnText(9));
    booking.setSource(statement.columnText(10));
    booking.setDestination(statement.columnText(11));
    booking.setPassengerName(statement.columnText(12));
    booking.setPassengerAge(statement.columnInt(13));
    booking.setPassengerStatus(statement.columnText(14));
    booking.setSeatNumber(
        statement.isNull(15) ? -1 : statement.columnInt(15)
    );

    return booking;
}

const char* detailSelect = R"SQL(
    SELECT
        b.id, b.pnr, b.status, b.total_fare_paise, b.created_at,
        b.journey_id, j.journey_date, j.travel_class,
        t.train_number, t.name, t.source, t.destination,
        p.name, p.age, p.status, s.seat_number
    FROM bookings b
    JOIN journeys j ON j.id = b.journey_id
    JOIN trains t ON t.id = j.train_id
    JOIN passengers p ON p.booking_id = b.id
    LEFT JOIN seats s ON s.id = p.seat_id
)SQL";

}  // namespace

bool BookingRepository::pnrExists(const std::string& pnr) const {
    auto statement = prepare(
        "SELECT 1 FROM bookings WHERE pnr = ? LIMIT 1;"
    );

    statement.bindText(1, pnr);

    return statement.step() == SQLITE_ROW;
}

long long BookingRepository::insertBooking(
    long long journeyId,
    const std::string& pnr,
    const std::string& status,
    long long totalFarePaise
) {
    auto statement = prepare(
        "INSERT INTO bookings "
        "    (pnr, journey_id, status, total_fare_paise) "
        "VALUES (?, ?, ?, ?);"
    );

    statement.bindText(1, pnr);
    statement.bindInt64(2, journeyId);
    statement.bindText(3, status);
    statement.bindInt64(4, totalFarePaise);
    statement.step();

    return sqlite3_last_insert_rowid(connection);
}

void BookingRepository::insertPassenger(
    long long bookingId,
    const std::string& name,
    int age,
    const std::optional<long long>& seatId,
    const std::string& status
) {
    auto statement = prepare(
        "INSERT INTO passengers "
        "    (booking_id, name, age, seat_id, status) "
        "VALUES (?, ?, ?, ?, ?);"
    );

    statement.bindInt64(1, bookingId);
    statement.bindText(2, name);
    statement.bindInt(3, age);

    if (seatId.has_value()) {
        statement.bindInt64(4, *seatId);
    } else {
        statement.bindNull(4);
    }

    statement.bindText(5, status);
    statement.step();
}

std::optional<Booking> BookingRepository::findByPnr(
    const std::string& pnr
) const {
    auto statement = prepare(
        (std::string(detailSelect) + " WHERE b.pnr = ?;").c_str()
    );

    statement.bindText(1, pnr);

    if (statement.step() != SQLITE_ROW) {
        return std::nullopt;
    }

    return readBooking(statement);
}

std::optional<BookingRepository::CancellationRecord>
BookingRepository::findForCancellation(
    const std::string& pnr
) const {
    auto statement = prepare(
        "SELECT b.id, b.journey_id, b.status, p.seat_id "
        "FROM bookings b "
        "JOIN passengers p ON p.booking_id = b.id "
        "WHERE b.pnr = ? "
        "LIMIT 1;"
    );

    statement.bindText(1, pnr);

    if (statement.step() != SQLITE_ROW) {
        return std::nullopt;
    }

    return CancellationRecord{
        statement.columnInt64(0),
        statement.columnInt64(1),
        statement.columnText(2),
        !statement.isNull(3),
        statement.isNull(3) ? 0 : statement.columnInt64(3)
    };
}

void BookingRepository::markCancelled(long long bookingId) {
    // The seat is released before the status flips so the partial
    // unique index (unique_confirmed_seat) allows a waitlisted
    // passenger to take the seat immediately afterwards.
    auto passenger = prepare(
        "UPDATE passengers "
        "SET status = 'CANCELLED', seat_id = NULL "
        "WHERE booking_id = ?;"
    );

    passenger.bindInt64(1, bookingId);
    passenger.step();

    auto booking = prepare(
        "UPDATE bookings SET status = 'CANCELLED' WHERE id = ?;"
    );

    booking.bindInt64(1, bookingId);
    booking.step();
}

std::optional<BookingRepository::WaitlistCandidate>
BookingRepository::findEarliestWaitlisted(
    long long journeyId
) const {
    auto statement = prepare(
        "SELECT b.id, p.id, b.pnr "
        "FROM bookings b "
        "JOIN passengers p ON p.booking_id = b.id "
        "WHERE b.journey_id = ? "
        "  AND b.status = 'WAITLISTED' "
        "  AND p.status = 'WAITLISTED' "
        "ORDER BY b.id ASC "
        "LIMIT 1;"
    );

    statement.bindInt64(1, journeyId);

    if (statement.step() != SQLITE_ROW) {
        return std::nullopt;
    }

    return WaitlistCandidate{
        statement.columnInt64(0),
        statement.columnInt64(1),
        statement.columnText(2)
    };
}

std::optional<int> BookingRepository::getSeatNumber(
    long long seatId
) const {
    auto statement = prepare(
        "SELECT seat_number FROM seats WHERE id = ?;"
    );

    statement.bindInt64(1, seatId);

    if (statement.step() != SQLITE_ROW) {
        return std::nullopt;
    }

    return statement.columnInt(0);
}

bool BookingRepository::promotePassenger(
    long long passengerId,
    long long seatId
) {
    auto statement = prepare(
        "UPDATE passengers "
        "SET status = 'CONFIRMED', seat_id = ? "
        "WHERE id = ? AND status = 'WAITLISTED';"
    );

    statement.bindInt64(1, seatId);
    statement.bindInt64(2, passengerId);
    statement.step();

    return sqlite3_changes(connection) == 1;
}

bool BookingRepository::confirmBooking(long long bookingId) {
    auto statement = prepare(
        "UPDATE bookings "
        "SET status = 'CONFIRMED' "
        "WHERE id = ? AND status = 'WAITLISTED';"
    );

    statement.bindInt64(1, bookingId);
    statement.step();

    return sqlite3_changes(connection) == 1;
}
