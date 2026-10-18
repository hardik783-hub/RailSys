#include "routes/ApiRoutes.h"

#include "app/AppError.h"

#include <string>
#include <utility>

namespace {

crow::response jsonResponse(int status, crow::json::wvalue body) {
    crow::response response(status, body.dump());
    response.set_header("Content-Type", "application/json");
    return response;
}

crow::response errorResponse(int status, const std::string& message) {
    crow::json::wvalue body;
    body["error"] = message;
    return jsonResponse(status, std::move(body));
}

// Runtime polymorphism in action: every AppError subclass answers
// with its own HTTP status through the virtual httpStatus().
crow::response errorResponse(const AppError& error) {
    return errorResponse(error.httpStatus(), error.what());
}

crow::json::wvalue availabilityToJson(const Availability& availability) {
    const Journey& journey = availability.getJourney();
    const Train& train = availability.getTrain();

    crow::json::wvalue item;
    item["trainNumber"] = train.getTrainNumber();
    item["trainName"] = train.getName();
    item["source"] = train.getSource();
    item["destination"] = train.getDestination();
    item["journeyDate"] = journey.getJourneyDate();
    item["travelClass"] = journey.getTravelClass();
    item["capacity"] = journey.getCapacity();
    item["totalSeats"] = availability.getTotalSeats();
    item["confirmedSeats"] = availability.getConfirmedSeats();
    item["availableSeats"] = availability.getAvailableSeats();
    item["waitlistedCount"] = availability.getWaitlistedBookings();
    item["baseFarePaise"] = journey.getBaseFarePaise();
    item["baseFareRupees"] = journey.getBaseFareRupees();
    item["isFull"] = availability.isFull();

    return item;
}

crow::json::wvalue bookingToJson(const Booking& booking) {
    crow::json::wvalue item;
    item["pnr"] = booking.getPnr();
    item["status"] = booking.getStatus();
    item["trainNumber"] = booking.getTrainNumber();
    item["trainName"] = booking.getTrainName();
    item["source"] = booking.getSource();
    item["destination"] = booking.getDestination();
    item["journeyDate"] = booking.getJourneyDate();
    item["travelClass"] = booking.getTravelClass();
    item["passengerName"] = booking.getPassengerName();
    item["passengerAge"] = booking.getPassengerAge();
    item["passengerStatus"] = booking.getPassengerStatus();
    item["farePaise"] = booking.getTotalFarePaise();
    item["fareRupees"] = booking.getTotalFareRupees();

    if (booking.hasSeat()) {
        item["seatNumber"] = booking.getSeatNumber();
    } else {
        item["seatNumber"] = nullptr;
    }

    return item;
}

std::string paramOr(const crow::request& request,
                    const char* name) {
    const char* value = request.url_params.get(name);
    return value ? std::string(value) : std::string();
}

}  // namespace

