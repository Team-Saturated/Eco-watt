#ifndef PC_HTTPCLIENT_H_SHIM
#define PC_HTTPCLIENT_H_SHIM

#include <string>
#include <iostream>

// Mock HTTPClient for PC simulation
class HTTPClient {
public:
    bool begin(const std::string& url) {
        _url = url;
        std::cout << "[HTTPClient] Connecting to: " << url << std::endl;
        return true;
    }
    
    void addHeader(const char* name, const char* value) {
        std::cout << "[HTTPClient] Header: " << name << " = " << value << std::endl;
    }
    
    int POST(const std::string& payload) {
        std::cout << "[HTTPClient] POST payload size: " << payload.size() << " bytes" << std::endl;
        // Simulate successful response
        _response = R"({"status":"OK","received_bytes":)" + std::to_string(payload.size()) + R"(,"config":{"upload_interval":15000},"commands":["noop"]})";
        return 200; // HTTP OK
    }
    
    std::string getString() {
        return _response;
    }
    
    void end() {
        std::cout << "[HTTPClient] Connection closed" << std::endl;
    }
    
private:
    std::string _url;
    std::string _response;
};

// Mock WiFiClient for ESP8266 compatibility
class WiFiClient {
public:
    // Empty mock class
};

#endif // PC_HTTPCLIENT_H_SHIM
