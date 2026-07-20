#include "congestion_control.hh"

#include <cassert>
#include <cmath>

int main() {
    {
        RenoCongestionController controller;
        assert(controller.send_allowance(8000, 400) == 600);
        assert(controller.send_allowance(500, 400) == 100);
    }

    {
        RenoCongestionController controller(1000, 1, 8);
        controller.on_new_ack(1000);
        assert(controller.congestion_window() == 2000);
        controller.on_new_ack(2000);
        assert(controller.congestion_window() == 4000);
        assert(controller.state() == CongestionState::slow_start);
    }

    {
        RenoCongestionController controller(1000, 4, 4);
        controller.on_new_ack(4000);
        assert(controller.congestion_window() == 5000);
        assert(controller.state() == CongestionState::congestion_avoidance);
    }

    {
        RenoCongestionController controller(1000, 8, 8);
        assert(controller.on_duplicate_ack(8000) == CongestionAction::none);
        assert(controller.on_duplicate_ack(8000) == CongestionAction::none);
        assert(controller.on_duplicate_ack(8000) == CongestionAction::fast_retransmit);
        assert(controller.slow_start_threshold() == 4000);
        assert(controller.congestion_window() == 7000);
        assert(controller.state() == CongestionState::fast_recovery);

        controller.on_duplicate_ack(8000);
        assert(controller.congestion_window() == 8000);

        controller.on_new_ack(1000);
        assert(controller.congestion_window() == 4000);
        assert(controller.state() == CongestionState::congestion_avoidance);
    }

    {
        RenoCongestionController controller(1000, 10, 10);
        controller.on_timeout(10000);
        assert(controller.slow_start_threshold() == 5000);
        assert(controller.congestion_window() == 1000);
        assert(controller.state() == CongestionState::slow_start);
    }

    assert(std::abs(cubic_target_window(0.0, 2.0, 100.0) - 96.8) < 0.0001);
    assert(std::abs(cubic_target_window(2.0, 2.0, 100.0) - 100.0) < 0.0001);
    assert(std::abs(cubic_target_window(4.0, 2.0, 100.0) - 103.2) < 0.0001);

    return 0;
}
