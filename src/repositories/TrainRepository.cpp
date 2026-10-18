#include "repositories/TrainRepository.h"

std::vector<Train> TrainRepository::getAllTrains() const {
    auto statement = prepare(
        "SELECT id, train_number, name, source, destination "
        "FROM trains ORDER BY train_number;"
    );

    std::vector<Train> trains;

    while (statement.step() == SQLITE_ROW) {
        trains.emplace_back(
            statement.columnInt(0),
            statement.columnText(1),
            statement.columnText(2),
            statement.columnText(3),
            statement.columnText(4)
        );
    }

    return trains;
}

std::vector<std::string> TrainRepository::getStations() const {
    auto statement = prepare(
        "SELECT DISTINCT station FROM ("
        "    SELECT source AS station FROM trains "
        "    UNION "
        "    SELECT destination AS station FROM trains"
        ") ORDER BY station;"
    );

    std::vector<std::string> stations;

    while (statement.step() == SQLITE_ROW) {
        stations.push_back(statement.columnText(0));
    }

    return stations;
}
