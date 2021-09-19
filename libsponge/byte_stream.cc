#include "byte_stream.hh"

#include <algorithm>

// Dummy implementation of a flow-controlled in-memory byte stream.

// For Lab 0, please replace with a real implementation that passes the
// automated checks run by `make check_lab0`.

// You will need to add private members to the class declaration in `byte_stream.hh`

template <typename... Targs>
void DUMMY_CODE(Targs &&... /* unused */) {}

using namespace std;

ByteStream::ByteStream(const size_t capacity) { this->_capacity = capacity; }

size_t ByteStream::write(const string &data) {
    if (remaining_capacity() == 0) {
        return 0;
    }
    size_t len = min(remaining_capacity(), data.size());
    for (size_t i = 0; i < len; ++i) {
        _buf.push_back(data[i]);
    }
    _bytes_written += len;
    return len;
}

//! \param[in] len bytes will be copied from the output side of the buffer
string ByteStream::peek_output(const size_t len) const {
    size_t len2peek = min(buffer_size(), len);
    return string().assign(_buf.begin(), _buf.begin() + len2peek);
}

//! \param[in] len bytes will be removed from the output side of the buffer
void ByteStream::pop_output(const size_t len) {
    size_t len2pop = min(buffer_size(), len);
    for (size_t i = 0; i < len2pop; i++) {
        _buf.pop_front();
    }
    _bytes_read += len2pop;
}

//! Read (i.e., copy and then pop) the next "len" bytes of the stream
//! \param[in] len bytes will be popped and returned
//! \returns a string
std::string ByteStream::read(const size_t len) {
    string output = peek_output(len);
    pop_output(len);
    return output;
}

void ByteStream::end_input() { _end_of_input = true; }

bool ByteStream::input_ended() const { return _end_of_input; }

size_t ByteStream::buffer_size() const { return _buf.size(); }

bool ByteStream::buffer_empty() const { return _buf.empty(); }

bool ByteStream::eof() const { return _end_of_input && buffer_empty(); }

size_t ByteStream::bytes_written() const { return _bytes_written; }

size_t ByteStream::bytes_read() const { return _bytes_read; }

size_t ByteStream::remaining_capacity() const { return _capacity - buffer_size(); }