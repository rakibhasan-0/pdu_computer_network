use std::convert::TryInto;
use std::net::{Ipv4Addr, SocketAddr};

pub const NET_ALIVE_ID: u8 = 0;
pub const NET_GET_NODE_ID: u8 = 1;
pub const NET_GET_NODE_RESPONSE_ID: u8 = 2;
pub const NET_JOIN_ID: u8 = 3;
pub const NET_JOIN_RESPONSE_ID: u8 = 4;
pub const NET_CLOSE_CONNECTION_ID: u8 = 5;

pub const NET_NEW_RANGE_ID: u8 = 6;
pub const NET_LEAVING_ID: u8 = 7;
pub const NET_NEW_RANGE_RESPONSE_ID: u8 = 8;

pub const VAL_INSERT_ID: u8 = 100;
pub const VAL_REMOVE_ID: u8 = 101;
pub const VAL_LOOKUP_ID: u8 = 102;
pub const VAL_LOOKUP_RESPONSE_ID: u8 = 103;

pub const STUN_LOOKUP_ID: u8 = 200;
pub const STUN_RESPONSE_ID: u8 = 201;

const NET_ALIVE_SIZE: usize = 1;
const NET_GET_NODE_SIZE: usize = 1;
const NET_GET_NODE_RESPONSE_SIZE: usize = 1 + 4 + 2;
const NET_JOIN_SIZE: usize = 1 + 4 + 2 + 1 + 4 + 2;
const NET_JOIN_RESPONSE_SIZE: usize = 1 + 4 + 2 + 1 + 1;
const NET_CLOSE_CONNECTION_SIZE: usize = 1;

const NET_NEW_RANGE_SIZE: usize = 1 + 1 + 1;
const NET_NEW_RANGE_RESPONSE_SIZE: usize = 1;
const NET_LEAVING_SIZE: usize = 1 + 4 + 2;

const VAL_REMOVE_SIZE: usize = 1 + SSN_LENGTH;
const VAL_LOOKUP_SIZE: usize = 1 + SSN_LENGTH + 4 + 2;

const STUN_LOOKUP_SIZE: usize = 1;
const STUN_RESPONSE_SIZE: usize = 1 + 4;

const SSN_LENGTH: usize = 12;

pub enum PDU {
    NetAlive(NetAlivePdu),
    NetGetNode(NetGetNodePdu),
    NetGetNodeResponse(NetGetNodeResponsePdu),
    NetJoin(NetJoinPdu),
    NetJoinResponse(NetJoinResponsePdu),
    NetCloseConnection(NetCloseConnectionPdu),
    NetNewRange(NetNewRangePdu),
    NetNewRangeResponse(NetNewRangeResponsePdu),
    NetLeaving(NetLeavingPdu),
    ValInsert(ValInsertPdu),
    ValRemove(ValRemovePdu),
    ValLookup(ValLookupPdu),
    ValLookupResponse(ValLookupResponsePdu),
    StunLookup(StunLookupPdu),
    StunResponse(StunResponsePdu),
}

impl std::fmt::Debug for PDU {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        f.debug_struct(match self {
            Self::NetAlive(_) => "NetAlive",
            Self::NetGetNode(_) => "NetGetNode",
            Self::NetGetNodeResponse(_) => "NetGetNodeResponse",
            Self::NetJoin(_) => "NetJoin",
            Self::NetJoinResponse(_) => "NetJoinResponse",
            Self::NetCloseConnection(_) => "NetCloseConnection",
            Self::NetNewRange(_) => "NetNewRange",
            Self::NetNewRangeResponse(_) => "NetNewRangeResponse",
            Self::NetLeaving(_) => "NetLeaving",
            Self::ValInsert(_) => "ValInsert",
            Self::ValRemove(_) => "ValRemove",
            Self::ValLookup(_) => "ValLookup",
            Self::ValLookupResponse(_) => "ValLookupResponse",
            Self::StunLookup(_) => "StunLookup",
            Self::StunResponse(_) => "StunResponse",
        })
        .finish()
    }
}

impl PDU {
    pub fn to_bytes(self) -> Vec<u8> {
        match self {
            Self::NetAlive(p) => Vec::from(p),
            Self::NetGetNode(p) => Vec::from(p),
            Self::NetGetNodeResponse(p) => Vec::from(p),
            Self::NetJoin(p) => Vec::from(p),
            Self::NetJoinResponse(p) => Vec::from(p),
            Self::NetCloseConnection(p) => Vec::from(p),
            Self::NetNewRange(p) => Vec::from(p),
            Self::NetNewRangeResponse(p) => Vec::from(p),
            Self::NetLeaving(p) => Vec::from(p),
            Self::ValInsert(p) => Vec::from(p),
            Self::ValRemove(p) => Vec::from(p),
            Self::ValLookup(p) => Vec::from(p),
            Self::ValLookupResponse(p) => Vec::from(p),
            Self::StunLookup(p) => Vec::from(p),
            Self::StunResponse(p) => Vec::from(p),
        }
    }
}

