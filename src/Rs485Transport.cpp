#include "Rs485Transport.h"
#include "Config.h"

Rs485Transport::Rs485Transport(HardwareSerial& ser, uint32_t baud, int deRePin, uint32_t timeoutMs)
: _ser(ser), _timeout(timeoutMs), _deRe(deRePin) {
#if defined(ESP32)
  _ser.begin(baud, SERIAL_8N1);
#else
  _ser.begin(baud);
#endif
  pinMode(_deRe, OUTPUT);
  setTx(false);
}

void Rs485Transport::setTx(bool en) {
  digitalWrite(_deRe, en ? HIGH : LOW); // DE/RE active-high
}

TransportResult Rs485Transport::exchange(const std::vector<uint8_t>& req) {
  TransportResult r;

  // Write
  setTx(true);
  _ser.write(req.data(), req.size());
  _ser.flush();
  setTx(false);

  // Read minimal 0x03 response: slave, func, byteCount, data(2*qty), crc(2)
  uint32_t start = millis();
  while ((millis() - start) < _timeout) {
    while (_ser.available()) r.bytes.push_back(_ser.read());
    if (r.bytes.size() >= 5) { r.ok = true; return r; }
    delay(2);
  }
  r.ok = false;
  r.error = "RS485 timeout";
  return r;
}
