#pragma once

#include <stdexcept>
#include <string>

// Base class for all application-level errors.
//
// Services throw the derived types; the route layer catches AppError
// by reference and calls the *virtual* httpStatus() method, so each
// subclass decides which HTTP status it maps to. This is the project's
// main example of runtime polymorphism: one catch clause handles every
// error type correctly.
class AppError : public std::runtime_error {
public:
    explicit AppError(const std::string& message)
        : std::runtime_error(message) {}

    ~AppError() override = default;

    virtual int httpStatus() const noexcept = 0;
};

// 400 - the caller sent something invalid.
class ValidationError : public AppError {
public:
    explicit ValidationError(const std::string& message)
        : AppError(message) {}

    int httpStatus() const noexcept override { return 400; }
};

// 404 - the requested journey / booking does not exist.
class NotFoundError : public AppError {
public:
    explicit NotFoundError(const std::string& message)
        : AppError(message) {}

    int httpStatus() const noexcept override { return 404; }
};

// 409 - the request conflicts with current state
// (for example cancelling an already cancelled booking).
class ConflictError : public AppError {
public:
    explicit ConflictError(const std::string& message)
        : AppError(message) {}

    int httpStatus() const noexcept override { return 409; }
};

// 500 - unexpected database or internal failure.
class DatabaseError : public AppError {
public:
    explicit DatabaseError(const std::string& message)
        : AppError(message) {}

    int httpStatus() const noexcept override { return 500; }
};
