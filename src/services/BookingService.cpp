#include "services/BookingService.h"

#include "app/AppError.h"
#include "app/Validation.h"
#include "database/Transaction.h"

#include <sqlite3.h>

#include <algorithm>
#include <cctype>
#include <cstdint>
#include <iomanip>
#include <sstream>

namespace {

std::string normalizePnr(const std::string& input) {
    std::string pnr = validation::trim(input);

    std::transform(
        pnr.begin(), pnr.end(), pnr.begin(),
        [](unsigned char character) {
            return static_cast<char>(std::toupper(character));
        }
    );

    return pnr;
}

}  // namespace

std::string BookingService::generatePnr() const {
    // sqlite3_randomness is seeded from the OS, and the loop plus the
    // UNIQUE constraint on bookings.pnr guarantee uniqueness.
    for (int attempt = 0; attempt < 20; ++attempt) {
        std::uint32_t randomValue = 0;
        sqlite3_randomness(sizeof(randomValue), &randomValue);

        std::ostringstream out;
        out << "RS" << std::uppercase << std::hex
            << std::setw(8) << std::setfill('0') << randomValue;

        const std::string pnr = out.str();

        if (!bookingRepository.pnrExists(pnr)) {
            return pnr;
        }
    }

    throw DatabaseError("Could not generate a unique PNR");
}

Booking BookingService::createBooking(const BookingRequest& request) {
    const std::string name = validation::trim(request.passengerName);

    if (request.trainNumber.empty() ||
        request.travelClass.empty()) {
        throw ValidationError(
            "trainNumber and travelClass are required"
        );
    }

    if (request.journeyDate.empty() ||
        !validation::isDate(request.journeyDate)) {
        throw ValidationError(
            "journeyDate must be a valid date in YYYY-MM-DD format"
        );
    }

    if (name.empty()) {
        throw ValidationError("Passenger name is required");
    }

    if (name.size() > 80) {
        throw ValidationError(
            "Passenger name must be 80 characters or fewer"
        );
    }

    if (request.passengerAge < 0 || request.passengerAge > 120) {
        throw ValidationError(
            "Passenger age must be between 0 and 120"
        );
    }

    Transaction transaction(bookingRepository.getConnection());

    const auto availability = journeyRepository.findByKey(
        request.trainNumber,
        request.journeyDate,
        request.travelClass
    );

    if (!availability.has_value()) {
        throw NotFoundError(
            "No journey found for train " + request.trainNumber +
            " on " + request.journeyDate +
            " in class " + request.travelClass
        );
    }

    const Journey& journey = availability->getJourney();

    // Make sure every seat row exists before allocating one.
    journeyRepository.ensureSeats(
        journey.getId(), journey.getCapacity()
    );

    const auto freeSeat =
        journeyRepository.findFirstFreeSeat(journey.getId());

    const std::string status =
        freeSeat.has_value() ? "CONFIRMED" : "WAITLISTED";

    const std::string pnr = generatePnr();

    const long long bookingId = bookingRepository.insertBooking(
        journey.getId(),
        pnr,
        status,
        journey.getBaseFarePaise()
    );

    bookingRepository.insertPassenger(
        bookingId,
        name,
        request.passengerAge,
        freeSeat.has_value()
            ? std::optional<long long>(freeSeat->seatId)
            : std::nullopt,
        status
    );

    transaction.commit();

    Booking booking;
    booking.setId(bookingId);
    booking.setJourneyId(journey.getId());
    booking.setPnr(pnr);
    booking.setStatus(status);
    booking.setTotalFarePaise(journey.getBaseFarePaise());

    const Train& train = availability->getTrain();
    booking.setTrainNumber(train.getTrainNumber());
    booking.setTrainName(train.getName());
    booking.setSource(train.getSource());
    booking.setDestination(train.getDestination());
    booking.setJourneyDate(journey.getJourneyDate());
    booking.setTravelClass(journey.getTravelClass());

    booking.setPassengerName(name);
    booking.setPassengerAge(request.passengerAge);
    booking.setSeatNumber(
        freeSeat.has_value() ? freeSeat->seatNumber : -1
    );
    booking.setPassengerStatus(status);

    return booking;
}

Booking BookingService::getBooking(const std::string& pnr) const {
    const std::string normalized = normalizePnr(pnr);

    if (!validation::isPnr(normalized)) {
        throw ValidationError(
            "Invalid PNR format. Expected e.g. RS384B3B9B"
        );
    }

    const auto booking =
        bookingRepository.findByPnr(normalized);

    if (!booking.has_value()) {
        throw NotFoundError("PNR " + normalized + " was not found");
    }

    return *booking;
}

BookingService::CancelResult BookingService::cancelBooking(
    const std::string& pnr
) {
    const std::string normalized = normalizePnr(pnr);

    if (!validation::isPnr(normalized)) {
        throw ValidationError(
            "Invalid PNR format. Expected e.g. RS384B3B9B"
        );
    }

    Transaction transaction(bookingRepository.getConnection());

    const auto record =
        bookingRepository.findForCancellation(normalized);

    if (!record.has_value()) {
        throw NotFoundError(
            "PNR " + normalized + " was not found"
        );
    }

    if (record->status == "CANCELLED") {
        throw ConflictError(
            "Booking " + normalized + " is already cancelled"
        );
    }

    bookingRepository.markCancelled(record->bookingId);

    CancelResult result;
    result.pnr = normalized;
    result.waitlistPromoted = false;
    result.promotedSeatNumber = 0;

    // Only a confirmed booking frees a seat, and the freed seat goes
    // to the earliest waitlisted booking (FIFO) on the same journey.
    if (record->status == "CONFIRMED" && record->hasSeat) {
        const auto candidate = bookingRepository
            .findEarliestWaitlisted(record->journeyId);

        if (candidate.has_value()) {
            const auto seatNumber =
                bookingRepository.getSeatNumber(record->seatId);

            if (!seatNumber.has_value()) {
                throw DatabaseError(
                    "Released seat disappeared during promotion"
                );
            }

            if (!bookingRepository.promotePassenger(
                    candidate->passengerId, record->seatId)) {
                throw DatabaseError(
                    "Could not promote waitlisted passenger"
                );
            }

            if (!bookingRepository.confirmBooking(
                    candidate->bookingId)) {
                throw DatabaseError(
                    "Could not confirm waitlisted booking"
                );
            }

            result.waitlistPromoted = true;
            result.promotedPnr = candidate->pnr;
            result.promotedSeatNumber = *seatNumber;
        }
    }

    transaction.commit();

    return result;
}
