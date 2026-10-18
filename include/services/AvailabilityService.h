#pragma once

#include "models/Availability.h"
#include "models/Train.h"
#include "repositories/JourneyRepository.h"
#include "repositories/TrainRepository.h"

#include <string>
#include <vector>

// Read-only catalogue logic: train listing, route search and seat
// availability. Throws NotFoundError / ValidationError which the route
// layer converts into HTTP responses.
class AvailabilityService {
private:
    const TrainRepository& trainRepository;
    const JourneyRepository& journeyRepository;

public:
    struct Meta {
        std::vector<std::string> stations;
        std::vector<std::string> travelClasses;
        int trainCount;
    };

    AvailabilityService(const TrainRepository& trainRepository,
                        const JourneyRepository& journeyRepository)
        : trainRepository(trainRepository),
          journeyRepository(journeyRepository) {}

    Meta getMeta() const;

    std::vector<Train> listTrains() const;

    // Empty source/destination/travelClass mean "any".
    std::vector<Availability> search(
        const std::string& source,
        const std::string& destination,
        const std::string& journeyDate,
        const std::string& travelClass
    ) const;

    // Exact lookup; throws NotFoundError when the journey is missing.
    Availability getAvailability(
        const std::string& trainNumber,
        const std::string& journeyDate,
        const std::string& travelClass
    ) const;
};