fn read_be_u8(input: &mut &[u8]) -> u8 {
    let (int_bytes, rest) = input.split_at(std::mem::size_of::<u8>());
    *input = rest;
    u8::from_be_bytes(int_bytes.try_into().unwrap())
}

fn read_be_u32(input: &mut &[u8]) -> u32 {
    let (int_bytes, rest) = input.split_at(std::mem::size_of::<u32>());
    *input = rest;
    u32::from_be_bytes(int_bytes.try_into().unwrap())
}

fn read_be_u16(input: &mut &[u8]) -> u16 {
    let (int_bytes, rest) = input.split_at(std::mem::size_of::<u16>());
    *input = rest;
    u16::from_be_bytes(int_bytes.try_into().unwrap())
}

pub trait ParsePdu: Sized {
    fn try_parse(buffer: &[u8]) -> Option<(Self, usize)>;
}

pub struct NetGetNodeResponsePdu {
    pub pdu_type: u8,
    pub address: u32,
    pub port: u16,
}

impl NetGetNodeResponsePdu {
    pub fn new(address: u32, port: u16) -> Self {
        NetGetNodeResponsePdu {
            pdu_type: NET_GET_NODE_RESPONSE_ID,
            address,
            port,
        }
    }

    pub fn get_addr(&self) -> SocketAddr {
        let ip: Ipv4Addr = self.address.into();
        (ip, self.port).into()
    }
}

impl From<NetGetNodeResponsePdu> for Vec<u8> {
    fn from(pdu: NetGetNodeResponsePdu) -> Self {
        let mut v = Vec::new();
        v.push(pdu.pdu_type);
        v.extend_from_slice(&pdu.address.to_be_bytes());
        v.extend_from_slice(&pdu.port.to_be_bytes());
        v
    }
}

impl ParsePdu for NetGetNodeResponsePdu {
    fn try_parse(buffer: &[u8]) -> Option<(Self, usize)> {
        let size = NET_GET_NODE_RESPONSE_SIZE;
        if buffer.len() < size {
            return None;
        }
        let mut buffer = &buffer[..];
        let pdu = NetGetNodeResponsePdu {
            pdu_type: read_be_u8(&mut buffer),
            address: read_be_u32(&mut buffer),
            port: read_be_u16(&mut buffer),
        };
        Some((pdu, size))
    }
}

impl From<NetGetNodeResponsePdu> for PDU {
    fn from(pdu: NetGetNodeResponsePdu) -> Self {
        Self::NetGetNodeResponse(pdu)
    }
}

pub struct NetAlivePdu {
    pub pdu_type: u8,
}

impl NetAlivePdu {
    pub fn new() -> Self {
        NetAlivePdu {
            pdu_type: NET_ALIVE_ID,
        }
    }
}

impl From<NetAlivePdu> for Vec<u8> {
    fn from(pdu: NetAlivePdu) -> Self {
        let mut v = Vec::new();
        v.push(pdu.pdu_type);
        v
    }
}

impl ParsePdu for NetAlivePdu {
    fn try_parse(buffer: &[u8]) -> Option<(Self, usize)> {
        let size = NET_ALIVE_SIZE;
        if buffer.len() < size {
            return None;
        }

        let mut buffer = &buffer[..];

        let pdu = NetAlivePdu {
            pdu_type: read_be_u8(&mut buffer),
        };
        Some((pdu, size))
    }
}

impl From<NetAlivePdu> for PDU {
    fn from(pdu: NetAlivePdu) -> Self {
        Self::NetAlive(pdu)
    }
}

pub struct NetGetNodePdu {
    pub pdu_type: u8,
}

impl NetGetNodePdu {
    pub fn new() -> Self {
        NetGetNodePdu {
            pdu_type: NET_GET_NODE_ID,
        }
    }
}

impl From<NetGetNodePdu> for Vec<u8> {
    fn from(pdu: NetGetNodePdu) -> Self {
        let mut v = Vec::new();
        v.push(pdu.pdu_type);
        v
    }
}

