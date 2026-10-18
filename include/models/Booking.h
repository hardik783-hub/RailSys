#pragma once

#include <string>

// A booking as returned to callers: the reservation itself plus the
// single passenger it carries (the demo supports one passenger per
// booking) and the journey details needed by the UI.
class Booking {
private:
    long long id;
    long long journeyId;
    std::string pnr;
    std::string status;  // CONFIRMED | WAITLISTED | CANCELLED
    long long totalFarePaise;
    std::string createdAt;

    std::string trainNumber;
    std::string trainName;
    std::string source;
    std::string destination;
    std::string journeyDate;
    std::string travelClass;

    std::string passengerName;
    int passengerAge;
    int seatNumber;  // -1 when no seat is assigned
    std::string passengerStatus;

public:
    Booking()
        : id(0),
          journeyId(0),
          totalFarePaise(0),
          passengerAge(0),
          seatNumber(-1) {}

    long long getId() const { return id; }
    long long getJourneyId() const { return journeyId; }
    const std::string& getPnr() const { return pnr; }
    const std::string& getStatus() const { return status; }
    long long getTotalFarePaise() const { return totalFarePaise; }
    const std::string& getCreatedAt() const { return createdAt; }

    const std::string& getTrainNumber() const { return trainNumber; }
    const std::string& getTrainName() const { return trainName; }
    const std::string& getSource() const { return source; }
    const std::string& getDestination() const { return destination; }
    const std::string& getJourneyDate() const { return journeyDate; }
    const std::string& getTravelClass() const { return travelClass; }

    const std::string& getPassengerName() const { return passengerName; }
    int getPassengerAge() const { return passengerAge; }
    int getSeatNumber() const { return seatNumber; }
    bool hasSeat() const { return seatNumber >= 0; }
    const std::string& getPassengerStatus() const {
        return passengerStatus;
    }

    double getTotalFareRupees() const {
        return static_cast<double>(totalFarePaise) / 100.0;
    }

    // Field setters are intentionally non-public in spirit: only the
    // repository/service layer builds a Booking, callers only read.
    void setId(long long value) { id = value; }
    void setJourneyId(long long value) { journeyId = value; }
    void setPnr(std::string value) { pnr = std::move(value); }
    void setStatus(std::string value) { status = std::move(value); }
    void setTotalFarePaise(long long value) { totalFarePaise = value; }
    void setCreatedAt(std::string value) { createdAt = std::move(value); }

    void setTrainNumber(std::string value) {
        trainNumber = std::move(value);
    }
    void setTrainName(std::string value) { trainName = std::move(value); }
    void setSource(std::string value) { source = std::move(value); }
    void setDestination(std::string value) {
        destination = std::move(value);
    }
    void setJourneyDate(std::string value) {
        journeyDate = std::move(value);
    }
    void setTravelClass(std::string value) {
        travelClass = std::move(value);
    }

    void setPassengerName(std::string value) {
        passengerName = std::move(value);
    }
    void setPassengerAge(int value) { passengerAge = value; }
    void setSeatNumber(int value) { seatNumber = value; }
    void setPassengerStatus(std::string value) {
        passengerStatus = std::move(value);
    }
};
