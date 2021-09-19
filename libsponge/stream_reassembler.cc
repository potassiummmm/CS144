#include "stream_reassembler.hh"

// Dummy implementation of a stream reassembler.

// For Lab 1, please replace with a real implementation that passes the
// automated checks run by `make check_lab1`.

// You will need to add private members to the class declaration in `stream_reassembler.hh`

template <typename... Targs>
void DUMMY_CODE(Targs &&... /* unused */) {}

using namespace std;

StreamReassembler::StreamReassembler(const size_t capacity) : _buf(capacity, '\0'), _bitmap(capacity, false), _start_index(0), _unassembled_bytes(0), _eof(false), _output(capacity), _capacity(capacity) {}

//! \details This function accepts a substring (aka a segment) of bytes,
//! possibly out-of-order, from the logical stream, and assembles any newly
//! contiguous substrings and writes them into the output stream in order.
void StreamReassembler::push_substring(const string &data, const size_t index, const bool eof) {
    size_t len = data.length();
    if (index >= _start_index + _output.remaining_capacity()) {
        return;
    }
    if (index + len < _start_index && eof) {
        _eof = true;
    }
    else {
        if (eof && index + len <= _start_index + _output.remaining_capacity()) {
            _eof = true;
        }
        size_t dest = min(_start_index + _output.remaining_capacity(), index + len);
        for (size_t i = index; i < dest; i++) {
            size_t diff = i - _start_index;
            if (i >= _start_index && !_bitmap[diff]) {
                _bitmap[diff] = true;
                _buf[diff] = data[i - index];
                _unassembled_bytes++;
            }
        }
        string str;
        while (_bitmap.front()) {
            str += _buf.front();
            _buf.pop_front();
            _buf.push_back('\0');
            _bitmap.pop_front();
            _bitmap.push_back(false);
        }
        if (!str.empty()) {
            size_t str_len = str.length();
            _output.write(str);
            _unassembled_bytes -= str_len;
            _start_index += str_len;
        }
    }
    if (_eof && empty()) {
        _output.end_input();
    }
}

size_t StreamReassembler::unassembled_bytes() const { return _unassembled_bytes; }

bool StreamReassembler::empty() const { return _unassembled_bytes == 0; }
