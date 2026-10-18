#pragma once

#include "models/Booking.h"
#include "repositories/Repository.h"

#include <optional>
#include <string>

// Data access for bookings, passengers and seat reassignment.
class BookingRepository : public Repository {
public:
    // Row snapshot needed before cancelling a booking.
    struct CancellationRecord {
        long long bookingId;
        long long journeyId;
        std::string status;
        bool hasSeat;
        long long seatId;
    };

    // Oldest waitlisted booking on a journey (FIFO promotion).
    struct WaitlistCandidate {
        long long bookingId;
        long long passengerId;
        std::string pnr;
    };

    explicit BookingRepository(sqlite3* connection)
        : Repository(connection) {}

    bool pnrExists(const std::string& pnr) const;

    long long insertBooking(long long journeyId,
                            const std::string& pnr,
                            const std::string& status,
                            long long totalFarePaise);

    void insertPassenger(long long bookingId,
                         const std::string& name,
                         int age,
                         const std::optional<long long>& seatId,
                         const std::string& status);

    // Full booking detail (journey + train + passenger + seat).
    std::optional<Booking> findByPnr(const std::string& pnr) const;

    std::optional<CancellationRecord> findForCancellation(
        const std::string& pnr
    ) const;

    // Marks booking and its passengers cancelled and frees the seat.
    void markCancelled(long long bookingId);

    std::optional<WaitlistCandidate> findEarliestWaitlisted(
        long long journeyId
    ) const;

    std::optional<int> getSeatNumber(long long seatId) const;

    // Assigns a freed seat to a waitlisted passenger.
    // Returns false when the row did not match anymore.
    bool promotePassenger(long long passengerId, long long seatId);

    bool confirmBooking(long long bookingId);
};