impl ParsePdu for NetGetNodePdu {
    fn try_parse(buffer: &[u8]) -> Option<(Self, usize)> {
        let size = NET_GET_NODE_SIZE;
        if buffer.len() < size {
            return None;
        }

        let mut buffer = &buffer[..];
        let pdu = NetGetNodePdu {
            pdu_type: read_be_u8(&mut buffer),
        };
        Some((pdu, size))
    }
}

impl From<NetGetNodePdu> for PDU {
    fn from(pdu: NetGetNodePdu) -> Self {
        Self::NetGetNode(pdu)
    }
}

pub struct NetCloseConnectionPdu {
    pub pdu_type: u8,
}

impl NetCloseConnectionPdu {
    pub fn new() -> Self {
        NetCloseConnectionPdu {
            pdu_type: NET_CLOSE_CONNECTION_ID,
        }
    }
}

impl From<NetCloseConnectionPdu> for Vec<u8> {
    fn from(pdu: NetCloseConnectionPdu) -> Self {
        let mut v = Vec::new();
        v.push(pdu.pdu_type);
        v
    }
}

impl ParsePdu for NetCloseConnectionPdu {
    fn try_parse(buffer: &[u8]) -> Option<(Self, usize)> {
        let size = NET_CLOSE_CONNECTION_SIZE;
        if buffer.len() < size {
            return None;
        }

        let mut buffer = &buffer[..];
        let pdu = NetCloseConnectionPdu {
            pdu_type: read_be_u8(&mut buffer),
        };
        Some((pdu, size))
    }
}

impl From<NetCloseConnectionPdu> for PDU {
    fn from(pdu: NetCloseConnectionPdu) -> Self {
        Self::NetCloseConnection(pdu)
    }
}

pub struct NetJoinPdu {
    pub pdu_type: u8,
    pub src_address: u32,
    pub src_port: u16,
    pub max_span: u8,
    pub max_address: u32,
    pub max_port: u16,
}

impl NetJoinPdu {
    pub fn new(
        src_address: u32,
        src_port: u16,
        max_span: u8,
        max_address: u32,
        max_port: u16,
    ) -> Self {
        NetJoinPdu {
            pdu_type: NET_JOIN_ID,
            src_address,
            src_port,
            max_span,
            max_address,
            max_port,
        }
    }

    pub fn get_max_socket_addr(&self) -> SocketAddr {
        let ip: Ipv4Addr = self.max_address.into();
        (ip, self.max_port).into()
    }

    pub fn get_src_socket_addr(&self) -> SocketAddr {
        let ip: Ipv4Addr = self.src_address.into();
        (ip, self.src_port).into()
    }
}

impl From<NetJoinPdu> for Vec<u8> {
    fn from(pdu: NetJoinPdu) -> Self {
        let mut v = Vec::new();
        v.push(pdu.pdu_type);
        v.extend_from_slice(&pdu.src_address.to_be_bytes());
        v.extend_from_slice(&pdu.src_port.to_be_bytes());
        v.push(pdu.max_span);
        v.extend_from_slice(&pdu.max_address.to_be_bytes());
        v.extend_from_slice(&pdu.max_port.to_be_bytes());
        v
    }
}

impl ParsePdu for NetJoinPdu {
    fn try_parse(buffer: &[u8]) -> Option<(Self, usize)> {
        let size = NET_JOIN_SIZE;
        if buffer.len() < size {
            return None;
        }

        let mut buffer = &buffer[..];

        let pdu = NetJoinPdu {
            pdu_type: read_be_u8(&mut buffer),
            src_address: read_be_u32(&mut buffer),
            src_port: read_be_u16(&mut buffer),
            max_span: read_be_u8(&mut buffer),
            max_address: read_be_u32(&mut buffer),
            max_port: read_be_u16(&mut buffer),
        };
        Some((pdu, size))
    }
}

impl From<NetJoinPdu> for PDU {
    fn from(pdu: NetJoinPdu) -> Self {
        Self::NetJoin(pdu)
    }
}

pub struct NetJoinResponsePdu {
    pub pdu_type: u8,
    pub next_address: u32,
    pub next_port: u16,
    pub range_start: u8,
    pub range_end: u8,
}

impl NetJoinResponsePdu {
    pub fn new(next_address: u32, next_port: u16, range_start: u8, range_end: u8) -> Self {
        NetJoinResponsePdu {
            pdu_type: NET_JOIN_RESPONSE_ID,
            next_address,
            next_port,
            range_start,
            range_end,
        }
    }

    pub fn get_next_addr(&self) -> SocketAddr {
        let ip: Ipv4Addr = self.next_address.into();
        (ip, self.next_port).into()
    }
}

