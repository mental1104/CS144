#ifndef SPONGE_LIBSPONGE_UDP_CONNECTION_HH
#define SPONGE_LIBSPONGE_UDP_CONNECTION_HH

#include "util/socket.hh"

#include <cstddef>
#include <string>

// Teaching adapter for a connected UDP socket.
//
// Unlike TCPConnection, this class does not implement a transport protocol
// state machine. UDP connect() only records a default peer in the kernel so
// send()/recv() can omit an address; it performs no handshake and adds no
// delivery, ordering, duplicate-suppression, or congestion-control guarantees.
class UDPConnection {
  private:
    UDPSocket _socket{};

  public:
    UDPConnection() = default;

    // Choose the local IP address and port. Port zero asks Linux to allocate
    // an ephemeral port.
    void bind(const Address &local_address);

    // Record the default peer. For UDP this does not exchange packets.
    void connect(const Address &peer_address);

    // Send exactly one UDP datagram to the connected peer.
    void send(const std::string &datagram);

    // Receive exactly one UDP datagram from the connected peer.
    std::string recv(std::size_t max_datagram_size = 65536);

    Address local_address() const;
    Address peer_address() const;
};

#endif  // SPONGE_LIBSPONGE_UDP_CONNECTION_HH
