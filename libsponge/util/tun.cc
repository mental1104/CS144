#include "tun.hh"

#include "util.hh"

#ifdef __linux__
#include <cstring>
#include <fcntl.h>
#include <linux/if.h>
#include <linux/if_tun.h>
#include <sys/ioctl.h>
#else
#include <stdexcept>
#endif

using namespace std;

#ifdef __linux__
static constexpr const char *CLONEDEV = "/dev/net/tun";

//! \param[in] devname is the name of the TUN or TAP device, specified at its creation.
//! \param[in] is_tun is `true` for a TUN device (expects IP datagrams), or `false` for a TAP device (expects Ethernet frames)
//!
//! To create a TUN device, you should already have run
//!
//!     ip tuntap add mode tun user `username` name `devname`
//!
//! as root before calling this function.

TunTapFD::TunTapFD(const string &devname, const bool is_tun)
    : FileDescriptor(SystemCall("open", open(CLONEDEV, O_RDWR))) {
    struct ifreq tun_req {};

    tun_req.ifr_flags = (is_tun ? IFF_TUN : IFF_TAP) | IFF_NO_PI;  // tun device with no packetinfo

    // copy devname to ifr_name, making sure to null terminate

    strncpy(static_cast<char *>(tun_req.ifr_name), devname.data(), IFNAMSIZ - 1);
    tun_req.ifr_name[IFNAMSIZ - 1] = '\0';

    SystemCall("ioctl", ioctl(fd_num(), TUNSETIFF, static_cast<void *>(&tun_req)));
}
#else
namespace {
int unsupported_tuntap_fd() {
    throw runtime_error("Linux TUN/TAP devices are not supported on this platform");
}
}

TunTapFD::TunTapFD(const string &, const bool) : FileDescriptor(unsupported_tuntap_fd()) {}
#endif