impl From<NetJoinResponsePdu> for Vec<u8> {
    fn from(pdu: NetJoinResponsePdu) -> Self {
        let mut v = Vec::new();
        v.push(pdu.pdu_type);
        v.extend_from_slice(&pdu.next_address.to_be_bytes());
        v.extend_from_slice(&pdu.next_port.to_be_bytes());
        v.push(pdu.range_start);
        v.push(pdu.range_end);
        v
    }
}

impl ParsePdu for NetJoinResponsePdu {
    fn try_parse(buffer: &[u8]) -> Option<(Self, usize)> {
        let size = NET_JOIN_RESPONSE_SIZE;
        if buffer.len() < size {
            return None;
        }

        let mut buffer = &buffer[..];

        let pdu = NetJoinResponsePdu {
            pdu_type: read_be_u8(&mut buffer),
            next_address: read_be_u32(&mut buffer),
            next_port: read_be_u16(&mut buffer),
            range_start: read_be_u8(&mut buffer),
            range_end: read_be_u8(&mut buffer),
        };
        Some((pdu, size))
    }
}

impl From<NetJoinResponsePdu> for PDU {
    fn from(pdu: NetJoinResponsePdu) -> Self {
        Self::NetJoinResponse(pdu)
    }
}

pub struct StunLookupPdu {
    pub pdu_type: u8,
}

impl StunLookupPdu {
    pub fn new() -> Self {
        StunLookupPdu {
            pdu_type: STUN_LOOKUP_ID,
        }
    }
}

impl From<StunLookupPdu> for Vec<u8> {
    fn from(pdu: StunLookupPdu) -> Self {
        let mut v = Vec::new();
        v.push(pdu.pdu_type);
        v
    }
}

impl ParsePdu for StunLookupPdu {
    fn try_parse(buffer: &[u8]) -> Option<(Self, usize)> {
        let size = STUN_LOOKUP_SIZE;
        if buffer.len() < size {
            return None;
        }

        let mut buffer = &buffer[..];

        let pdu = StunLookupPdu {
            pdu_type: read_be_u8(&mut buffer),
        };
        Some((pdu, size))
    }
}

impl From<StunLookupPdu> for PDU {
    fn from(pdu: StunLookupPdu) -> Self {
        Self::StunLookup(pdu)
    }
}

pub struct NetNewRangePdu {
    pub pdu_type: u8,
    pub range_start: u8,
    pub range_end: u8,
}

impl NetNewRangePdu {
    pub fn new(range_start: u8, range_end: u8) -> Self {
        NetNewRangePdu {
            pdu_type: NET_NEW_RANGE_ID,
            range_start,
            range_end,
        }
    }
}

impl From<NetNewRangePdu> for Vec<u8> {
    fn from(pdu: NetNewRangePdu) -> Self {
        let mut v = Vec::new();
        v.push(pdu.pdu_type);
        v.push(pdu.range_start);
        v.push(pdu.range_end);
        v
    }
}

impl ParsePdu for NetNewRangePdu {
    fn try_parse(buffer: &[u8]) -> Option<(Self, usize)> {
        let size = NET_NEW_RANGE_SIZE;
        if buffer.len() < size {
            return None;
        }

        let mut buffer = &buffer[..];

        let pdu = NetNewRangePdu {
            pdu_type: read_be_u8(&mut buffer),
            range_start: read_be_u8(&mut buffer),
            range_end: read_be_u8(&mut buffer),
        };
        Some((pdu, size))
    }
}

impl From<NetNewRangePdu> for PDU {
    fn from(pdu: NetNewRangePdu) -> Self {
        Self::NetNewRange(pdu)
    }
}

pub struct NetNewRangeResponsePdu {
    pub pdu_type: u8,
}

impl NetNewRangeResponsePdu {
    pub fn new() -> Self {
        NetNewRangeResponsePdu {
            pdu_type: NET_NEW_RANGE_RESPONSE_ID,
        }
    }
}

impl From<NetNewRangeResponsePdu> for Vec<u8> {
    fn from(pdu: NetNewRangeResponsePdu) -> Self {
        let mut v = Vec::new();
        v.push(pdu.pdu_type);
        v
    }
}

impl ParsePdu for NetNewRangeResponsePdu {
    fn try_parse(buffer: &[u8]) -> Option<(Self, usize)> {
        let size = NET_NEW_RANGE_RESPONSE_SIZE;
        if buffer.len() < size {
            return None;
        }

        let mut buffer = &buffer[..];

        let pdu = NetNewRangeResponsePdu {
            pdu_type: read_be_u8(&mut buffer),
        };
        Some((pdu, size))
    }
}

