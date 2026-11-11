#pragma once

namespace telemetry {

inline constexpr wchar_t device_path[] = L"\\\\.\\BnqqKernelTelemetry";
inline constexpr unsigned long device_type = 0x8000;
inline constexpr unsigned long function_query = 0x800;
// CTL_CODE(device_type, function_query, METHOD_BUFFERED, FILE_READ_ACCESS)
inline constexpr unsigned long query = (device_type << 16) | (1UL << 14) | (function_query << 2);
inline constexpr unsigned int version = 1;

struct Snapshot {
    unsigned int size;
    unsigned int abi_version;
    unsigned long long uptime_100ns;
    unsigned long long requests;
    unsigned long long succeeded;
    unsigned long long rejected;
};

static_assert(sizeof(Snapshot) == 40);

enum class Result { success, unsupported, invalid_input, small_buffer };

struct Counters {
    unsigned long long succeeded = 0;
    unsigned long long rejected = 0;

    // The caller serializes this operation. Failed requests never write output.
    Result dispatch(unsigned long code, unsigned long input_length,
                    unsigned long output_length, Snapshot* output,
                    unsigned long long uptime) noexcept {
        if (code != query) {
            ++rejected;
            return Result::unsupported;
        }
        if (input_length != 0) {
            ++rejected;
            return Result::invalid_input;
        }
        if (output_length < sizeof(Snapshot) || output == nullptr) {
            ++rejected;
            return Result::small_buffer;
        }
        ++succeeded;
        *output = {sizeof(Snapshot), version, uptime, succeeded + rejected, succeeded, rejected};
        return Result::success;
    }
};

inline bool valid(const Snapshot& value) noexcept {
    return value.size == sizeof(Snapshot) && value.abi_version == version &&
           value.succeeded <= value.requests && value.rejected == value.requests - value.succeeded;
}

}
