#include "congestion_control.hh"

#include <cstdlib>
#include <iomanip>
#include <iostream>
#include <string>

using namespace std;

namespace {

const char *state_name(const CongestionState state) {
    switch (state) {
        case CongestionState::slow_start:
            return "slow_start";
        case CongestionState::congestion_avoidance:
            return "congestion_avoidance";
        case CongestionState::fast_recovery:
            return "fast_recovery";
    }
    return "unknown";
}

void print_reno(const string &event, const RenoCongestionController &controller) {
    cout << left << setw(24) << event
         << " cwnd=" << setw(6) << controller.congestion_window()
         << "ssthresh=" << setw(6) << controller.slow_start_threshold()
         << "dup_ack=" << setw(2) << controller.duplicate_ack_count()
         << "state=" << state_name(controller.state()) << '\n';
}

void effective_window_demo() {
    RenoCongestionController controller(1000, 4, 16);
    cout << "rwnd=12000 cwnd=" << controller.congestion_window()
         << " flight=2500 allowance=" << controller.send_allowance(12000, 2500) << '\n';
    cout << "rwnd=2000  cwnd=" << controller.congestion_window()
         << " flight=1500 allowance=" << controller.send_allowance(2000, 1500) << '\n';
}

void slow_start_demo() {
    RenoCongestionController controller(1000, 1, 8);
    print_reno("initial", controller);
    controller.on_new_ack(1000);
    print_reno("ACK 1000 bytes", controller);
    controller.on_new_ack(2000);
    print_reno("ACK 2000 bytes", controller);
    controller.on_new_ack(4000);
    print_reno("ACK 4000 bytes", controller);
}

void congestion_avoidance_demo() {
    RenoCongestionController controller(1000, 4, 4);
    print_reno("initial", controller);
    controller.on_new_ack(2000);
    print_reno("ACK half window", controller);
    controller.on_new_ack(2000);
    print_reno("ACK one RTT total", controller);
}

void fast_recovery_demo() {
    RenoCongestionController controller(1000, 8, 8);
    print_reno("initial", controller);
    for (size_t index = 1; index <= 3; ++index) {
        const CongestionAction action = controller.on_duplicate_ack(8000);
        const string event = "duplicate ACK " + to_string(index) +
                             (action == CongestionAction::fast_retransmit ? " -> retransmit" : "");
        print_reno(event, controller);
    }
    controller.on_duplicate_ack(8000);
    print_reno("duplicate ACK 4", controller);
    controller.on_new_ack(1000);
    print_reno("new ACK exits recovery", controller);
}

void timeout_demo() {
    RenoCongestionController controller(1000, 10, 10);
    print_reno("before RTO", controller);
    controller.on_timeout(10000);
    print_reno("RTO", controller);
    controller.on_new_ack(1000);
    print_reno("first ACK after RTO", controller);
}

void cubic_curve_demo() {
    constexpr double origin_time = 2.0;
    constexpr double previous_max = 100.0;

    cout << fixed << setprecision(1);
    for (const double elapsed : {0.0, 1.0, 2.0, 3.0, 4.0}) {
        cout << "t=" << elapsed
             << " target_cwnd=" << cubic_target_window(elapsed, origin_time, previous_max)
             << '\n';
    }
}

void usage(const char *program) {
    cerr << "usage: " << program
         << " <effective-window|slow-start|congestion-avoidance|fast-recovery|timeout|cubic-curve>\n";
}

}  // namespace

int main(const int argc, char *argv[]) {
    if (argc != 2) {
        usage(argv[0]);
        return EXIT_FAILURE;
    }

    const string scenario = argv[1];
    if (scenario == "effective-window") {
        effective_window_demo();
    } else if (scenario == "slow-start") {
        slow_start_demo();
    } else if (scenario == "congestion-avoidance") {
        congestion_avoidance_demo();
    } else if (scenario == "fast-recovery") {
        fast_recovery_demo();
    } else if (scenario == "timeout") {
        timeout_demo();
    } else if (scenario == "cubic-curve") {
        cubic_curve_demo();
    } else {
        usage(argv[0]);
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}
