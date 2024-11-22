use crate::pdu;
use crate::pdu::PDU;
use pdu::*;

use mio::net::{TcpStream, UdpSocket};
use std::collections::VecDeque;
use std::net::SocketAddr;

use std::io::prelude::*;
use std::io::ErrorKind;

const BUFFER_SIZE: usize = 25600;

pub type Message = (PDU, SocketAddr);

pub struct TcpWrapper {
    incoming_queue: VecDeque<Message>,
    buffer: [u8; BUFFER_SIZE],
    buffer_fill: usize,
}

impl TcpWrapper {
    pub fn new() -> Self {
        TcpWrapper {
            incoming_queue: VecDeque::new(),
            buffer: [0; BUFFER_SIZE],
            buffer_fill: 0,
        }
    }

    pub fn send(&self, socket: &mut TcpStream, pdu: PDU) {
        let bytes = pdu.to_bytes();
        let mut sent = 0;
        while sent != bytes.len() {
            match socket.write(&bytes[sent..]) {
                Ok(amt) => {
                    sent += amt;
                }
                Err(ref e) if e.kind() == ErrorKind::WouldBlock => {
                    //Cant send at the moment, sleep some
                    std::thread::sleep(std::time::Duration::from_millis(10));
                }
                Err(e) => {
                    panic!("Failed to send data via TCP socket: {:#?}", e);
                }
            }
        }
    }

    pub fn next_pdu(&mut self) -> Option<Message> {
        self.incoming_queue.pop_front()
    }

    pub fn try_read(&mut self, socket: &mut TcpStream) -> bool {
        let mut closed = false;
        loop {
            match socket.read(&mut self.buffer[self.buffer_fill..]) {
                Ok(amt) => {
                    if amt == 0 {
                        println!("    Remote closed the connection {:?}", socket.peer_addr());
                        closed = true;
                        break;
                    }
                    self.buffer_fill += amt;
                }
                Err(ref e) if e.kind() == ErrorKind::WouldBlock => {
                    //No data to read at the moment
                    break;
                }
                Err(e) => {
                    panic!("Failed to read from UDP socket: {:#?}", e);
                }
            }
        }

        while let Some((pdu, used)) = parse_pdu(&self.buffer[..self.buffer_fill]) {
            self.incoming_queue
                .push_back((pdu, socket.peer_addr().unwrap()));
            self.buffer.copy_within(used..self.buffer_fill, 0);
            self.buffer_fill -= used;
        }

        closed
    }
}

pub struct UdpWrapper {
    incoming_queue: VecDeque<Message>,
    buffer: [u8; BUFFER_SIZE],
    buffer_fill: usize,
}

impl UdpWrapper {
    pub fn new() -> Self {
        UdpWrapper {
            incoming_queue: VecDeque::new(),
            buffer: [0; BUFFER_SIZE],
            buffer_fill: 0,
        }
    }

    pub fn send(&self, socket: &mut UdpSocket, pdu: PDU, rec: SocketAddr) {
        let bytes = pdu.to_bytes();
        loop {
            match socket.send_to(&bytes, rec) {
                Ok(amt) => {
                    if amt != bytes.len() {
                        panic!(
                            "Failed to send the entire buffer via UDP {}/{}",
                            amt,
                            bytes.len()
                        );
                    }

                    break;
                }
                Err(ref e) if e.kind() == ErrorKind::WouldBlock => {
                    //Cant send at the moment, sleep some
                    std::thread::sleep(std::time::Duration::from_millis(10));
                }
                Err(e) => {
                    panic!("Failed to send data via UDP socket: {:#?}", e);
                }
            }
        }
    }

    pub fn next_pdu(&mut self) -> Option<Message> {
        self.incoming_queue.pop_front()
    }

    pub fn try_read(&mut self, socket: &mut UdpSocket) {
        loop {
            match socket.recv_from(&mut self.buffer[self.buffer_fill..]) {
                Ok((amt, src)) => {
                    self.buffer_fill += amt;

                    while let Some((pdu, used)) = parse_pdu(&self.buffer[..self.buffer_fill]) {
                        self.incoming_queue.push_back((pdu, src));
                        self.buffer.copy_within(used..self.buffer_fill, 0);
                        self.buffer_fill -= used;
                    }
                }
                Err(ref e) if e.kind() == ErrorKind::WouldBlock => {
                    //No data to read at the moment
                    break;
                }
                Err(e) => {
                    panic!("Failed to read from UDP socket: {:#?}", e);
                }
            }
        }
    }
}