impl From<NetNewRangeResponsePdu> for PDU {
    fn from(pdu: NetNewRangeResponsePdu) -> Self {
        Self::NetNewRangeResponse(pdu)
    }
}

pub struct NetLeavingPdu {
    pub pdu_type: u8,
    pub new_address: u32,
    pub new_port: u16,
}

impl NetLeavingPdu {
    pub fn new(new_address: u32, new_port: u16) -> Self {
        NetLeavingPdu {
            pdu_type: NET_LEAVING_ID,
            new_address,
            new_port,
        }
    }

    pub fn get_new_addr(&self) -> SocketAddr {
        let ip: Ipv4Addr = self.new_address.into();
        (ip, self.new_port).into()
    }
}

impl From<NetLeavingPdu> for Vec<u8> {
    fn from(pdu: NetLeavingPdu) -> Self {
        let mut v = Vec::new();
        v.push(pdu.pdu_type);
        v.extend_from_slice(&pdu.new_address.to_be_bytes());
        v.extend_from_slice(&pdu.new_port.to_be_bytes());
        v
    }
}

impl ParsePdu for NetLeavingPdu {
    fn try_parse(buffer: &[u8]) -> Option<(Self, usize)> {
        let size = NET_LEAVING_SIZE;
        if buffer.len() < size {
            return None;
        }

        let mut buffer = &buffer[..];

        let pdu = NetLeavingPdu {
            pdu_type: read_be_u8(&mut buffer),
            new_address: read_be_u32(&mut buffer),
            new_port: read_be_u16(&mut buffer),
        };
        Some((pdu, size))
    }
}

impl From<NetLeavingPdu> for PDU {
    fn from(pdu: NetLeavingPdu) -> Self {
        Self::NetLeaving(pdu)
    }
}

pub struct StunResponsePdu {
    pub pdu_type: u8,
    pub address: u32,
}

impl StunResponsePdu {
    pub fn new(address: u32) -> Self {
        StunResponsePdu {
            pdu_type: STUN_RESPONSE_ID,
            address,
        }
    }
}

impl From<StunResponsePdu> for Vec<u8> {
    fn from(pdu: StunResponsePdu) -> Self {
        let mut v = Vec::new();
        v.push(pdu.pdu_type);
        v.extend_from_slice(&pdu.address.to_be_bytes());
        v
    }
}

impl ParsePdu for StunResponsePdu {
    fn try_parse(buffer: &[u8]) -> Option<(Self, usize)> {
        let size = STUN_RESPONSE_SIZE;
        if buffer.len() < size {
            return None;
        }

        let mut buffer = &buffer[..];

        let pdu = StunResponsePdu {
            pdu_type: read_be_u8(&mut buffer),
            address: read_be_u32(&mut buffer),
        };
        Some((pdu, size))
    }
}

impl From<StunResponsePdu> for PDU {
    fn from(pdu: StunResponsePdu) -> Self {
        Self::StunResponse(pdu)
    }
}

pub struct ValInsertPdu {
    pub pdu_type: u8,
    pub ssn: String,
    pub name_length: u8,
    pub name: String,
    pub email_length: u8,
    pub email: String,
}

impl ValInsertPdu {
    pub fn new(ssn: String, name: String, email: String) -> Self {
        ValInsertPdu {
            pdu_type: VAL_INSERT_ID,
            name_length: name.len() as u8,
            email_length: email.len() as u8,
            ssn,
            name,
            email,
        }
    }
}

impl From<ValInsertPdu> for Vec<u8> {
    fn from(pdu: ValInsertPdu) -> Self {
        let mut v = Vec::new();
        v.push(pdu.pdu_type);
        v.extend(pdu.ssn.chars().take(SSN_LENGTH).map(|x| x as u8));
        v.push(pdu.name_length);
        v.extend(pdu.name.chars().map(|x| x as u8));
        v.push(pdu.email_length);
        v.extend(pdu.email.chars().map(|x| x as u8));
        v
    }
}

