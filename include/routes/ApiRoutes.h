#pragma once

#include "services/AvailabilityService.h"
#include "services/BookingService.h"

#include "crow.h"

#include <mutex>

// Registers every /api/* route on the Crow application.
//
// Handlers stay thin: they read parameters, call a service and
// serialize the result. AppError subclasses are caught here and
// mapped to HTTP status codes through the virtual httpStatus()
// method, so error handling lives in exactly one place.
void registerApiRoutes(crow::SimpleApp& app,
                       const AvailabilityService& availabilityService,
                       BookingService& bookingService,
                       std::mutex& databaseMutex);
