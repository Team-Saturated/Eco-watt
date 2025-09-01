// #include <Arduino.h>
// #include "Config.h"
// #include "Acquisition.h"
// #include "Buffer.h"
// #include "Uploader.h"
// #include <thread>
// #include <vector>

// int main() {
//   Serial.begin(115200);
//   Serial.println("\nEcoWatt M1 (PC Simulation)");

//   Acquisition ac; 
//   RingBuffer buf(BUFFER_CAPACITY); 
//   Uploader up;
//   ac.begin(); 
//   up.begin();

//   uint32_t lastPoll = millis(), lastUp = millis();

//   for (;;) {                             // infinite
//     uint32_t now = millis();

//     if (now - lastPoll >= POLL_INTERVAL_MS) {
//       lastPoll = now;
//       Sample s = ac.acquire();
//       bool ok = buf.push(s);
//       if (!ok) Serial.println("[WARN] Buffer overwrite occurred.");
//       Serial.print("[POLL] t="); Serial.print((unsigned long)s.t_ms);
//       Serial.print(" v=");       Serial.print(s.v, 2);
//       Serial.print(" i=");       Serial.println(s.i, 2);
//     }

//     if (now - lastUp >= UPLOAD_INTERVAL_MS) {
//       lastUp = now;
//       std::vector<Sample> batch;
//       buf.drainTo(batch);
//       up.upload(batch);
//     }

//     delay(1);
//   }

//   return 0; // never reached
// }
