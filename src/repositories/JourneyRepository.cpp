#include "repositories/JourneyRepository.h"

namespace {

// Shared SELECT used by search() and findByKey(): the journey, its
// train and the seat counters, all inside one statement.
const char* availabilitySql = R"SQL(
    SELECT
        j.id,
        j.train_id,
        j.journey_date,
        j.travel_class,
        j.capacity,
        j.base_fare_paise,
        t.train_number,
        t.name,
        t.source,
        t.destination,
        (
            SELECT COUNT(*)
            FROM passengers p
            JOIN seats s2 ON s2.id = p.seat_id
            WHERE s2.journey_id = j.id
              AND p.status = 'CONFIRMED'
        ) AS confirmed_seats,
        (
            SELECT COUNT(*)
            FROM bookings b
            WHERE b.journey_id = j.id
              AND b.status = 'WAITLISTED'
        ) AS waitlisted_bookings
    FROM journeys j
    JOIN trains t ON t.id = j.train_id
)SQL";

Availability readAvailability(const Statement& statement) {
    Journey journey(
        statement.columnInt(0),
        statement.columnInt(1),
        statement.columnText(2),
        statement.columnText(3),
        statement.columnInt(4),
        statement.columnInt64(5)
    );

    Train train(
        journey.getTrainId(),
        statement.columnText(6),
        statement.columnText(7),
        statement.columnText(8),
        statement.columnText(9)
    );

    return Availability(
        std::move(journey),
        std::move(train),
        statement.columnInt(10),
        statement.columnInt(11)
    );
}

}  // namespace

std::vector<Availability> JourneyRepository::search(
    const std::string& source,
    const std::string& destination,
    const std::string& journeyDate,
    const std::string& travelClass
) const {
    std::string sql = availabilitySql;
    std::string where = " WHERE j.journey_date = ?";

    if (!source.empty()) {
        where += " AND LOWER(t.source) = LOWER(?)";
    }
    if (!destination.empty()) {
        where += " AND LOWER(t.destination) = LOWER(?)";
    }
    if (!travelClass.empty()) {
        where += " AND j.travel_class = ?";
    }

    sql += where + " ORDER BY t.train_number, j.travel_class;";

    auto statement = prepare(sql.c_str());

    int index = 1;
    statement.bindText(index++, journeyDate);
    if (!source.empty()) statement.bindText(index++, source);
    if (!destination.empty()) statement.bindText(index++, destination);
    if (!travelClass.empty()) statement.bindText(index++, travelClass);

    std::vector<Availability> results;

    while (statement.step() == SQLITE_ROW) {
        results.push_back(readAvailability(statement));
    }

    return results;
}

std::optional<Availability> JourneyRepository::findByKey(
    const std::string& trainNumber,
    const std::string& journeyDate,
    const std::string& travelClass
) const {
    auto statement = prepare(
        (std::string(availabilitySql) +
         " WHERE t.train_number = ?"
         "   AND j.journey_date = ?"
         "   AND j.travel_class = ?;"
        ).c_str()
    );

    statement.bindText(1, trainNumber);
    statement.bindText(2, journeyDate);
    statement.bindText(3, travelClass);

    if (statement.step() != SQLITE_ROW) {
        return std::nullopt;
    }

    return readAvailability(statement);
}

std::vector<std::string> JourneyRepository::getTravelClasses() const {
    auto statement = prepare(
        "SELECT DISTINCT travel_class FROM journeys "
        "ORDER BY travel_class;"
    );

    std::vector<std::string> classes;

    while (statement.step() == SQLITE_ROW) {
        classes.push_back(statement.columnText(0));
    }

    return classes;
}

void JourneyRepository::ensureSeats(
    long long journeyId,
    int capacity
) const {
    if (capacity <= 0) {
        return;
    }

    auto statement = prepare(R"SQL(
        INSERT OR IGNORE INTO seats (journey_id, seat_number)
        WITH RECURSIVE numbers(n) AS (
            SELECT 1
            UNION ALL
            SELECT n + 1 FROM numbers WHERE n < ?
        )
        SELECT ?, n FROM numbers WHERE n <= ?;
    )SQL");

    statement.bindInt(1, capacity);
    statement.bindInt64(2, journeyId);
    statement.bindInt(3, capacity);

    statement.step();
}

std::optional<JourneyRepository::FreeSeat>
JourneyRepository::findFirstFreeSeat(long long journeyId) const {
    auto statement = prepare(R"SQL(
        SELECT s.id, s.seat_number
        FROM seats s
        LEFT JOIN passengers p
          ON p.seat_id = s.id
         AND p.status = 'CONFIRMED'
        WHERE s.journey_id = ?
          AND p.id IS NULL
        ORDER BY s.seat_number
        LIMIT 1;
    )SQL");

    statement.bindInt64(1, journeyId);

    if (statement.step() != SQLITE_ROW) {
        return std::nullopt;
    }

    return FreeSeat{
        statement.columnInt64(0),
        statement.columnInt(1)
    };
}
