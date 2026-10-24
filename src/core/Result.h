#pragma once
#include <QString>
#include <variant>
#include <utility>

namespace netraxis {
enum class ErrorCode {
    InvalidInput, PermissionDenied, Unsupported, AuthenticationRequired,
    Timeout, Cancelled, Conflict, Storage, CorruptData, Network, Internal
};
struct Error {
    ErrorCode code;
    QString message;
    bool retryable = false;
    QString operation;
};
template<class T> class Result {
public:
    static Result success(T value) { return Result(std::move(value)); }
    static Result failure(Error error) { return Result(std::move(error)); }
    bool ok() const { return std::holds_alternative<T>(data_); }
    explicit operator bool() const { return ok(); }
    const T &value() const { return std::get<T>(data_); }
    T &value() { return std::get<T>(data_); }
    const Error &error() const { return std::get<Error>(data_); }
private:
    explicit Result(T value) : data_(std::move(value)) {}
    explicit Result(Error error) : data_(std::move(error)) {}
    std::variant<T, Error> data_;
};
using Status = Result<std::monostate>;
inline Status success() { return Status::success({}); }
}
