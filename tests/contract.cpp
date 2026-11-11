#include <iostream>
#include "../include/telemetry.hpp"

int main() {
    using namespace telemetry;
    Counters counters;
    Snapshot output{};
    if (counters.dispatch(query + 4, 0, sizeof(output), &output, 0) != Result::unsupported ||
        counters.dispatch(query, 1, sizeof(output), &output, 0) != Result::invalid_input ||
        counters.dispatch(query, 0, sizeof(output) - 1, &output, 0) != Result::small_buffer ||
        counters.dispatch(query, 0, sizeof(output), nullptr, 0) != Result::small_buffer ||
        output.size != 0 || counters.succeeded != 0 || counters.rejected != 4) {
        std::cerr << "Reject path changed the output or counters unexpectedly\n";
        return 1;
    }
    if (counters.dispatch(query, 0, sizeof(output), &output, 123) != Result::success ||
        !valid(output) || output.requests != 5 || output.succeeded != 1 ||
        output.rejected != 4 || output.uptime_100ns != 123) {
        std::cerr << "Snapshot is inconsistent\n";
        return 1;
    }
    output.abi_version = version + 1;
    if (valid(output)) {
        std::cerr << "Unknown protocol version was accepted\n";
        return 1;
    }
    std::cout << "PASS: protocol validation and accounting\n";
}
