#pragma once

#include "models/Availability.h"
#include "repositories/Repository.h"

#include <optional>
#include <string>
#include <vector>

// Data access for journeys and their seats.
class JourneyRepository : public Repository {
public:
    struct FreeSeat {
        long long seatId;
        int seatNumber;
    };

    explicit JourneyRepository(sqlite3* connection)
        : Repository(connection) {}

    // Journey + train + seat counters matching a route search.
    // Empty source/destination/travelClass mean "any".
    std::vector<Availability> search(
        const std::string& source,
        const std::string& destination,
        const std::string& journeyDate,
        const std::string& travelClass
    ) const;

    // Exact (train number, date, class) lookup.
    std::optional<Availability> findByKey(
        const std::string& trainNumber,
        const std::string& journeyDate,
        const std::string& travelClass
    ) const;

    // Distinct travel classes that actually have journeys scheduled.
    std::vector<std::string> getTravelClasses() const;

    // Creates any missing seat rows up to the journey capacity so
    // seat numbers stay unique (guarded by UNIQUE(journey_id,
    // seat_number) in the database).
    void ensureSeats(long long journeyId, int capacity) const;

    // Lowest-numbered seat without a confirmed passenger.
    std::optional<FreeSeat> findFirstFreeSeat(
        long long journeyId
    ) const;
};
