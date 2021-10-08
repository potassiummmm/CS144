#include "network_interface.hh"

#include "arp_message.hh"
#include "ethernet_frame.hh"

#include <iostream>

// Dummy implementation of a network interface
// Translates from {IP datagram, next hop address} to link-layer frame, and from link-layer frame to IP datagram

// For Lab 5, please replace with a real implementation that passes the
// automated checks run by `make check_lab5`.

// You will need to add private members to the class declaration in `network_interface.hh`

template <typename... Targs>
void DUMMY_CODE(Targs &&... /* unused */) {}

using namespace std;

//! \param[in] ethernet_address Ethernet (what ARP calls "hardware") address of the interface
//! \param[in] ip_address IP (what ARP calls "protocol") address of the interface
NetworkInterface::NetworkInterface(const EthernetAddress &ethernet_address, const Address &ip_address)
    : _ethernet_address(ethernet_address), _ip_address(ip_address) {
    cerr << "DEBUG: Network interface has Ethernet address " << to_string(_ethernet_address) << " and IP address "
         << ip_address.ip() << "\n";
}

void NetworkInterface::send_arp_request(const uint32_t next_hop_ip) {
    ARPMessage msg;
    msg.opcode = ARPMessage::OPCODE_REQUEST;
    msg.sender_ethernet_address = _ethernet_address;
    msg.sender_ip_address = _ip_address.ipv4_numeric();
    msg.target_ethernet_address = {0, 0, 0, 0, 0, 0};
    msg.target_ip_address = next_hop_ip;
    EthernetFrame frame;
    frame.header().type = EthernetHeader::TYPE_ARP;
    frame.header().src = _ethernet_address;
    frame.header().dst = ETHERNET_BROADCAST;
    frame.payload() = msg.serialize();
    _frames_out.push(frame);
}

//! \param[in] dgram the IPv4 datagram to be sent
//! \param[in] next_hop the IP address of the interface to send it to (typically a router or default gateway, but may also be another host if directly connected to the same network as the destination)
//! (Note: the Address type can be converted to a uint32_t (raw 32-bit IP address) with the Address::ipv4_numeric() method.)
void NetworkInterface::send_datagram(const InternetDatagram &dgram, const Address &next_hop) {
    // convert IP address of next hop to raw 32-bit representation (used in ARP header)
    const uint32_t next_hop_ip = next_hop.ipv4_numeric();
    EthernetFrame frame;
    frame.header().type = EthernetHeader::TYPE_IPv4;
    frame.header().src = _ethernet_address;
    frame.payload() = dgram.serialize();
    if (_table.count(next_hop_ip) && _timer <= _table[next_hop_ip]._ttl) {
        frame.header().dst = _table[next_hop_ip]._addr;
        _frames_out.push(frame);
    } else {
        if (!_req_time.count(next_hop_ip) || _timer > _req_time[next_hop_ip] + 5000) {
            send_arp_request(next_hop_ip);
            _req_time[next_hop_ip] = _timer;
        }
        _frames_pending.push({frame, next_hop_ip});
    }
}

//! \param[in] frame the incoming Ethernet frame
optional<InternetDatagram> NetworkInterface::recv_frame(const EthernetFrame &frame) {
    if (frame.header().dst != ETHERNET_BROADCAST && frame.header().dst != _ethernet_address) {
        return nullopt;
    }
    if (frame.header().type == EthernetHeader::TYPE_IPv4) {
        InternetDatagram dgram;
        if (dgram.parse(frame.payload()) == ParseResult::NoError) {
            return dgram;
        }
        return nullopt;
    } else if (frame.header().type == EthernetHeader::TYPE_ARP) {
        ARPMessage msg;
        if (msg.parse(frame.payload()) == ParseResult::NoError) {
            uint32_t ip = msg.sender_ip_address;
            _table[ip]._addr = msg.sender_ethernet_address;
            _table[ip]._ttl = _timer + 30000;  // 30 seconds
            if (msg.opcode == ARPMessage::OPCODE_REQUEST && msg.target_ip_address == _ip_address.ipv4_numeric()) {
                ARPMessage reply;
                reply.opcode = ARPMessage::OPCODE_REPLY;
                reply.sender_ethernet_address = _ethernet_address;
                reply.sender_ip_address = _ip_address.ipv4_numeric();
                reply.target_ethernet_address = msg.sender_ethernet_address;
                reply.target_ip_address = msg.sender_ip_address;
                EthernetFrame ef;
                ef.header().type = EthernetHeader::TYPE_ARP;
                ef.header().src = _ethernet_address;
                ef.header().dst = msg.sender_ethernet_address;
                ef.payload() = reply.serialize();
                _frames_out.push(ef);
            }
            while (!_frames_pending.empty()) {
                PendingFrame pf = _frames_pending.front();
                if (_table.count(pf._ip) && _timer <= _table[pf._ip]._ttl) {
                    pf._frame.header().dst = _table[pf._ip]._addr;
                    _frames_pending.pop();
                    _frames_out.push(pf._frame);
                } else {
                    break;
                }
            }
        } else {
            return nullopt;
        }
    }
    return nullopt;
}

//! \param[in] ms_since_last_tick the number of milliseconds since the last call to this method
void NetworkInterface::tick(const size_t ms_since_last_tick) { _timer += ms_since_last_tick; }
