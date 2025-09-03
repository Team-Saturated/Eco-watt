#include "Buffer.h"

RingBuffer::RingBuffer(size_t capacity)
: _cap(capacity), _head(0), _tail(0), _size(0), _dropped(0), _buf()
{
  _buf.resize(_cap);  // pre-allocate slots
}

bool RingBuffer::push(const Record& r) {
  bool dropped = false;
  if (_size == _cap) {
    // Buffer full: overwrite oldest (advance tail)
    _tail = (_tail + 1) % _cap;
    _size--;
    _dropped++;
    dropped = true;
  }
  _buf[_head] = r;                // copy-assign into slot
  _head = (_head + 1) % _cap;
  _size++;
  return !dropped;
}

void RingBuffer::drainTo(std::vector<Record>& out) {
  // Move (or copy) items in FIFO order
  while (_size > 0) {
    out.push_back(std::move(_buf[_tail])); // move to minimize copies
    _buf[_tail] = Record{};                // reset slot
    _tail = (_tail + 1) % _cap;
    _size--;
  }
}

void RingBuffer::clear() {
  while (_size > 0) {
    _buf[_tail] = Record{};
    _tail = (_tail + 1) % _cap;
    _size--;
  }
}
