#include "tcp_receiver.hh"

// Dummy implementation of a TCP receiver

// For Lab 2, please replace with a real implementation that passes the
// automated checks run by `make check_lab2`.

template <typename... Targs>
void DUMMY_CODE(Targs &&... /* unused */) {}

using namespace std;

void TCPReceiver::segment_received(const TCPSegment &seg) {
    const TCPHeader header = seg.header();
    string data = seg.payload().copy();
    bool eof = false;
    if (!_syn_recvd && header.syn) {
        _syn_recvd = true;
        _isn = header.seqno;
        if (header.fin) {
            eof = true;
            _fin_recvd = true;
        }
        _reassembler.push_substring(data, 0, eof);
        return;
    }
    if (!_syn_recvd) {
        return;
    }
    if (header.fin) {
        eof = true;
        _fin_recvd = true;
    }
    uint64_t checkpoint = stream_out().bytes_written();
    uint64_t abs_seqno = unwrap(header.seqno, _isn, checkpoint);
    _reassembler.push_substring(data, abs_seqno - 1, eof);
}

optional<WrappingInt32> TCPReceiver::ackno() const {
    if (!_syn_recvd) {
        return nullopt;
    }
    uint64_t abs_seqno = stream_out().bytes_written();
    if (_fin_recvd && stream_out().input_ended()) {
        abs_seqno++;
    }
    return optional<WrappingInt32>(wrap(abs_seqno + 1, _isn));
}

size_t TCPReceiver::window_size() const { return stream_out().remaining_capacity(); }
