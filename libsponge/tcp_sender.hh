#ifndef SPONGE_LIBSPONGE_TCP_SENDER_HH
#define SPONGE_LIBSPONGE_TCP_SENDER_HH

#include "byte_stream.hh"
#include "tcp_config.hh"
#include "tcp_segment.hh"
#include "wrapping_integers.hh"

#include <deque>
#include <functional>
#include <optional>
#include <queue>

//! \brief The "sender" part of a TCP implementation.

//! Accepts a ByteStream, divides it up into segments and sends the
//! segments, keeps track of which segments are still in-flight,
//! maintains the Retransmission Timer, and retransmits in-flight
//! segments if the retransmission timer expires.

class TCPTimer {
  private:
    bool _running;
    unsigned int _current_rto;
    unsigned int _initial_rto;
    unsigned int _elapsed;
    unsigned int _consecutive_retransmissions;

  public:
    TCPTimer(const size_t rto = TCPConfig::TIMEOUT_DFLT,
             const size_t init_time = TCPConfig::TIMEOUT_DFLT)
    : _running(false), _current_rto(rto), _initial_rto(init_time), _elapsed(0),
      _consecutive_retransmissions(0){};

    unsigned int retransmission() const { return _consecutive_retransmissions; }

    bool is_running() const { return _running; }

    void start() {
        _running = true;
        _current_rto = _initial_rto;
        _elapsed = 0;
        _consecutive_retransmissions = 0;
    }

    void stop() {
        _running = false;
        _consecutive_retransmissions = 0;
    }

    unsigned int current_rto() const { return _current_rto; }

    void restart_after_timeout(const size_t window) {
        if (_running == false)
          return;

        if (window != 0) {
            _current_rto *= 2;
            _consecutive_retransmissions++;
        }

        _elapsed = 0;
    }

    bool advance_and_expired(const size_t ms_since_last_tick) {
        if (_running == false)
            return false;

        _elapsed += ms_since_last_tick;

        if (_elapsed >= _current_rto)
            return true;
        return false;
    }

    bool timer() const { return is_running(); }
    void close() { stop(); }
    unsigned int rtoValue() const { return current_rto(); }
    void double_rto_restart(const size_t window) { restart_after_timeout(window); }
    bool rto(const size_t ms_since_last_tick) { return advance_and_expired(ms_since_last_tick); }
};

class TCPSender {
  private:
    //! our initial sequence number, the number for our SYN.
    WrappingInt32 _isn;

    //! outbound queue of segments that the TCPSender wants sent
    std::queue<TCPSegment> _segments_out{};

    //! retransmission timer for the connection
    unsigned int _initial_retransmission_timeout;

    //! outgoing stream of bytes that have not yet been sent
    ByteStream _stream;

    TCPTimer tcptimer;

    //! the (absolute) sequence number for the next byte to be sent
    uint64_t _next_seqno{0};

    size_t _window_size{1};
    bool _flag_fin{false};
    size_t _bytes_in_flight{0};

    // 标记收到的outstanding_tcp
    std::deque<TCPSegment> _outstanding_segment{};
    std::optional<WrappingInt32> _ackno{};

  public:
    //! Initialize a TCPSender
    TCPSender(const size_t capacity = TCPConfig::DEFAULT_CAPACITY,
              const uint16_t retx_timeout = TCPConfig::TIMEOUT_DFLT,
              const std::optional<WrappingInt32> fixed_isn = {});

    //! \name "Input" interface for the writer
    //!@{
    ByteStream &stream_in() { return _stream; }
    const ByteStream &stream_in() const { return _stream; }
    //!@}

    //! \name Methods that can cause the TCPSender to send a segment
    //!@{

    //! \brief A new acknowledgment was received
    void ack_received(const WrappingInt32 ackno, const uint16_t window_size);

    //! \brief Generate an empty-payload segment (useful for creating empty ACK segments)
    void send_empty_segment();

    void send_noempty_segment(size_t remain);

    //! \brief create and send segments to fill as much of the window as possible
    void fill_window();

    //! \brief Notifies the TCPSender of the passage of time
    void tick(const size_t ms_since_last_tick);
    //!@}

    //! \name Accessors
    //!@{

    //! \brief How many sequence numbers are occupied by segments sent but not yet acknowledged?
    //! \note count is in "sequence space," i.e. SYN and FIN each count for one byte
    //! (see TCPSegment::length_in_sequence_space())
    size_t bytes_in_flight() const;

    //! \brief Number of consecutive retransmissions that have occurred in a row
    unsigned int consecutive_retransmissions() const;

    //! \brief TCPSegments that the TCPSender has enqueued for transmission.
    //! \note These must be dequeued and sent by the TCPConnection,
    //! which will need to fill in the fields that are set by the TCPReceiver
    //! (ackno and window size) before sending.
    std::queue<TCPSegment> &segments_out() { return _segments_out; }
    //!@}

    //! \name What is the next sequence number? (used for testing)
    //!@{

    //! \brief absolute seqno for the next byte to be sent
    uint64_t next_seqno_absolute() const { return _next_seqno; }

    //! \brief relative seqno for the next byte to be sent
    WrappingInt32 next_seqno() const { return wrap(_next_seqno, _isn); }
    //!@}

    size_t remain() {
        size_t ack_absolute_seqno = unwrap(_ackno.value(), _isn, _next_seqno);
        return _next_seqno - ack_absolute_seqno;
    }

};

#endif  // SPONGE_LIBSPONGE_TCP_SENDER_HH
