#include "udp_connection.hh"

#include <stdexcept>
#include <string>

using namespace std;

namespace {
void require(const bool condition, const string &message) {
    if (!condition) {
        throw runtime_error(message);
    }
}
}  // namespace

int main() {
    UDPConnection sender;
    UDPConnection receiver;

    sender.bind(Address("127.0.0.1", 0));
    receiver.bind(Address("127.0.0.1", 0));

    sender.connect(receiver.local_address());
    receiver.connect(sender.local_address());

    require(sender.peer_address() == receiver.local_address(), "sender peer address mismatch");
    require(receiver.peer_address() == sender.local_address(), "receiver peer address mismatch");

    sender.send("first");
    sender.send("second-datagram");

    require(receiver.recv() == "first", "first UDP datagram was not received intact");
    require(receiver.recv() == "second-datagram", "UDP datagram boundaries were not preserved");

    receiver.send("reply");
    require(sender.recv() == "reply", "connected UDP reply failed");

    return 0;
}