impl ParsePdu for ValInsertPdu {
    fn try_parse(buffer: &[u8]) -> Option<(Self, usize)> {
        if buffer.len() < 1 + SSN_LENGTH + 1 {
            return None;
        }

        let mut buffer = &buffer[..];
        let pdu_type = read_be_u8(&mut buffer);

        let (ssn, mut buffer) = buffer.split_at(SSN_LENGTH);
        let ssn = ssn.iter().map(|&x| x as char).collect();
        let name_length = read_be_u8(&mut buffer);

        if buffer.len() < name_length as usize {
            return None;
        }

        let (name, mut buffer) = buffer.split_at(name_length as usize);
        let name: String = name.iter().map(|&x| x as char).collect();

        let email_length = read_be_u8(&mut buffer);

        if buffer.len() < email_length as usize {
            return None;
        }

        let email = buffer
            .iter()
            .take(email_length as usize)
            .map(|&x| x as char)
            .collect();

        let pdu = ValInsertPdu {
            pdu_type,
            ssn,
            name_length,
            name,
            email_length,
            email,
        };
        //       Type   SSN    Name length  Name           Email length   Email
        let size = 1 + SSN_LENGTH + 1 + name_length as usize + 1 + email_length as usize;
        Some((pdu, size))
    }
}

impl From<ValInsertPdu> for PDU {
    fn from(pdu: ValInsertPdu) -> Self {
        Self::ValInsert(pdu)
    }
}

pub struct ValLookupPdu {
    pub pdu_type: u8,
    pub ssn: String,
    pub sender_address: u32,
    pub sender_port: u16,
}

impl ValLookupPdu {
    pub fn new(ssn: String, sender_address: u32, sender_port: u16) -> Self {
        ValLookupPdu {
            pdu_type: VAL_LOOKUP_ID,
            sender_address,
            sender_port,
            ssn,
        }
    }
}

impl From<ValLookupPdu> for Vec<u8> {
    fn from(pdu: ValLookupPdu) -> Self {
        let mut v = Vec::new();
        v.push(pdu.pdu_type);
        v.extend(pdu.ssn.chars().take(SSN_LENGTH).map(|x| x as u8));
        v.extend_from_slice(&pdu.sender_address.to_be_bytes());
        v.extend_from_slice(&pdu.sender_port.to_be_bytes());
        v
    }
}

impl ParsePdu for ValLookupPdu {
    fn try_parse(buffer: &[u8]) -> Option<(Self, usize)> {
        let size = VAL_LOOKUP_SIZE;
        if buffer.len() < size {
            return None;
        }

        let mut buffer = &buffer[..];
        let pdu_type = read_be_u8(&mut buffer);
        let (ssn, mut buffer) = buffer.split_at(SSN_LENGTH);
        let pdu = ValLookupPdu {
            pdu_type,
            ssn: ssn.iter().map(|&x| x as char).collect(),
            sender_address: read_be_u32(&mut buffer),
            sender_port: read_be_u16(&mut buffer),
        };
        Some((pdu, size))
    }
}

impl From<ValLookupPdu> for PDU {
    fn from(pdu: ValLookupPdu) -> Self {
        Self::ValLookup(pdu)
    }
}

pub struct ValLookupResponsePdu {
    pub pdu_type: u8,
    pub ssn: String,
    pub name_length: u8,
    pub name: String,
    pub email_length: u8,
    pub email: String,
}

impl ValLookupResponsePdu {
    pub fn new(ssn: String, name: String, email: String) -> Self {
        ValLookupResponsePdu {
            pdu_type: VAL_LOOKUP_RESPONSE_ID,
            name_length: name.len() as u8,
            email_length: email.len() as u8,
            ssn,
            name,
            email,
        }
    }
}

impl From<ValLookupResponsePdu> for Vec<u8> {
    fn from(pdu: ValLookupResponsePdu) -> Self {
        let mut v = Vec::new();
        v.push(pdu.pdu_type);
        v.extend(pdu.ssn.chars().take(SSN_LENGTH).map(|x| x as u8));
        v.push(pdu.name_length);
        v.extend(pdu.name.chars().map(|x| x as u8));
        v.push(pdu.email_length);
        v.extend(pdu.email.chars().map(|x| x as u8));
        v
    }
}

impl ParsePdu for ValLookupResponsePdu {
    fn try_parse(buffer: &[u8]) -> Option<(Self, usize)> {
        let (pdu, s) = ValInsertPdu::try_parse(buffer)?;
        let pdu = ValLookupResponsePdu {
            pdu_type: VAL_LOOKUP_ID,
            ssn: pdu.ssn,
            name_length: pdu.name_length,
            name: pdu.name,
            email_length: pdu.email_length,
            email: pdu.email,
        };
        Some((ValLookupResponsePdu { ..pdu }, s))
    }
}

impl From<ValLookupResponsePdu> for PDU {
    fn from(pdu: ValLookupResponsePdu) -> Self {
        Self::ValLookupResponse(pdu)
    }
}

