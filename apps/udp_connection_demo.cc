#include "udp_connection.hh"

#include <iostream>
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
    UDPConnection left;
    UDPConnection right;

    left.bind(Address("127.0.0.1", 0));
    right.bind(Address("127.0.0.1", 0));

    left.connect(right.local_address());
    right.connect(left.local_address());

    left.send("one");
    left.send("two-two");

    const string first = right.recv();
    const string second = right.recv();

    require(first == "one", "first UDP datagram changed or merged");
    require(second == "two-two", "second UDP datagram changed or merged");

    cout << "left:  " << left.local_address().to_string() << " -> " << left.peer_address().to_string() << '\n';
    cout << "right: " << right.local_address().to_string() << " -> " << right.peer_address().to_string() << '\n';
    cout << "first:  size=" << first.size() << " payload=" << first << '\n';
    cout << "second: size=" << second.size() << " payload=" << second << '\n';
    cout << "two send() calls produced two recv() results: UDP preserves datagram boundaries.\n";

    return 0;
}
