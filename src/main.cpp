#include "database/Database.h"
#include "repositories/BookingRepository.h"
#include "repositories/JourneyRepository.h"
#include "repositories/TrainRepository.h"
#include "routes/ApiRoutes.h"
#include "routes/StaticRoutes.h"
#include "services/AvailabilityService.h"
#include "services/BookingService.h"

#include "crow.h"

#include <filesystem>
#include <iostream>
#include <memory>
#include <mutex>

int main(int argc, char** argv) {
    try {
        std::filesystem::create_directories("data");

        auto database =
            std::make_shared<Database>("data/railsys.db");

        database->initializeSchema();
        database->seedDemoData();

        // --------------------------------------------------
        // Wiring: repositories (data access) feed services
        // (business logic), which are handed to the route
        // layer (HTTP). main() only connects the pieces.
        // --------------------------------------------------
        TrainRepository trainRepository(database->getConnection());
        JourneyRepository journeyRepository(
            database->getConnection()
        );
        BookingRepository bookingRepository(database->getConnection());

        AvailabilityService availabilityService(
            trainRepository, journeyRepository
        );
        BookingService bookingService(
            journeyRepository, bookingRepository
        );

        crow::SimpleApp app;

        // All routes share one SQLite connection, so database
        // work is serialized with this mutex.
        std::mutex databaseMutex;

        registerApiRoutes(
            app, availabilityService, bookingService, databaseMutex
        );

        // Locate the frontend files relative to the current directory
        // or the executable, so the server works from any cwd.
        const auto publicDirectory = findPublicDirectory(
            argc > 0 ? argv[0] : "railsys"
        );
        registerStaticRoutes(app, publicDirectory);

        std::cout
            << "Serving frontend from "
            << std::filesystem::absolute(publicDirectory).string()
            << '\n'
            << "RailSys is ready on http://127.0.0.1:18080\n";

        app.port(18080)
            .bindaddr("127.0.0.1")
            .run();

    } catch (const std::exception& error) {
        std::cerr << "RailSys startup failed: "
                  << error.what() << '\n';

        return 1;
    }

    return 0;
}