pub struct ValRemovePdu {
    pub pdu_type: u8,
    pub ssn: String,
}

impl ValRemovePdu {
    pub fn new(ssn: String) -> Self {
        ValRemovePdu {
            pdu_type: VAL_REMOVE_ID,
            ssn,
        }
    }
}

impl From<ValRemovePdu> for Vec<u8> {
    fn from(pdu: ValRemovePdu) -> Self {
        let mut v = Vec::new();
        v.push(pdu.pdu_type);
        v.extend(pdu.ssn.chars().take(SSN_LENGTH).map(|x| x as u8));
        v
    }
}

impl ParsePdu for ValRemovePdu {
    fn try_parse(buffer: &[u8]) -> Option<(Self, usize)> {
        let size = VAL_REMOVE_SIZE;
        if buffer.len() < size {
            return None;
        }

        let mut buffer = &buffer[..];

        let pdu = ValRemovePdu {
            pdu_type: read_be_u8(&mut buffer),
            ssn: buffer[..SSN_LENGTH].iter().map(|&x| x as char).collect(),
        };
        Some((pdu, size))
    }
}

impl From<ValRemovePdu> for PDU {
    fn from(pdu: ValRemovePdu) -> Self {
        Self::ValRemove(pdu)
    }
}

#[cfg(test)]
mod serialization_test {
    use crate::pdu::PDU::*;
    use crate::pdu::*;
    #[test]
    fn test_net_alive() {
        let a = NetAlivePdu::new();
        let b: Vec<u8> = a.into();
        assert_eq!(b.len(), NET_ALIVE_SIZE);
        let (a, b) = NetAlivePdu::try_parse(&b).unwrap();
        assert_eq!(b, NET_ALIVE_SIZE);
    }

    #[test]
    fn test_net_get_node() {
        let a = NetGetNodePdu::new();
        let b: Vec<u8> = a.into();
        assert_eq!(b.len(), NET_GET_NODE_SIZE);
        let (a, b) = NetGetNodePdu::try_parse(&b).unwrap();
        assert_eq!(b, NET_GET_NODE_SIZE);
    }

    #[test]
    fn test_net_get_node_response() {
        let a = NetGetNodeResponsePdu::new(123456, 1234);
        let b: Vec<u8> = a.into();
        assert_eq!(b.len(), NET_GET_NODE_RESPONSE_SIZE);
        let (a, b) = NetGetNodeResponsePdu::try_parse(&b).unwrap();
        assert_eq!(b, NET_GET_NODE_RESPONSE_SIZE);
        assert_eq!(123456, a.address);
        assert_eq!(1234, a.port);
    }

    #[test]
    fn test_net_join() {
        let a = NetJoinPdu::new(1010, 1011, 10, 1122, 1123);
        let b: Vec<u8> = a.into();
        assert_eq!(b.len(), NET_JOIN_SIZE);
        let (a, b) = NetJoinPdu::try_parse(&b).unwrap();
        assert_eq!(b, NET_JOIN_SIZE);
        assert_eq!(1010, a.src_address);
        assert_eq!(1011, a.src_port);
        assert_eq!(10, a.max_span);
        assert_eq!(1122, a.max_address);
        assert_eq!(1123, a.max_port);
    }

    #[test]
    fn test_net_join_response() {
        let a = NetJoinResponsePdu::new(123456, 1234, 10, 20);
        let b: Vec<u8> = a.into();
        assert_eq!(b.len(), NET_JOIN_RESPONSE_SIZE);
        let (a, b) = NetJoinResponsePdu::try_parse(&b).unwrap();
        assert_eq!(b, NET_JOIN_RESPONSE_SIZE);
        assert_eq!(123456, a.next_address);
        assert_eq!(1234, a.next_port);
        assert_eq!(10, a.range_start);
        assert_eq!(20, a.range_end);
    }

    #[test]
    fn test_net_close_connection() {
        let a = NetCloseConnectionPdu::new();
        let b: Vec<u8> = a.into();
        assert_eq!(b.len(), NET_CLOSE_CONNECTION_SIZE);
        let (a, b) = NetCloseConnectionPdu::try_parse(&b).unwrap();
        assert_eq!(b, NET_CLOSE_CONNECTION_SIZE);
    }

    #[test]
    fn test_net_new_range() {
        let a = NetNewRangePdu::new(1, 255);
        let b: Vec<u8> = a.into();
        assert_eq!(b.len(), NET_NEW_RANGE_SIZE);
        let (a, b) = NetNewRangePdu::try_parse(&b).unwrap();
        assert_eq!(b, NET_NEW_RANGE_SIZE);
        assert_eq!(a.range_start, 1);
        assert_eq!(a.range_end, 255);
    }

