cmake_minimum_required(VERSION 3.16)

project(RailSys LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

find_package(Threads REQUIRED)
find_package(SQLite3 REQUIRED)

add_subdirectory(third_party/Crow)

# ---------------------------------------------------------------
# Application library: everything except main() so the tests can
# link against the same code the server runs.
# ---------------------------------------------------------------
add_library(railsys_core STATIC
    src/app/AppError.cpp
    src/database/Database.cpp
    src/database/Statement.cpp
    src/database/Transaction.cpp
    src/repositories/Repository.cpp
    src/repositories/TrainRepository.cpp
    src/repositories/JourneyRepository.cpp
    src/repositories/BookingRepository.cpp
    src/services/AvailabilityService.cpp
    src/services/BookingService.cpp
)

target_include_directories(railsys_core PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_link_libraries(railsys_core PUBLIC
    SQLite::SQLite3
    Threads::Threads
)

# ---------------------------------------------------------------
# Server executable
# ---------------------------------------------------------------
add_executable(railsys
    src/main.cpp
    src/routes/ApiRoutes.cpp
    src/routes/StaticRoutes.cpp
)

target_link_libraries(railsys PRIVATE
    railsys_core
    Crow::Crow
)

# ---------------------------------------------------------------
# Tests: booking, waitlist and cancellation behaviour
# ---------------------------------------------------------------
enable_testing()

add_executable(railsys_tests
    tests/BookingServiceTests.cpp
)

target_link_libraries(railsys_tests PRIVATE railsys_core)

add_test(NAME booking_service_tests COMMAND railsys_tests)
