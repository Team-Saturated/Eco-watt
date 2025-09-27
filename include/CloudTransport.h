#pragma once
#include "Transport.h"


/**
 * @class CloudTransport
 * @brief Concrete transport that exchanges frames with a cloud endpoint.
 *
 * Typical flow:
 * - Construct with the base URL, an authorization header value, and a timeout.
 * - Call @ref exchange to POST/PUT the request bytes and receive response bytes.
 */
class CloudTransport : public ITransport {
  public:
    /**
     * @brief Create a cloud transport.
     * @param url Base endpoint.
     * @param auth Authorization header value.
     * @param timeoutMs Request timeout in milliseconds.
     */
    CloudTransport(const String& url, const String& auth, uint32_t timeoutMs);
    TransportResult exchange(const std::vector<uint8_t>& request) override;

  private:
    String _url;
    String _auth;
    uint32_t _timeout;
  };
