/*
 * Connection management
 */
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <termios.h>
#include <string>

#include <string.h> // for memset
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>

#include <iostream>

#include "util.h"
#include "connection.h"
#include "expatwrap.h"

std::ostream &operator<<(std::ostream &, const addrinfo &);

struct AddressLookup {
public:
    addrinfo *addrInfo;
    AddressLookup(const std::string &host, const std::string &service, int hintflags);
    ~AddressLookup();
};

ResolverException::ResolverException(const std::string &func, int gaie)
    : gaie(gaie)
    , function(func)
{
}

ResolverException::~ResolverException()
    throw()
{
}

std::ostream &
ResolverException::describe(std::ostream &os) const
{
    return os << function << ": " << gai_strerror(gaie);
}

AddressLookup::AddressLookup(const std::string &host, const std::string &service,
        int hintflags)
{
    int rc;
    struct addrinfo hints;
    // Only attempt to look up stream services.
    memset(&hints, 0, sizeof hints);
    hints.ai_family = PF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags = hintflags;
    if ((rc = getaddrinfo(host == "" ? 0 : host.c_str(), service.c_str(), &hints, &addrInfo)) != 0)
	throw ResolverException("getaddrinfo", rc);
}

AddressLookup::~AddressLookup()
{
    freeaddrinfo(addrInfo);
}


std::ostream &operator<<(std::ostream &os, const addrinfo &ai)
{

    char host[NI_MAXHOST];
    char service[NI_MAXSERV];
    int rc = getnameinfo(ai.ai_addr, ai.ai_addrlen, host, sizeof host, service,
		sizeof service, NI_NUMERICHOST|NI_NUMERICSERV);

    if (!rc)
        return os << host << ":" << service;
    else
        return os << "(name resolution failed)";
}

NetworkConnection::NetworkConnection()
    : Connection()
    , host("localhost")
    , service("telnet")
{
}

ListenConnection::ListenConnection()
    : Connection()
    , host("")
    , service("8080")
{
}

ListenConnection::~ListenConnection()
{
}

NetworkConnection::~NetworkConnection()
{
}

Connection::Connection()
{
}

Connection::~Connection()
{
}

int
NetworkConnection::connect() const
{
    int fd = -1;
    std::clog << "resolving host " << host << " for service " << service << std::endl;
    AddressLookup al(host, service, 0);
    for (addrinfo *ai = al.addrInfo; ai; ai = ai->ai_next) {
	if ((fd = socket(ai->ai_family, ai->ai_socktype, ai->ai_protocol)) != -1) {
	    std::clog << "trying " << *ai;
	    if (::connect(fd, ai->ai_addr, ai->ai_addrlen) == 0) {
		std::clog << "... success (fd = " << fd << ")" << std::endl;
		return fd;
	    }
	    std::clog << "... failed" << std::endl;
	    ::close(fd);
	}
    }
    throw ResolverException("no usable address found", 0);
}

int
ListenConnection::connect() const
{
    int fd = -1;
    std::clog << "resolving host " << host << "for service " << service << std::endl;
    AddressLookup al(host, service, AI_PASSIVE);
    for (addrinfo *ai = al.addrInfo; ai; ai = ai->ai_next) {
	if ((fd = socket(ai->ai_family, ai->ai_socktype, ai->ai_protocol)) != -1) {
            std::clog << "trying " << *ai;
	    if (::bind(fd, ai->ai_addr, ai->ai_addrlen) == 0) {
		std::clog << "... success" << std::endl;
                if (::listen(fd, 10) == 0) {
                    sockaddr_storage sas;
                    socklen_t sl = sizeof sas;
                    int fd2 = ::accept(fd, (struct sockaddr *)&sas, &sl);
                    if (fd2 != -1) {
                        std::clog << "accepted call with fd " <<  fd2 << std::endl;
                        close(fd);
                        return fd2;
                    }
                }
	    }
	    std::clog << "... failed" << std::endl;
	    ::close(fd);
	}
    }
    throw ResolverException("no usable address found", 0);
}

ModemConnection::ModemConnection()
    : Connection()
    , device("/dev/cuaa0")
    , speed(9600)
    , bits(8)
    , flowXonXoff(false)
    , flowHard(true)
    , parity(none)
{
}

int
ModemConnection::connect() const
{
    int fd = open(device.c_str(), O_RDWR);
    if (fd == -1)
	throw UnixException(errno, "cannot open modem");

    termios io;

    std::clog << "using terminal device " << device << ", speed=" << speed << std::endl;
    if (tcgetattr(fd, &io) == -1) {
	close(fd);
	throw UnixException(errno, "cannot get modem configuration");
    }

    // Speed
    cfsetspeed(&io, speed);

    // Input Flags
    io.c_iflag &= ~(ICANON|ISTRIP|INPCK|PARMRK|INLCR|IGNCR|ICRNL);
    io.c_iflag |= IGNBRK;
    if (flowXonXoff)
	io.c_iflag |= IXOFF|IXON;
    else
	io.c_iflag &= ~(IXOFF|IXON);

    // Control Flags
    io.c_cflag |= CREAD;
    if (parity)
	io.c_cflag |= PARENB;
    else
	io.c_cflag &= ~PARENB;

    switch (parity) {
        case odd:
            io.c_cflag |= PARODD;
            break;
        default:
            io.c_cflag &= ~PARODD;
            break;
    }

    io.c_cflag &= ~CSIZE;
    io.c_cflag |= bits == 7 ? CS7 : bits == 6 ? CS6 : CS8;

#ifdef __linux__
#define HFLOW  CRTSCTS
#endif

#ifdef __FreeBSD__
#define HFLOW  (CCTS_OFLOW | CRTS_IFLOW)
#endif
    if (flowHard)
	io.c_cflag |= HFLOW;
    else
	io.c_cflag &= ~HFLOW;

    // Output Flags (disable all output processing)
    io.c_oflag &= ~(OPOST|ONLCR|OCRNL);

    if (tcsetattr(fd, TCSANOW, &io)) {
	close(fd);
	throw UnixException(errno, "cannot set modem configuration");
    }
    return fd;
}

ModemConnection::~ModemConnection()
{
}
