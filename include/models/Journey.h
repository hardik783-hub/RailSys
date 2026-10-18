#pragma once

#include <string>

// A scheduled journey: one train on one date in one travel class.
class Journey {
private:
    int id;
    int trainId;
    std::string journeyDate;
    std::string travelClass;
    int capacity;
    long long baseFarePaise;

public:
    Journey(int id,
            int trainId,
            std::string journeyDate,
            std::string travelClass,
            int capacity,
            long long baseFarePaise)
        : id(id),
          trainId(trainId),
          journeyDate(std::move(journeyDate)),
          travelClass(std::move(travelClass)),
          capacity(capacity),
          baseFarePaise(baseFarePaise) {}

    int getId() const { return id; }
    int getTrainId() const { return trainId; }
    const std::string& getJourneyDate() const { return journeyDate; }
    const std::string& getTravelClass() const { return travelClass; }
    int getCapacity() const { return capacity; }
    long long getBaseFarePaise() const { return baseFarePaise; }

    double getBaseFareRupees() const {
        return static_cast<double>(baseFarePaise) / 100.0;
    }
};