fn parse_pdu(buffer: &[u8]) -> Option<(PDU, usize)> {
    if buffer.len() == 0 {
        return None;
    }

    match buffer[0] {
        0..=8 => parse_net_pdu(buffer),
        100..=103 => parse_val_pdu(buffer),
        200..=201 => parse_stun_pdu(buffer),
        _ => {
            panic!("Got invalid PDU: \n {:#?}", buffer);
        }
    }
}

fn parse_net_pdu(buffer: &[u8]) -> Option<(PDU, usize)> {
    match buffer[0] {
        pdu::NET_ALIVE_ID => {
            let (p, s) = NetAlivePdu::try_parse(buffer)?;
            Some((p.into(), s))
        }
        pdu::NET_GET_NODE_ID => {
            let (p, s) = NetGetNodePdu::try_parse(buffer)?;
            Some((p.into(), s))
        }
        pdu::NET_GET_NODE_RESPONSE_ID => {
            let (p, s) = NetGetNodeResponsePdu::try_parse(buffer)?;
            Some((p.into(), s))
        }
        pdu::NET_JOIN_ID => {
            let (p, s) = NetJoinPdu::try_parse(buffer)?;
            Some((p.into(), s))
        }
        pdu::NET_JOIN_RESPONSE_ID => {
            let (p, s) = NetJoinResponsePdu::try_parse(buffer)?;
            Some((p.into(), s))
        }
        pdu::NET_CLOSE_CONNECTION_ID => {
            let (p, s) = NetCloseConnectionPdu::try_parse(buffer)?;
            Some((p.into(), s))
        }
        pdu::NET_NEW_RANGE_ID => {
            let (p, s) = NetNewRangePdu::try_parse(buffer)?;
            Some((p.into(), s))
        }
        pdu::NET_NEW_RANGE_RESPONSE_ID => {
            let (p, s) = NetNewRangeResponsePdu::try_parse(buffer)?;
            Some((p.into(), s))
        }
        pdu::NET_LEAVING_ID => {
            let (p, s) = NetLeavingPdu::try_parse(buffer)?;
            Some((p.into(), s))
        }
        x => {
            panic!("Invalid lookup id, this should not happen ({}) ", x);
        }
    }
}

fn parse_val_pdu(buffer: &[u8]) -> Option<(PDU, usize)> {
    match buffer[0] {
        pdu::VAL_INSERT_ID => {
            let (p, s) = ValInsertPdu::try_parse(buffer)?;
            Some((PDU::ValInsert(p), s))
        }
        pdu::VAL_REMOVE_ID => {
            let (p, s) = ValRemovePdu::try_parse(buffer)?;
            Some((PDU::ValRemove(p), s))
        }
        pdu::VAL_LOOKUP_ID => {
            let (p, s) = ValLookupPdu::try_parse(buffer)?;
            Some((PDU::ValLookup(p), s))
        }
        pdu::VAL_LOOKUP_RESPONSE_ID => {
            let (p, s) = ValLookupResponsePdu::try_parse(buffer)?;
            Some((PDU::ValLookupResponse(p), s))
        }
        x => {
            panic!("Invalid lookup id, this should not happen ({}) ", x);
        }
    }
}

fn parse_stun_pdu(buffer: &[u8]) -> Option<(PDU, usize)> {
    match buffer[0] {
        pdu::STUN_LOOKUP_ID => {
            let (p, s) = StunLookupPdu::try_parse(buffer)?;
            Some((PDU::StunLookup(p), s))
        }
        pdu::STUN_RESPONSE_ID => {
            let (p, s) = StunResponsePdu::try_parse(buffer)?;
            Some((PDU::StunResponse(p), s))
        }
        x => {
            panic!("Invalid stun id, this should not happen ({}) ", x);
        }
    }
}
