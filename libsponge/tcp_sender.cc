#include "tcp_sender.hh"

#include "tcp_config.hh"

#include <random>

// Dummy implementation of a TCP sender

// For Lab 3, please replace with a real implementation that passes the
// automated checks run by `make check_lab3`.

template <typename... Targs>
void DUMMY_CODE(Targs &&... /* unused */) {}

using namespace std;

//! \param[in] capacity the capacity of the outgoing byte stream
//! \param[in] retx_timeout the initial amount of time to wait before retransmitting the oldest outstanding segment
//! \param[in] fixed_isn the Initial Sequence Number to use, if set (otherwise uses a random ISN)
TCPSender::TCPSender(const size_t capacity, const uint16_t retx_timeout, const std::optional<WrappingInt32> fixed_isn)
    : _timer{retx_timeout}
    , _isn(fixed_isn.value_or(WrappingInt32{random_device()()}))
    , _initial_retransmission_timeout{retx_timeout}
    , _RTO{retx_timeout}
    , _stream(capacity) {}

uint64_t TCPSender::bytes_in_flight() const { return _next_seqno - _send_base; }

void TCPSender::fill_window() {
    if (_fin_sent || _window_size == 0 || (_syn_sent && _stream.buffer_empty() && !_stream.input_ended())) {
        return;
    }
    if (!_syn_sent) {  // send SYN
        TCPSegment seg;
        seg.header().syn = true;
        seg.header().seqno = wrap(0, _isn);
        _next_seqno = 1;
        _window_size--;
        _segments_out.push(seg);
        _outstanding_segments.push_back(seg);
        _syn_sent = true;
    } else if (_stream.eof()) {  // send FIN
        TCPSegment seg;
        seg.header().fin = true;
        seg.header().seqno = wrap(_next_seqno, _isn);
        _next_seqno++;
        _window_size--;
        _segments_out.push(seg);
        _outstanding_segments.push_back(seg);
        _fin_sent = true;
    } else {
        while (!_stream.buffer_empty() && _window_size != 0) {
            size_t len = min(TCPConfig::MAX_PAYLOAD_SIZE, _window_size);
            TCPSegment seg;
            seg.header().seqno = wrap(_next_seqno, _isn);
            seg.payload() = _stream.read(len);
            _window_size -= seg.length_in_sequence_space();
            if (_window_size != 0 && _stream.eof()) {
                seg.header().fin = true;
                _fin_sent = true;
                _window_size--;
            }
            _next_seqno += seg.length_in_sequence_space();
            _segments_out.push(seg);
            _outstanding_segments.push_back(seg);
            if (_fin_sent) {
                break;
            }
        }
    }
    if (!_timer.running()) {
        _timer.reset(_RTO);
        _timer.start();
    }
}

//! \param ackno The remote receiver's ackno (acknowledgment number)
//! \param window_size The remote receiver's advertised window size
void TCPSender::ack_received(const WrappingInt32 ackno, const uint16_t window_size) {
    uint64_t abs_ackno = unwrap(ackno, _isn, _send_base);
    if (_send_base == 0 && abs_ackno == 1) {  // SYN acked
        _send_base = 1;
        _outstanding_segments.pop_front();
    } else if (_fin_sent && _outstanding_segments.size() == 1 && abs_ackno == _next_seqno) {  // FIN acked
        _send_base += _outstanding_segments.front().length_in_sequence_space();
        _outstanding_segments.pop_front();
    } else if (!_outstanding_segments.empty() &&
               abs_ackno >= _send_base + _outstanding_segments.front().length_in_sequence_space()) {  // new ack
        auto seg_len = _outstanding_segments.front().length_in_sequence_space();
        auto seg_abs_seqno = unwrap(_outstanding_segments.front().header().seqno, _isn, _send_base);
        while (seg_len + seg_abs_seqno <= abs_ackno) {
            _send_base += seg_len;
            _outstanding_segments.pop_front();
            if (_outstanding_segments.empty()) {
                break;
            }
            seg_len = _outstanding_segments.front().length_in_sequence_space();
            seg_abs_seqno = unwrap(_outstanding_segments.front().header().seqno, _isn, _send_base);
        }
        _RTO = _initial_retransmission_timeout;
        if (!_outstanding_segments.empty()) {
            _timer.reset(_RTO);
            _timer.start();
        }
        _consecutive_retransmissions = 0;
    }
    if (bytes_in_flight() == 0) {
        _timer.stop();
    } else if (bytes_in_flight() >= window_size) {
        _window_size = 0;
        return;
    }
    if (window_size == 0) {
        _window_size = 1;
        _win_empty = true;
    } else {
        _window_size = window_size;
        _win_empty = false;
    }
    fill_window();
}

//! \param[in] ms_since_last_tick the number of milliseconds since the last call to this method
void TCPSender::tick(const size_t ms_since_last_tick) {
    _timer.tick(ms_since_last_tick);
    if (_timer.running() && _timer.expired()) {
        TCPSegment earliest_seg = _outstanding_segments.front();
        _segments_out.push(earliest_seg);
        if (!_win_empty) {
            _consecutive_retransmissions++;
            _RTO <<= 1;
        }
        _timer.reset(_RTO);
    }
}

unsigned int TCPSender::consecutive_retransmissions() const { return _consecutive_retransmissions; }

void TCPSender::send_empty_segment() {
    TCPSegment seg;
    seg.header().seqno = wrap(_next_seqno, _isn);
    _segments_out.push(seg);
}
