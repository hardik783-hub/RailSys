#pragma once

#include "models/Journey.h"
#include "models/Train.h"

// Seat availability snapshot for one journey.
//
// The journey capacity is the authoritative coach size; seat rows are
// created lazily by the booking flow. Values are read inside a single
// SQL query, so the numbers are internally consistent.
class Availability {
private:
    Journey journey;
    Train train;
    int confirmedSeats;
    int waitlistedBookings;

public:
    Availability(Journey journey,
                 Train train,
                 int confirmedSeats,
                 int waitlistedBookings)
        : journey(std::move(journey)),
          train(std::move(train)),
          confirmedSeats(confirmedSeats),
          waitlistedBookings(waitlistedBookings) {}

    const Journey& getJourney() const { return journey; }
    const Train& getTrain() const { return train; }

    int getTotalSeats() const { return journey.getCapacity(); }
    int getConfirmedSeats() const { return confirmedSeats; }
    int getWaitlistedBookings() const { return waitlistedBookings; }

    int getAvailableSeats() const {
        const int free = getTotalSeats() - confirmedSeats;
        return free > 0 ? free : 0;
    }

    bool isFull() const { return getAvailableSeats() == 0; }
};
