
#pragma once

#include <string>

class Train {
private:
    int id;
    std::string trainNumber;
    std::string name;
    std::string source;
    std::string destination;

public:
    Train(int id,
          std::string trainNumber,
          std::string name,
          std::string source,
          std::string destination)
        : id(id),
          trainNumber(std::move(trainNumber)),
          name(std::move(name)),
          source(std::move(source)),
          destination(std::move(destination)) {}

    int getId() const { return id; }
    const std::string& getTrainNumber() const { return trainNumber; }
    const std::string& getName() const { return name; }
    const std::string& getSource() const { return source; }
    const std::string& getDestination() const { return destination; }
};
