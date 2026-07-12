#ifndef INTERTWINE_CPP_FRAMEWORK_I_TRANSPORT_HPP
#define INTERTWINE_CPP_FRAMEWORK_I_TRANSPORT_HPP

#include "intertwine/fw/TransportTypes.hpp"

#include <string>

namespace intertwine {
namespace fw {

class ITransport {
public:
    virtual ~ITransport() {}

    virtual int sendRequest(const Request& req, Response& resp) = 0;
    virtual int receiveResponse(Response& resp) = 0;
    virtual bool connect() = 0;
    virtual void disconnect() = 0;
    virtual bool isConnected() const = 0;
    virtual std::string getTransportType() const = 0;
};

} // namespace fw
} // namespace intertwine

#endif