    #[test]
    fn test_net_new_range_response() {
        let a = NetNewRangeResponsePdu::new();
        let b: Vec<u8> = a.into();
        assert_eq!(b.len(), NET_NEW_RANGE_RESPONSE_SIZE);
        let (a, b) = NetNewRangeResponsePdu::try_parse(&b).unwrap();
        assert_eq!(b, NET_NEW_RANGE_RESPONSE_SIZE);
        assert_eq!(a.pdu_type, NET_NEW_RANGE_RESPONSE_ID);
    }

    #[test]
    fn test_net_leaving() {
        let a = NetLeavingPdu::new(12345, 255);
        let b: Vec<u8> = a.into();
        assert_eq!(b.len(), NET_LEAVING_SIZE);
        let (a, b) = NetLeavingPdu::try_parse(&b).unwrap();
        assert_eq!(b, NET_LEAVING_SIZE);
        assert_eq!(a.new_address, 12345);
        assert_eq!(a.new_port, 255);
    }

    #[test]
    fn test_val_insert() {
        let ssn = "111111111111".to_owned();
        let name = "Test".to_owned();
        let email = "Emai".to_owned();
        let a = ValInsertPdu::new(ssn.clone(), name.clone(), email.clone());
        let b: Vec<u8> = a.into();
        let len = 1 + SSN_LENGTH + 1 + name.len() + 1 + email.len();
        assert_eq!(b.len(), len);
        let (a, b) = ValInsertPdu::try_parse(&b).unwrap();
        assert_eq!(b, len);
        assert_eq!(a.ssn, ssn);
        assert_eq!(a.name_length, name.len() as u8);
        assert_eq!(a.name, name);
        assert_eq!(a.email_length, email.len() as u8);
        assert_eq!(a.email, email);
    }

    #[test]
    fn test_val_remove() {
        let ssn = "111111111111".to_owned();
        let a = ValRemovePdu::new(ssn.clone());
        let b: Vec<u8> = a.into();
        assert_eq!(b.len(), VAL_REMOVE_SIZE);
        let (a, b) = ValRemovePdu::try_parse(&b).unwrap();
        assert_eq!(b, VAL_REMOVE_SIZE);
        assert_eq!(a.ssn, ssn);
    }

    #[test]
    fn test_val_lookup() {
        let ssn = "111111111111".to_owned();
        let a = ValLookupPdu::new(ssn.clone(), 12345, 1234);
        let b: Vec<u8> = a.into();
        assert_eq!(b.len(), VAL_LOOKUP_SIZE);
        let (a, b) = ValLookupPdu::try_parse(&b).unwrap();
        assert_eq!(b, VAL_LOOKUP_SIZE);
        assert_eq!(a.ssn, ssn);
        assert_eq!(a.sender_address, 12345);
        assert_eq!(a.sender_port, 1234);
    }

    #[test]
    fn test_val_lookup_response() {
        let ssn = "111111111111".to_owned();
        let name = "Test".to_owned();
        let email = "Emai".to_owned();
        let a = ValLookupResponsePdu::new(ssn.clone(), name.clone(), email.clone());
        let b: Vec<u8> = a.into();
        let len = 1 + SSN_LENGTH + 1 + name.len() + 1 + email.len();
        assert_eq!(b.len(), len);
        let (a, b) = ValLookupResponsePdu::try_parse(&b).unwrap();
        assert_eq!(b, len);
        assert_eq!(a.ssn, ssn);
        assert_eq!(a.name_length, name.len() as u8);
        assert_eq!(a.name, name);
        assert_eq!(a.email_length, email.len() as u8);
        assert_eq!(a.email, email);
    }

    #[test]
    fn test_stun_lookup() {
        let a = StunLookupPdu::new();
        let b: Vec<u8> = a.into();
        assert_eq!(b.len(), STUN_LOOKUP_SIZE);
        let (a, b) = StunLookupPdu::try_parse(&b).unwrap();
        assert_eq!(b, STUN_LOOKUP_SIZE);
    }

    #[test]
    fn test_stun_response() {
        let a = StunResponsePdu::new(12345);
        let b: Vec<u8> = a.into();
        assert_eq!(b.len(), STUN_RESPONSE_SIZE);
        let (a, b) = StunResponsePdu::try_parse(&b).unwrap();
        assert_eq!(b, STUN_RESPONSE_SIZE);
        assert_eq!(a.address, 12345);
    }
}
