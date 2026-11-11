#include <windows.h>

#include <iostream>
#include <string_view>
#include "../include/telemetry.hpp"

namespace {

class Device {
public:
    Device() : handle(CreateFileW(telemetry::device_path, GENERIC_READ,
        FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr, OPEN_EXISTING, 0, nullptr)) {}
    ~Device() { if (handle != INVALID_HANDLE_VALUE) CloseHandle(handle); }
    Device(const Device&) = delete;
    Device& operator=(const Device&) = delete;
    HANDLE handle;
};

bool snapshot(HANDLE device, telemetry::Snapshot& value) {
    DWORD bytes = 0;
    if (!DeviceIoControl(device, telemetry::query, nullptr, 0, &value,
                         sizeof(value), &bytes, nullptr)) {
        std::cerr << "DeviceIoControl failed, Win32 error " << GetLastError() << '\n';
        return false;
    }
    if (bytes != sizeof(value) || !telemetry::valid(value)) {
        std::cerr << "Incompatible or inconsistent telemetry response\n";
        return false;
    }
    return true;
}

bool expect_rejection(HANDLE device, DWORD code, void* input, DWORD input_size,
                      DWORD output_size, DWORD expected) {
    telemetry::Snapshot value{};
    DWORD bytes = 0;
    if (DeviceIoControl(device, code, input, input_size, &value, output_size, &bytes, nullptr)) {
        std::cerr << "Malformed request unexpectedly succeeded\n";
        return false;
    }
    const DWORD error = GetLastError();
    if (error != expected || bytes != 0) {
        std::cerr << "Unexpected rejection: error " << error << ", bytes " << bytes << '\n';
        return false;
    }
    return true;
}

bool self_test(HANDLE device, const telemetry::Snapshot& before, telemetry::Snapshot& after) {
    BYTE unexpected = 1;
    if (!expect_rejection(device, telemetry::query + 4, nullptr, 0, sizeof(after), ERROR_INVALID_FUNCTION) ||
        !expect_rejection(device, telemetry::query, &unexpected, 1, sizeof(after), ERROR_INVALID_PARAMETER) ||
        !expect_rejection(device, telemetry::query, nullptr, 0, 1, ERROR_INSUFFICIENT_BUFFER) ||
        !snapshot(device, after)) {
        return false;
    }
    if (after.succeeded <= before.succeeded || after.rejected < before.rejected ||
        after.rejected - before.rejected < 3 ||
        after.uptime_100ns < before.uptime_100ns) {
        std::cerr << "Telemetry counters or uptime did not advance as expected\n";
        return false;
    }
    return true;
}

}

int main(int argc, char** argv) {
    const std::string_view mode = argc == 2 ? argv[1] : "";
    if (argc > 2 || (argc == 2 && mode != "--self-test" && mode != "--help")) {
        std::cerr << "Usage: telemetry-client [--self-test | --help]\n";
        return 2;
    }
    if (mode == "--help") {
        std::cout << "Usage: telemetry-client [--self-test | --help]\n";
        return 0;
    }
    const Device device;
    if (device.handle == INVALID_HANDLE_VALUE) {
        std::cerr << "Cannot open BnqqKernelTelemetry, Win32 error " << GetLastError() << '\n';
        return 1;
    }
    telemetry::Snapshot value{};
    if (!snapshot(device.handle, value)) return 1;
    if (mode == "--self-test") {
        const auto before = value;
        if (!self_test(device.handle, before, value)) return 1;
        std::cout << "PASS: unsupported IOCTL, unexpected input, short output and valid query\n";
    }
    std::cout << "ABI version: " << value.abi_version << '\n'
              << "Driver uptime (ms): " << value.uptime_100ns / 10000 << '\n'
              << "Requests: " << value.requests << '\n'
              << "Succeeded: " << value.succeeded << '\n'
              << "Rejected: " << value.rejected << '\n';
}
