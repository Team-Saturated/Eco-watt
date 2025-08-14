#pragma once
#include <Arduino.h>
#include <vector>
#include "Config.h"

class RingBuffer {
public:
  explicit RingBuffer(size_t cap = BUFFER_CAPACITY)
    : _cap(cap), _data(cap) {}

  bool push(const Sample& s) {
    if (_count < _cap) {
      _data[_tail] = s;
      _tail = (_tail + 1) % _cap;
      _count++;
      return true;
    } else {
      // overwrite oldest (ring behavior)
      _data[_tail] = s;
      _tail = (_tail + 1) % _cap;
      _head = (_head + 1) % _cap;
      return false; // indicates overwrite occurred
    }
  }

  void drainTo(std::vector<Sample>& out) {
    out.clear();
    out.reserve(_count);
    while (_count > 0) {
      out.push_back(_data[_head]);
      _head = (_head + 1) % _cap;
      _count--;
    }
  }

  size_t size() const { return _count; }
  size_t capacity() const { return _cap; }

private:
  size_t _cap;
  std::vector<Sample> _data;
  size_t _head{0}, _tail{0}, _count{0};
};
