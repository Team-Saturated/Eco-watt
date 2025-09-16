#include "Buffer.h"

RingBuffer::RingBuffer(size_t capacity)
: _cap(capacity),
  _head(0),
  _tail(0),
  _size(0),
  _dropped(0),
  _buf(capacity) {}

bool RingBuffer::push(const Record& r) {
  bool dropped = false;

  if (_cap == 0) return false;

  // If full, overwrite the oldest (advance tail)
  if (_size == _cap) {
    _tail = (_tail + 1) % _cap;
    _dropped++;
    dropped = true;
    // size stays at _cap
  } else {
    _size++;
  }

  _buf[_head] = r;
  _head = (_head + 1) % _cap;
  return !dropped;
}

void RingBuffer::drainTo(std::vector<Record>& out) {
  while (_size) {
    out.push_back(_buf[_tail]);
    _tail = (_tail + 1) % _cap;
    _size--;
  }
}

void RingBuffer::clear() {
  _head = _tail = _size = 0;
  // leave _dropped as-is for introspection
}
