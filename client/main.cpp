#include <windows.h>

#include <iostream>
#include <chrono>
#include <future>
#include <string_view>
#include <thread>
#include <vector>
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

bool stress_test(HANDLE device, const telemetry::Snapshot& before, telemetry::Snapshot& after) {
    constexpr int clients = 8;
    constexpr int queries = 250;
    const auto start = std::chrono::steady_clock::now() + std::chrono::milliseconds(250);
    std::vector<std::future<bool>> jobs;
    try {
        for (int i = 0; i < clients; ++i) {
            jobs.push_back(std::async(std::launch::async, [start, queries] {
                const Device connection;
                if (connection.handle == INVALID_HANDLE_VALUE) return false;
                std::this_thread::sleep_until(start);
                telemetry::Snapshot previous{};
                for (int n = 0; n < queries; ++n) {
                    telemetry::Snapshot current{};
                    if (!snapshot(connection.handle, current) ||
                        current.succeeded <= previous.succeeded ||
                        current.uptime_100ns < previous.uptime_100ns) {
                        return false;
                    }
                    previous = current;
                }
                return true;
            }));
        }
        bool passed = true;
        for (auto& job : jobs) {
            if (!job.get()) passed = false;
        }
        if (!passed || !snapshot(device, after)) return false;
    } catch (const std::exception& error) {
        std::cerr << "Could not run stress test: " << error.what() << '\n';
        return false;
    }
    if (after.succeeded != before.succeeded + clients * queries + 1 ||
        after.rejected != before.rejected || after.uptime_100ns < before.uptime_100ns) {
        std::cerr << "Stress test accounting mismatch; run without other clients\n";
        return false;
    }
    std::cout << "PASS: " << clients << " independent handles, " << clients * queries
              << " worker queries\n";
    return true;
}

}

int main(int argc, char** argv) {
    const std::string_view mode = argc == 2 ? argv[1] : "";
    if (argc > 2 || (argc == 2 && mode != "--self-test" && mode != "--stress" && mode != "--help")) {
        std::cerr << "Usage: telemetry-client [--self-test | --stress | --help]\n";
        return 2;
    }
    if (mode == "--help") {
        std::cout << "Usage: telemetry-client [--self-test | --stress | --help]\n";
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
    } else if (mode == "--stress") {
        const auto before = value;
        if (!stress_test(device.handle, before, value)) return 1;
    }
    std::cout << "ABI version: " << value.abi_version << '\n'
              << "Driver uptime (ms): " << value.uptime_100ns / 10000 << '\n'
              << "Requests: " << value.requests << '\n'
              << "Succeeded: " << value.succeeded << '\n'
              << "Rejected: " << value.rejected << '\n';
}
