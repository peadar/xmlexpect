/*
 * Connection management.
 */
#ifndef connection_h_guard
#define connection_h_guard

class Connection {
public:
    Connection();
    virtual ~Connection();
    virtual int connect() const = 0;
};

class ResolverException : public Exception {
    int gaie;
    std::string function;
public:
    std::ostream &describe(std::ostream &) const;
    ResolverException(const std::string &function, int gaie);
    ~ResolverException() throw();
};

struct ModemConnection : public Connection {
    std::string device;
    int speed;
    int bits;
    bool flowXonXoff;
    bool flowHard;
    enum Parity { none, even, odd };
    Parity parity;
    ModemConnection();
    ~ModemConnection();
    int connect() const;
};

struct NetworkConnection : public Connection {
    std::string host;
    std::string service;
    NetworkConnection();
    ~NetworkConnection();
    int connect() const;
};

struct ListenConnection : public Connection {
    std::string host;
    std::string service;
    ListenConnection();
    ~ListenConnection();
    int connect() const;
};

#endif
