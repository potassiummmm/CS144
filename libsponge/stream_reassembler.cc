#include "stream_reassembler.hh"

// Dummy implementation of a stream reassembler.

// For Lab 1, please replace with a real implementation that passes the
// automated checks run by `make check_lab1`.

// You will need to add private members to the class declaration in `stream_reassembler.hh`

template <typename... Targs>
void DUMMY_CODE(Targs &&... /* unused */) {}

using namespace std;

StreamReassembler::StreamReassembler(const size_t capacity)
    : _unassembled_bytes(0), _eof(false), _output(capacity), _capacity(capacity) {}

void StreamReassembler::insert_block(Block block) {
    if (_buf.empty()) {
        _buf.insert(block);
        _unassembled_bytes += block._data.size();
        return;
    }
    Block tmp = block;
    auto it = _buf.lower_bound(block);
    size_t x = tmp._index, sz = tmp._data.size();
    if (it != _buf.begin()) {
        it--;
        if (x < it->_index + it->_data.size()) {  // Merge block
            if (x + sz <= it->_index + it->_data.size())
                return;
            tmp._data = it->_data + tmp._data.substr(it->_index + it->_data.size() - x);
            tmp._index = it->_index;
            x = tmp._index;
            sz = tmp._data.size();
            _unassembled_bytes -= it->_data.size();
            _buf.erase(it++);
        } else
            it++;
    }
    while (it != _buf.end() && x + sz > it->_index) {
        if (x >= it->_index && x + sz < it->_index + it->_data.size())
            return;
        if (x + sz < it->_index + it->_data.size()) {
            tmp._data += it->_data.substr(x + sz - it->_index);
        }
        _unassembled_bytes -= it->_data.size();
        _buf.erase(it++);
    }
    _buf.insert(tmp);
    _unassembled_bytes += tmp._data.size();
}

//! \details This function accepts a substring (aka a segment) of bytes,
//! possibly out-of-order, from the logical stream, and assembles any newly
//! contiguous substrings and writes them into the output stream in order.
void StreamReassembler::push_substring(const string &data, const size_t index, const bool eof) {
    size_t len = data.length();
    size_t first_unread = _output.bytes_read();
    size_t first_unassembled = _output.bytes_written();
    size_t first_unaccept = first_unread + _capacity;
    if (index >= first_unaccept || index + len < first_unassembled) {
        return;
    }
    if (index + len < first_unassembled && eof) {
        _eof = true;
    } else {
        if (eof && index + len <= first_unaccept) {
            _eof = true;
        }
        Block block(index, data);
        if (index + len > first_unaccept) {
            block._data = block._data.substr(0, first_unaccept - index);
        }
        if (index > first_unassembled) {
            insert_block(block);
        } else {
            _output.write(data.substr(first_unassembled - index));
            auto it = _buf.begin();
            while (it != _buf.end() && it->_index <= _output.bytes_written()) {
                if (it->_index + it->_data.size() > index + block._data.size())
                    _output.write(it->_data.substr(_output.bytes_written() - it->_index));
                _unassembled_bytes -= it->_data.size();
                _buf.erase(it++);
            }
        }
    }
    if (_eof && empty()) {
        _output.end_input();
    }
}

size_t StreamReassembler::unassembled_bytes() const { return _unassembled_bytes; }

bool StreamReassembler::empty() const { return _unassembled_bytes == 0; }
