#ifndef SPONGE_LIBSPONGE_CONGESTION_CONTROL_HH
#define SPONGE_LIBSPONGE_CONGESTION_CONTROL_HH

#include <cstddef>

enum class CongestionState {
    slow_start,
    congestion_avoidance,
    fast_recovery,
};

enum class CongestionAction {
    none,
    fast_retransmit,
};

//! A deterministic teaching model for RFC 5681-style Reno state changes.
//! It is intentionally kept separate from TCPSender so existing CS144 Lab
//! behavior remains unchanged.
class RenoCongestionController {
  private:
    size_t _mss;
    size_t _cwnd;
    size_t _ssthresh;
    size_t _acked_bytes_in_avoidance{0};
    size_t _duplicate_ack_count{0};
    CongestionState _state{CongestionState::slow_start};

    size_t reduced_threshold(const size_t flight_size) const;

  public:
    explicit RenoCongestionController(const size_t mss = 1000,
                                      const size_t initial_cwnd_segments = 1,
                                      const size_t initial_ssthresh_segments = 64);

    //! Fresh sequence-space permitted by both receiver and congestion windows.
    size_t send_allowance(const size_t advertised_window, const size_t flight_size) const;

    //! Process newly acknowledged bytes. Any new ACK exits this simplified fast recovery.
    void on_new_ack(size_t newly_acked_bytes);

    //! Process one duplicate ACK and report whether fast retransmit should run.
    CongestionAction on_duplicate_ack(const size_t flight_size);

    //! Treat an RTO as a strong congestion signal.
    void on_timeout(const size_t flight_size);

    size_t congestion_window() const { return _cwnd; }
    size_t slow_start_threshold() const { return _ssthresh; }
    size_t duplicate_ack_count() const { return _duplicate_ack_count; }
    CongestionState state() const { return _state; }
};

//! Illustrative CUBIC target window in segments: W(t) = C * (t - K)^3 + W_max.
double cubic_target_window(double elapsed_seconds,
                           double origin_time_seconds,
                           double previous_max_window_segments,
                           double cubic_constant = 0.4);

#endif  // SPONGE_LIBSPONGE_CONGESTION_CONTROL_HH
