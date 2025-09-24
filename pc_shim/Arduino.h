#ifndef ARDUINO_H_SHIM
#define ARDUINO_H_SHIM

#include <cstdint>
#include <chrono>
#include <thread>
#include <random>
#include <iostream>
#include <cmath>
#include <ios>
#include <string>
#include <sstream>
#include <cstdarg>

using std::uint32_t;

// --- String class for Arduino compatibility ---
class String {
public:
    String() = default;
    String(const char* str) : _str(str ? str : "") {}
    String(const std::string& str) : _str(str) {}
    String(int val) { 
        std::ostringstream ss; 
        ss << val; 
        _str = ss.str(); 
    }
    String(float val) { 
        std::ostringstream ss; 
        ss << val; 
        _str = ss.str(); 
    }
    
    const char* c_str() const { return _str.c_str(); }
    size_t length() const { return _str.length(); }
    bool isEmpty() const { return _str.empty(); }
    
    String operator+(const String& other) const {
        return String(_str + other._str);
    }
    
    String& operator+=(const String& other) {
        _str += other._str;
        return *this;
    }
    
    String& operator+=(const char* str) {
        _str += str;
        return *this;
    }
    
    operator std::string() const { return _str; }
    
private:
    std::string _str;
};

// --- timing ---
inline uint32_t millis() {
  static auto t0 = std::chrono::steady_clock::now();
  return (uint32_t)std::chrono::duration_cast<std::chrono::milliseconds>(
           std::chrono::steady_clock::now() - t0).count();
}

inline uint32_t micros() {
  static auto t0 = std::chrono::steady_clock::now();
  return (uint32_t)std::chrono::duration_cast<std::chrono::microseconds>(
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
  void begin(unsigned long) { std::cout << "[Serial] Started at baud rate\n"; }
  void println() { std::cout << std::endl; }

  template<typename T>
  void print(const T& v) { std::cout << v; }

  template<typename T>
  void println(const T& v) { std::cout << v << std::endl; }

  void print(const String& s) { std::cout << s.c_str(); }
  void println(const String& s) { std::cout << s.c_str() << std::endl; }

  // Printf style
  void printf(const char* format, ...) {
    char buffer[1024];
    std::va_list args;
    va_start(args, format);
    vsnprintf(buffer, sizeof(buffer), format, args);
    va_end(args);
    std::cout << buffer;
  }

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
