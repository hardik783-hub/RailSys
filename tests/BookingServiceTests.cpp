// Focused tests for the critical booking behaviour:
//   - seat allocation and seat uniqueness
//   - waitlist when a journey is full
//   - FIFO promotion on cancellation
//   - duplicate cancellation rejection
//   - input validation and error types
//
// The tests run against a temporary SQLite file, so they never touch
// the real demo database in data/.
//
// Build & run:  cmake --build build && ./build/railsys_tests
// or:           cd build && ctest --output-on-failure

#include "app/AppError.h"
#include "database/Database.h"
#include "repositories/BookingRepository.h"
#include "repositories/JourneyRepository.h"
#include "repositories/TrainRepository.h"
#include "services/AvailabilityService.h"
#include "services/BookingService.h"

#include <cstdio>
#include <filesystem>
#include <iostream>
#include <set>
#include <string>
#include <unistd.h>

namespace {

int checksRun = 0;
int checksFailed = 0;

#define CHECK(condition)                                                  \
    do {                                                                  \
        ++checksRun;                                                      \
        if (!(condition)) {                                               \
            ++checksFailed;                                               \
            std::cerr << "  FAIL line " << __LINE__ << ": "               \
                      << #condition << '\n';                              \
        }                                                                 \
    } while (false)

#define CHECK_THROWS(expression, ExceptionType)                           \
    do {                                                                  \
        ++checksRun;                                                      \
        bool caughtExpected = false;                                      \
        try {                                                             \
            (void)(expression);                                           \
        } catch (const ExceptionType&) {                                  \
            caughtExpected = true;                                        \
        } catch (const std::exception& error) {                           \
            std::cerr << "  FAIL line " << __LINE__                       \
                      << ": wrong exception: " << error.what() << '\n';   \
        }                                                                 \
        if (!caughtExpected) {                                            \
            ++checksFailed;                                               \
            std::cerr << "  FAIL line " << __LINE__ << ": "               \
                      << #expression << " did not throw "                 \
                      << #ExceptionType << '\n';                          \
        }                                                                 \
    } while (false)

// Wires up an isolated database with fresh repositories/services.
struct Fixture {
    std::filesystem::path dbPath;
    Database database;
    TrainRepository trains;
    JourneyRepository journeys;
    BookingRepository bookings;
    AvailabilityService availabilityService;
    BookingService bookingService;

    // Returns a brand-new temp path (removes any stale file first),
    // so Database::Database() always creates a fresh database.
    static std::filesystem::path freshPath(const std::string& name) {
        auto path = std::filesystem::temp_directory_path() /
                    ("railsys_test_" + name + "_" +
                     std::to_string(::getpid()) + ".db");
        std::remove(path.string().c_str());
        return path;
    }

    explicit Fixture(const std::string& name)
        : dbPath(freshPath(name)),
          database(dbPath.string()),
          trains(database.getConnection()),
          journeys(database.getConnection()),
          bookings(database.getConnection()),
          availabilityService(trains, journeys),
          bookingService(journeys, bookings) {
        database.initializeSchema();
    }

    ~Fixture() {
        std::remove(dbPath.string().c_str());
    }

    // Inserts a train + journey; seats are created automatically by
    // the service (ensureSeats) before allocation.
    long long addJourney(const std::string& trainNumber,
                         const std::string& date,
                         const std::string& travelClass,
                         int capacity,
                         long long farePaise,
                         const std::string& source = "Alpha",
                         const std::string& destination = "Beta") {
        std::string sql =
            "INSERT OR IGNORE INTO trains "
            "    (train_number, name, source, destination) "
            "VALUES ('" + trainNumber + "', 'Test Train', "
            "        '" + source + "', '" + destination + "');";

        char* error = nullptr;
        if (sqlite3_exec(
                database.getConnection(), sql.c_str(),
                nullptr, nullptr, &error) != SQLITE_OK) {
            throw std::runtime_error(error ? error : "insert failed");
        }

        sql =
            "INSERT INTO journeys "
            "    (train_id, journey_date, travel_class, capacity, "
            "     base_fare_paise) "
            "SELECT id, '" + date + "', '" + travelClass + "', " +
            std::to_string(capacity) + ", " +
            std::to_string(farePaise) +
            " FROM trains WHERE train_number = '" + trainNumber +
            "';";

        if (sqlite3_exec(
                database.getConnection(), sql.c_str(),
                nullptr, nullptr, &error) != SQLITE_OK) {
            throw std::runtime_error(error ? error : "insert failed");
        }

        return sqlite3_last_insert_rowid(database.getConnection());
    }

