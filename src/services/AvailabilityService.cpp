#include "services/AvailabilityService.h"

#include "app/AppError.h"
#include "app/Validation.h"

AvailabilityService::Meta AvailabilityService::getMeta() const {
    auto trains = trainRepository.getAllTrains();

    return Meta{
        trainRepository.getStations(),
        journeyRepository.getTravelClasses(),
        static_cast<int>(trains.size())
    };
}

std::vector<Train> AvailabilityService::listTrains() const {
    return trainRepository.getAllTrains();
}

std::vector<Availability> AvailabilityService::search(
    const std::string& source,
    const std::string& destination,
    const std::string& journeyDate,
    const std::string& travelClass
) const {
    if (journeyDate.empty() || !validation::isDate(journeyDate)) {
        throw ValidationError(
            "journeyDate must be a valid date in YYYY-MM-DD format"
        );
    }

    return journeyRepository.search(
        validation::trim(source),
        validation::trim(destination),
        journeyDate,
        validation::trim(travelClass)
    );
}

Availability AvailabilityService::getAvailability(
    const std::string& trainNumber,
    const std::string& journeyDate,
    const std::string& travelClass
) const {
    if (trainNumber.empty() || travelClass.empty()) {
        throw ValidationError(
            "trainNumber and travelClass are required"
        );
    }

    if (journeyDate.empty() || !validation::isDate(journeyDate)) {
        throw ValidationError(
            "journeyDate must be a valid date in YYYY-MM-DD format"
        );
    }

    const auto availability = journeyRepository.findByKey(
        trainNumber, journeyDate, travelClass
    );

    if (!availability.has_value()) {
        throw NotFoundError(
            "No journey found for train " + trainNumber +
            " on " + journeyDate + " in class " + travelClass
        );
    }

    return *availability;
}
