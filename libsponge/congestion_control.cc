#include "congestion_control.hh"

#include <algorithm>
#include <stdexcept>

RenoCongestionController::RenoCongestionController(const size_t mss,
                                                   const size_t initial_cwnd_segments,
                                                   const size_t initial_ssthresh_segments)
    : _mss(mss)
    , _cwnd(mss * initial_cwnd_segments)
    , _ssthresh(mss * initial_ssthresh_segments) {
    if (_mss == 0 || initial_cwnd_segments == 0 || initial_ssthresh_segments == 0) {
        throw std::invalid_argument("Reno congestion-control parameters must be non-zero");
    }

    if (_cwnd >= _ssthresh) {
        _state = CongestionState::congestion_avoidance;
    }
}

size_t RenoCongestionController::reduced_threshold(const size_t flight_size) const {
    return std::max(flight_size / 2, 2 * _mss);
}

size_t RenoCongestionController::send_allowance(const size_t advertised_window,
                                                const size_t flight_size) const {
    const size_t usable_window = std::min(advertised_window, _cwnd);
    return usable_window > flight_size ? usable_window - flight_size : 0;
}

void RenoCongestionController::on_new_ack(size_t newly_acked_bytes) {
    _duplicate_ack_count = 0;

    if (_state == CongestionState::fast_recovery) {
        _cwnd = _ssthresh;
        _acked_bytes_in_avoidance = 0;
        _state = CongestionState::congestion_avoidance;
        return;
    }

    if (newly_acked_bytes == 0) {
        return;
    }

    if (_state == CongestionState::slow_start) {
        const size_t room_before_threshold = _ssthresh - _cwnd;
        const size_t slow_start_growth = std::min(newly_acked_bytes, room_before_threshold);
        _cwnd += slow_start_growth;
        newly_acked_bytes -= slow_start_growth;

        if (_cwnd >= _ssthresh) {
            _state = CongestionState::congestion_avoidance;
        }
    }

    if (_state == CongestionState::congestion_avoidance) {
        _acked_bytes_in_avoidance += newly_acked_bytes;
        while (_acked_bytes_in_avoidance >= _cwnd) {
            _acked_bytes_in_avoidance -= _cwnd;
            _cwnd += _mss;
        }
    }
}

CongestionAction RenoCongestionController::on_duplicate_ack(const size_t flight_size) {
    ++_duplicate_ack_count;

    if (_state == CongestionState::fast_recovery) {
        _cwnd += _mss;
        return CongestionAction::none;
    }

    if (_duplicate_ack_count < 3) {
        return CongestionAction::none;
    }

    _ssthresh = reduced_threshold(flight_size);
    _cwnd = _ssthresh + 3 * _mss;
    _acked_bytes_in_avoidance = 0;
    _state = CongestionState::fast_recovery;
    return CongestionAction::fast_retransmit;
}

void RenoCongestionController::on_timeout(const size_t flight_size) {
    _ssthresh = reduced_threshold(flight_size);
    _cwnd = _mss;
    _acked_bytes_in_avoidance = 0;
    _duplicate_ack_count = 0;
    _state = CongestionState::slow_start;
}

double cubic_target_window(const double elapsed_seconds,
                           const double origin_time_seconds,
                           const double previous_max_window_segments,
                           const double cubic_constant) {
    const double offset = elapsed_seconds - origin_time_seconds;
    return cubic_constant * offset * offset * offset + previous_max_window_segments;
}
