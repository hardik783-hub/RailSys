#pragma once

#include "models/Booking.h"
#include "repositories/BookingRepository.h"
#include "repositories/JourneyRepository.h"

#include <string>

// Booking business logic: validation, seat allocation, PNR generation
// and cancellation with FIFO waitlist promotion.
//
// Every mutating operation runs inside one SQLite transaction
// (BEGIN IMMEDIATE ... COMMIT), and the process-wide database mutex in
// the route layer serializes access, so concurrent requests can never
// allocate the same seat twice (the partial unique index on
// passengers(seat_id) is the final guarantee).
class BookingService {
private:
    const JourneyRepository& journeyRepository;
    BookingRepository& bookingRepository;

public:
    struct BookingRequest {
        std::string trainNumber;
        std::string journeyDate;
        std::string travelClass;
        std::string passengerName;
        int passengerAge;
    };

    struct CancelResult {
        std::string pnr;
        bool waitlistPromoted;
        std::string promotedPnr;
        int promotedSeatNumber;
    };

    BookingService(const JourneyRepository& journeyRepository,
                   BookingRepository& bookingRepository)
        : journeyRepository(journeyRepository),
          bookingRepository(bookingRepository) {}

    Booking createBooking(const BookingRequest& request);

    Booking getBooking(const std::string& pnr) const;

    CancelResult cancelBooking(const std::string& pnr);

private:
    std::string generatePnr() const;
};