void registerApiRoutes(crow::SimpleApp& app,
                       const AvailabilityService& availabilityService,
                       BookingService& bookingService,
                       std::mutex& databaseMutex) {
    // ---------------------------------------------------------
    // GET /api/health
    // ---------------------------------------------------------
    CROW_ROUTE(app, "/api/health")
    ([] {
        crow::json::wvalue response;
        response["status"] = "ok";
        response["project"] = "RailSys";
        response["database"] = "connected";

        return response;
    });

    // ---------------------------------------------------------
    // GET /api/meta - stations, travel classes, train count
    // ---------------------------------------------------------
    CROW_ROUTE(app, "/api/meta")
    .methods(crow::HTTPMethod::GET)
    ([&availabilityService, &databaseMutex] {
        std::lock_guard<std::mutex> lock(databaseMutex);

        try {
            const auto meta = availabilityService.getMeta();

            crow::json::wvalue::list stations;
            for (const auto& station : meta.stations) {
                stations.push_back(station);
            }

            crow::json::wvalue::list classes;
            for (const auto& travelClass : meta.travelClasses) {
                classes.push_back(travelClass);
            }

            crow::json::wvalue response;
            response["stations"] = std::move(stations);
            response["travelClasses"] = std::move(classes);
            response["trainCount"] = meta.trainCount;

            return jsonResponse(200, std::move(response));
        } catch (const AppError& error) {
            return errorResponse(error);
        } catch (const std::exception& error) {
            return errorResponse(500, error.what());
        }
    });

    // ---------------------------------------------------------
    // GET /api/trains
    // ---------------------------------------------------------
    CROW_ROUTE(app, "/api/trains")
    .methods(crow::HTTPMethod::GET)
    ([&availabilityService, &databaseMutex] {
        std::lock_guard<std::mutex> lock(databaseMutex);

        try {
            const auto trains = availabilityService.listTrains();

            crow::json::wvalue::list items;
            for (const auto& train : trains) {
                crow::json::wvalue item;
                item["id"] = train.getId();
                item["trainNumber"] = train.getTrainNumber();
                item["name"] = train.getName();
                item["source"] = train.getSource();
                item["destination"] = train.getDestination();

                items.push_back(std::move(item));
            }

            crow::json::wvalue response;
            response["trains"] = std::move(items);

            return jsonResponse(200, std::move(response));
        } catch (const AppError& error) {
            return errorResponse(error);
        } catch (const std::exception& error) {
            return errorResponse(500, error.what());
        }
    });

    // ---------------------------------------------------------
    // GET /api/search?source=&destination=&journeyDate=&travelClass=
    // All parameters are optional; an empty filter matches any.
    // ---------------------------------------------------------
    CROW_ROUTE(app, "/api/search")
    .methods(crow::HTTPMethod::GET)
    ([&availabilityService, &databaseMutex](const crow::request& req) {
        std::lock_guard<std::mutex> lock(databaseMutex);

        try {
            const auto results = availabilityService.search(
                paramOr(req, "source"),
                paramOr(req, "destination"),
                paramOr(req, "journeyDate"),
                paramOr(req, "travelClass")
            );

            crow::json::wvalue::list items;
            for (const auto& availability : results) {
                items.push_back(availabilityToJson(availability));
            }

            crow::json::wvalue response;
            response["journeyDate"] = paramOr(req, "journeyDate");
            response["count"] = static_cast<int>(items.size());
            response["results"] = std::move(items);

            return jsonResponse(200, std::move(response));
        } catch (const AppError& error) {
            return errorResponse(error);
        } catch (const std::exception& error) {
            return errorResponse(500, error.what());
        }
    });

    // ---------------------------------------------------------
    // GET /api/availability?trainNumber=&journeyDate=&travelClass=
    // ---------------------------------------------------------
    CROW_ROUTE(app, "/api/availability")
    .methods(crow::HTTPMethod::GET)
    ([&availabilityService, &databaseMutex](const crow::request& req) {
        std::lock_guard<std::mutex> lock(databaseMutex);

        try {
            const auto availability =
                availabilityService.getAvailability(
                    paramOr(req, "trainNumber"),
                    paramOr(req, "journeyDate"),
                    paramOr(req, "travelClass")
                );

            return jsonResponse(
                200, availabilityToJson(availability)
            );
        } catch (const AppError& error) {
            return errorResponse(error);
        } catch (const std::exception& error) {
            return errorResponse(500, error.what());
        }
    });

    // ---------------------------------------------------------
    // POST /api/bookings
    // ---------------------------------------------------------
    CROW_ROUTE(app, "/api/bookings")
    .methods(crow::HTTPMethod::POST)
    ([&bookingService,
      &databaseMutex](const crow::request& req) {
        std::lock_guard<std::mutex> lock(databaseMutex);

        try {
            auto body = crow::json::load(req.body);

            if (!body) {
                throw ValidationError(
                    "Request body must be valid JSON"
                );
            }

            BookingService::BookingRequest request;
            request.trainNumber =
                body.has("trainNumber")
                    ? std::string(body["trainNumber"].s()) : "";
            request.journeyDate =
                body.has("journeyDate")
                    ? std::string(body["journeyDate"].s()) : "";
            request.travelClass =
                body.has("travelClass")
                    ? std::string(body["travelClass"].s()) : "";
            request.passengerName =
                body.has("passengerName")
                    ? std::string(body["passengerName"].s()) : "";
            request.passengerAge =
                body.has("passengerAge")
                    ? body["passengerAge"].i() : -1;

            const Booking booking =
                bookingService.createBooking(request);

            return jsonResponse(201, bookingToJson(booking));
        } catch (const AppError& error) {
            return errorResponse(error);
        } catch (const std::exception& error) {
            return errorResponse(
                400,
                std::string("Invalid booking payload: ") +
                    error.what()
            );
        }
    });

    // ---------------------------------------------------------
    // GET /api/bookings/<string>
    // ---------------------------------------------------------
    CROW_ROUTE(app, "/api/bookings/<string>")
    .methods(crow::HTTPMethod::GET)
    ([&bookingService, &databaseMutex](const std::string& pnr) {
        std::lock_guard<std::mutex> lock(databaseMutex);

        try {
            return jsonResponse(
                200, bookingToJson(bookingService.getBooking(pnr))
            );
        } catch (const AppError& error) {
            return errorResponse(error);
        } catch (const std::exception& error) {
            return errorResponse(500, error.what());
        }
    });

    // ---------------------------------------------------------
    // POST /api/bookings/<string>/cancel
    // ---------------------------------------------------------
    CROW_ROUTE(app, "/api/bookings/<string>/cancel")
    .methods(crow::HTTPMethod::POST)
    ([&bookingService, &databaseMutex](const std::string& pnr) {
        std::lock_guard<std::mutex> lock(databaseMutex);

        try {
            const auto result =
                bookingService.cancelBooking(pnr);

            crow::json::wvalue response;
            response["message"] = "Booking cancelled successfully";
            response["pnr"] = result.pnr;
            response["status"] = "CANCELLED";
            response["waitlistPromoted"] = result.waitlistPromoted;

            if (result.waitlistPromoted) {
                response["promotedPNR"] = result.promotedPnr;
                response["promotedSeatNumber"] =
                    result.promotedSeatNumber;
            } else {
                response["promotedPNR"] = nullptr;
                response["promotedSeatNumber"] = nullptr;
            }

            return jsonResponse(200, std::move(response));
        } catch (const AppError& error) {
            return errorResponse(error);
        } catch (const std::exception& error) {
            return errorResponse(500, error.what());
        }
    });
}