    static BookingService::BookingRequest request(
        const std::string& trainNumber,
        const std::string& date,
        const std::string& travelClass,
        const std::string& name,
        int age
    ) {
        BookingService::BookingRequest result;
        result.trainNumber = trainNumber;
        result.journeyDate = date;
        result.travelClass = travelClass;
        result.passengerName = name;
        result.passengerAge = age;
        return result;
    }
};

// ------------------------------------------------------------------
void testConfirmedBookingGetsSeat() {
    std::cout << "testConfirmedBookingGetsSeat\n";

    Fixture fixture("confirmed");
    fixture.addJourney("T100", "2026-12-01", "Sleeper", 2, 45000);

    const auto booking = fixture.bookingService.createBooking(
        Fixture::request("T100", "2026-12-01", "Sleeper", "Asha", 25)
    );

    CHECK(booking.getStatus() == "CONFIRMED");
    CHECK(booking.getSeatNumber() == 1);
    CHECK(booking.getTotalFarePaise() == 45000);
    CHECK(booking.getPnr().rfind("RS", 0) == 0);
    CHECK(booking.getPassengerName() == "Asha");

    // Looked up again through the repository layer.
    const auto found =
        fixture.bookingService.getBooking(booking.getPnr());

    CHECK(found.getPnr() == booking.getPnr());
    CHECK(found.getStatus() == "CONFIRMED");
    CHECK(found.getSeatNumber() == 1);
    CHECK(found.getTrainNumber() == "T100");
    CHECK(found.getTravelClass() == "Sleeper");
}

// ------------------------------------------------------------------
void testSeatsAreUniqueAndFullJourneyWaitlists() {
    std::cout << "testSeatsAreUniqueAndFullJourneyWaitlists\n";

    Fixture fixture("seats");
    fixture.addJourney("T200", "2026-12-02", "Sleeper", 2, 10000);

    const auto first = fixture.bookingService.createBooking(
        Fixture::request("T200", "2026-12-02", "Sleeper", "Ravi", 30)
    );
    const auto second = fixture.bookingService.createBooking(
        Fixture::request("T200", "2026-12-02", "Sleeper", "Meera", 28)
    );
    const auto third = fixture.bookingService.createBooking(
        Fixture::request("T200", "2026-12-02", "Sleeper", "Zoya", 19)
    );

    CHECK(first.getSeatNumber() == 1);
    CHECK(second.getSeatNumber() == 2);
    CHECK(first.getSeatNumber() != second.getSeatNumber());

    // Coach is full -> waitlisted, no seat assigned.
    CHECK(third.getStatus() == "WAITLISTED");
    CHECK(third.getSeatNumber() == -1);

    // Availability reflects the database, not guesses.
    const auto availability = fixture.availabilityService
        .getAvailability("T200", "2026-12-02", "Sleeper");

    CHECK(availability.getTotalSeats() == 2);
    CHECK(availability.getConfirmedSeats() == 2);
    CHECK(availability.getAvailableSeats() == 0);
    CHECK(availability.isFull());
    CHECK(availability.getWaitlistedBookings() == 1);

    // Every PNR is unique.
    std::set<std::string> pnrs{
        first.getPnr(), second.getPnr(), third.getPnr()};
    CHECK(pnrs.size() == 3);
}

// ------------------------------------------------------------------
void testCancellationPromotesEarliestWaitlisted() {
    std::cout << "testCancellationPromotesEarliestWaitlisted\n";

    Fixture fixture("promotion");
    fixture.addJourney("T300", "2026-12-03", "Sleeper", 1, 20000);

    const auto confirmed = fixture.bookingService.createBooking(
        Fixture::request("T300", "2026-12-03", "Sleeper", "Amit", 40)
    );
    const auto waitlistOne = fixture.bookingService.createBooking(
        Fixture::request("T300", "2026-12-03", "Sleeper", "Sunita", 36)
    );
    const auto waitlistTwo = fixture.bookingService.createBooking(
        Fixture::request("T300", "2026-12-03", "Sleeper", "Kabir", 9)
    );

    CHECK(confirmed.getStatus() == "CONFIRMED");
    CHECK(waitlistOne.getStatus() == "WAITLISTED");
    CHECK(waitlistTwo.getStatus() == "WAITLISTED");

    const auto result = fixture.bookingService
        .cancelBooking(confirmed.getPnr());

    CHECK(result.waitlistPromoted);
    CHECK(result.promotedPnr == waitlistOne.getPnr());
    CHECK(result.promotedSeatNumber == 1);

    // The earliest waitlisted passenger wins; the later one waits.
    CHECK(fixture.bookingService
              .getBooking(waitlistOne.getPnr())
              .getStatus() == "CONFIRMED");
    CHECK(fixture.bookingService
              .getBooking(waitlistOne.getPnr())
              .getSeatNumber() == 1);
    CHECK(fixture.bookingService
              .getBooking(waitlistTwo.getPnr())
              .getStatus() == "WAITLISTED");
    CHECK(fixture.bookingService
              .getBooking(confirmed.getPnr())
              .getStatus() == "CANCELLED");

    // Cancelling the second confirmed booking promotes passenger 2.
    const auto secondResult = fixture.bookingService
        .cancelBooking(waitlistOne.getPnr());

    CHECK(secondResult.waitlistPromoted);
    CHECK(secondResult.promotedPnr == waitlistTwo.getPnr());
    CHECK(fixture.bookingService
              .getBooking(waitlistTwo.getPnr())
              .getSeatNumber() == 1);
}

// ------------------------------------------------------------------
void testCancellingWaitlistDoesNotPromote() {
    std::cout << "testCancellingWaitlistDoesNotPromote\n";

    Fixture fixture("waitcancel");
    fixture.addJourney("T400", "2026-12-04", "Sleeper", 1, 15000);

    const auto confirmed = fixture.bookingService.createBooking(
        Fixture::request("T400", "2026-12-04", "Sleeper", "Deepa", 33)
    );
    const auto waiting = fixture.bookingService.createBooking(
        Fixture::request("T400", "2026-12-04", "Sleeper", "Arjun", 21)
    );

    CHECK(confirmed.getStatus() == "CONFIRMED");
    CHECK(waiting.getStatus() == "WAITLISTED");

    // Cancelling the waitlisted booking must not touch the confirmed
    // one, and nobody is promoted.
    const auto result = fixture.bookingService
        .cancelBooking(waiting.getPnr());

    CHECK(!result.waitlistPromoted);
    CHECK(fixture.bookingService
              .getBooking(confirmed.getPnr())
              .getStatus() == "CONFIRMED");
    CHECK(fixture.bookingService
              .getBooking(confirmed.getPnr())
              .getSeatNumber() == 1);

    // With the waitlist empty, cancelling frees the seat for a new
    // booking instead of promoting anyone.
    const auto cancelConfirmed = fixture.bookingService
        .cancelBooking(confirmed.getPnr());
    CHECK(!cancelConfirmed.waitlistPromoted);

    const auto fresh = fixture.bookingService.createBooking(
        Fixture::request("T400", "2026-12-04", "Sleeper", "Neha", 27)
    );
    CHECK(fresh.getStatus() == "CONFIRMED");
    CHECK(fresh.getSeatNumber() == 1);
}

// ------------------------------------------------------------------
void testDuplicateCancellationIsRejected() {
    std::cout << "testDuplicateCancellationIsRejected\n";

    Fixture fixture("duplicate");
    fixture.addJourney("T500", "2026-12-05", "Sleeper", 2, 10000);

    const auto booking = fixture.bookingService.createBooking(
        Fixture::request("T500", "2026-12-05", "Sleeper", "Iqbal", 44)
    );

    fixture.bookingService.cancelBooking(booking.getPnr());

    CHECK_THROWS(
        fixture.bookingService.cancelBooking(booking.getPnr()),
        ConflictError
    );

    // And the booking still reads as cancelled, once.
    CHECK(fixture.bookingService
              .getBooking(booking.getPnr())
              .getStatus() == "CANCELLED");
}

// ------------------------------------------------------------------
void testValidationErrors() {
    std::cout << "testValidationErrors\n";

    Fixture fixture("validation");
    fixture.addJourney("T600", "2026-12-06", "Sleeper", 2, 10000);

    // Empty / oversized / negative age.
    CHECK_THROWS(
        fixture.bookingService.createBooking(
            Fixture::request("T600", "2026-12-06", "Sleeper", "", 20)),
        ValidationError
    );
    CHECK_THROWS(
        fixture.bookingService.createBooking(Fixture::request(
            "T600", "2026-12-06", "Sleeper",
            std::string(120, 'x'), 20)),
        ValidationError
    );
    CHECK_THROWS(
        fixture.bookingService.createBooking(
            Fixture::request("T600", "2026-12-06", "Sleeper", "Ana", -1)),
        ValidationError
    );
    CHECK_THROWS(
        fixture.bookingService.createBooking(
            Fixture::request("T600", "2026-12-06", "Sleeper", "Ana", 121)),
        ValidationError
    );

    // Bad date and unknown journey.
    CHECK_THROWS(
        fixture.bookingService.createBooking(
            Fixture::request("T600", "12-12-2026", "Sleeper", "Ana", 20)),
        ValidationError
    );
    CHECK_THROWS(
        fixture.bookingService.createBooking(
            Fixture::request("T999", "2026-12-06", "Sleeper", "Ana", 20)),
        NotFoundError
    );

    // Unknown PNR and malformed PNR.
    CHECK_THROWS(
        fixture.bookingService.getBooking("RS00000000"),
        NotFoundError
    );
    CHECK_THROWS(
        fixture.bookingService.getBooking("NOT-APNR"),
        ValidationError
    );
    CHECK_THROWS(
        fixture.bookingService.cancelBooking("NOT-APNR"),
        ValidationError
    );

    // Availability service: unknown journey is a 404-class error.
    CHECK_THROWS(
        fixture.availabilityService.getAvailability(
            "T600", "2026-12-06", "First Class"),
        NotFoundError
    );
    CHECK_THROWS(
        fixture.availabilityService.search("Alpha", "Beta", "bad", ""),
        ValidationError
    );

    // Whitespace in the passenger name is trimmed, not rejected.
    fixture.bookingService.createBooking(
        Fixture::request("T600", "2026-12-06", "Sleeper",
                         "  Priti  ", 31));
    CHECK(fixture.availabilityService
              .getAvailability("T600", "2026-12-06", "Sleeper")
              .getConfirmedSeats() == 1);
}

// ------------------------------------------------------------------
void testSearchFiltersRoutes() {
    std::cout << "testSearchFiltersRoutes\n";

    Fixture fixture("search");
    fixture.addJourney("T700", "2026-12-07", "Sleeper", 3, 11000);
    fixture.addJourney("T701", "2026-12-07", "Sleeper", 3, 12000,
                       "Beta", "Gamma");

    // Route filter matches only the train that runs Alpha -> Beta.
    const auto results = fixture.availabilityService.search(
        "Alpha", "Beta", "2026-12-07", "");

    CHECK(results.size() == 1);
    CHECK(results.front().getTrain().getTrainNumber() == "T700");
    CHECK(results.front().getAvailableSeats() == 3);

    // Wrong direction returns nothing (not an error).
    const auto none = fixture.availabilityService.search(
        "Beta", "Alpha", "2026-12-07", "Sleeper");
    CHECK(none.empty());

    // Class filter works.
    const auto byClass = fixture.availabilityService.search(
        "", "", "2026-12-07", "Sleeper");
    CHECK(byClass.size() == 2);
}

}  // namespace

int main() {
    testConfirmedBookingGetsSeat();
    testSeatsAreUniqueAndFullJourneyWaitlists();
    testCancellationPromotesEarliestWaitlisted();
    testCancellingWaitlistDoesNotPromote();
    testDuplicateCancellationIsRejected();
    testValidationErrors();
    testSearchFiltersRoutes();

    std::cout << '\n'
              << (checksRun - checksFailed) << " / " << checksRun
              << " checks passed\n";

    if (checksFailed > 0) {
        std::cout << checksFailed << " FAILED\n";
        return 1;
    }

    std::cout << "All booking service tests passed\n";
    return 0;
}
