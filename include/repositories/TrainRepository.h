#pragma once

#include "models/Train.h"
#include "repositories/Repository.h"

#include <string>
#include <vector>

class TrainRepository : public Repository {
public:
    explicit TrainRepository(sqlite3* connection)
        : Repository(connection) {}

    std::vector<Train> getAllTrains() const;

    // Distinct station names (sources and destinations merged),
    // used to populate the origin/destination pickers.
    std::vector<std::string> getStations() const;
};
