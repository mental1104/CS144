#include "wrapping_integers.hh"

// Dummy implementation of a 32-bit wrapping integer

// For Lab 2, please replace with a real implementation that passes the
// automated checks run by `make check_lab2`.

template <typename... Targs>
void DUMMY_CODE(Targs &&... /* unused */) {}

using namespace std;

//! Transform an "absolute" 64-bit sequence number (zero-indexed) into a WrappingInt32
//! \param n The input absolute 64-bit sequence number
//! \param isn The initial sequence number
WrappingInt32 wrap(uint64_t n, WrappingInt32 isn) {
    return isn + static_cast<uint32_t>(n);
}

//! Transform a WrappingInt32 into an "absolute" 64-bit sequence number (zero-indexed)
//! \param n The relative sequence number
//! \param isn The initial sequence number
//! \param checkpoint A recent absolute 64-bit sequence number
//! \returns the 64-bit sequence number that wraps to `n` and is closest to `checkpoint`
//!
//! \note Each of the two streams of the TCP connection has its own ISN. One stream
//! runs from the local TCPSender to the remote TCPReceiver and has one ISN,
//! and the other stream runs from the remote TCPSender to the local TCPReceiver and
//! has a different ISN.
uint64_t unwrap(WrappingInt32 n, WrappingInt32 isn, uint64_t checkpoint) {
    uint32_t offset = n.raw_value() - isn.raw_value();
    uint64_t mask   = static_cast<uint64_t>(UINT32_MAX) << 32;
    uint64_t top32  = checkpoint & mask;
    uint64_t a      = (top32 - (1ul << 32)) + offset;
    uint64_t diff_a = checkpoint - a;
    uint64_t b      = top32 + offset;
    uint64_t diff_b = (checkpoint > b)? (checkpoint-b):(b-checkpoint);
    uint64_t c      = (top32 + (1ul << 32)) + offset;
    uint64_t diff_c = c - checkpoint;
    
    if(top32 == 0)
        return (diff_b < diff_c)? b : c;

    if(diff_a < diff_b)
        return (diff_a < diff_c)? a : c;

    return (diff_b < diff_c)? b :c;
}
