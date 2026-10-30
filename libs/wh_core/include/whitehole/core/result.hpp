// result.hpp: Fehlerbehandlung ohne Ausnahmen im Echtzeitpfad.
#pragma once

#include <optional>
#include <string>
#include <utility>
#include <variant>

namespace whitehole {

enum class ErrorCode {
    InvalidArgument,
    NotFound,
    AlreadyExists,
    IncompatiblePorts,
    CycleDetected,
    Unsupported,
    IoError,
    GpuError,
    ShaderError,
    Internal,
};

struct Error {
    ErrorCode code{ErrorCode::Internal};
    std::string message;
};

/// Entweder ein Wert vom Typ T oder ein Error. Der Zugriff auf den jeweils
/// falschen Fall ist ein Programmierfehler und wirft std::bad_variant_access.
/// Hinweis: Ersetzbar durch std::expected, sobald C++23 verfügbar ist.
template <typename T>
class [[nodiscard]] Result {
public:
    Result(T value) : data_{std::in_place_index<0>, std::move(value)} {}
    Result(Error error) : data_{std::in_place_index<1>, std::move(error)} {}

    [[nodiscard]] bool ok() const noexcept { return data_.index() == 0; }
    explicit operator bool() const noexcept { return ok(); }

    [[nodiscard]] T& value() & { return std::get<0>(data_); }
    [[nodiscard]] const T& value() const& { return std::get<0>(data_); }
    [[nodiscard]] T&& value() && { return std::get<0>(std::move(data_)); }

    [[nodiscard]] const Error& error() const { return std::get<1>(data_); }

private:
    std::variant<T, Error> data_;
};

/// Spezialisierung für Operationen ohne Rückgabewert: Standard = Erfolg.
template <>
class [[nodiscard]] Result<void> {
public:
    Result() = default;
    Result(Error error) : error_{std::move(error)} {}

    [[nodiscard]] bool ok() const noexcept { return !error_.has_value(); }
    explicit operator bool() const noexcept { return ok(); }

    [[nodiscard]] const Error& error() const { return error_.value(); }

private:
    std::optional<Error> error_;
};

}  // namespace whitehole
