#include "udp_connection.hh"

void UDPConnection::bind(const Address &local_address) { _socket.bind(local_address); }

void UDPConnection::connect(const Address &peer_address) { _socket.connect(peer_address); }

void UDPConnection::send(const std::string &datagram) { _socket.send(datagram); }

std::string UDPConnection::recv(const std::size_t max_datagram_size) {
    return _socket.recv(max_datagram_size).payload;
}

Address UDPConnection::local_address() const { return _socket.local_address(); }

Address UDPConnection::peer_address() const { return _socket.peer_address(); }
