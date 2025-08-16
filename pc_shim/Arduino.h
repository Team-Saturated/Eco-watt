#ifndef ARDUINO_H_SHIM
#define ARDUINO_H_SHIM

#include <cstdint>
#include <chrono>
#include <thread>
#include <random>
#include <iostream>
#include <cmath>
#include <ios>

using std::uint32_t;

// --- timing ---
// --- timing ---
inline uint32_t millis() {
  static auto t0 = std::chrono::steady_clock::now();
  return (uint32_t)std::chrono::duration_cast<std::chrono::milliseconds>(
           std::chrono::steady_clock::now() - t0).count();
}

#ifdef _WIN32
  #include <windows.h>
  inline void delay(uint32_t ms) { ::Sleep(ms); }
#else
  inline void delay(uint32_t ms) {
    std::this_thread::sleep_for(std::chrono::milliseconds(ms));
  }
#endif

// --- math ---
#ifndef PI
#define PI 3.14159265358979323846
#endif

// --- random ---
inline long random(long a, long b) {
  static std::mt19937 rng{std::random_device{}()};
  std::uniform_int_distribution<long> dist(a, b - 1);
  return dist(rng);
}
inline void randomSeed(uint32_t) {}

// --- ESP mock ---
struct ESPClass { uint32_t getChipId() const { return 1234567; } };
static ESPClass ESP;

// --- Serial mock ---
struct SerialClass {
  void begin(unsigned long) {}
  void println() { std::cout << std::endl; }

  template<typename T>
  void print(const T& v) { std::cout << v; }

  template<typename T>
  void println(const T& v) { std::cout << v << std::endl; }

  // Arduino-style float/double with precision
  void print(double v, int precision) {
    std::streamsize old_prec = std::cout.precision();
    auto old_flags = std::cout.flags();
    std::cout.setf(std::ios::fixed);
    std::cout.precision(precision);
    std::cout << v;
    std::cout.precision(old_prec);
    std::cout.flags(old_flags);
  }
  void print(float v,   int precision) { print(static_cast<double>(v), precision); }
  void println(double v, int precision) { print(v, precision); std::cout << std::endl; }
  void println(float v,  int precision) { print(static_cast<double>(v), precision); std::cout << std::endl; }
};
static SerialClass Serial;

// flash-string macro stub
#define F(x) x

#endif // ARDUINO_H_SHIM
