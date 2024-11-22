use std::net::Ipv4Addr;
use structopt::StructOpt;

#[derive(StructOpt, Debug)]
#[structopt(name = "Node", version = "1.1")]
struct Opt {
    /// Tracker address
    tracker_address: Ipv4Addr,
    /// Tracker port
    tracker_port: u16,
}

fn main() {
    let opt = Opt::from_args();
    let mut node = node::Node::new((opt.tracker_address, opt.tracker_port).into());
    node.run();
}

#[allow(dead_code)]
mod node {
    use ou2::socket_wrapper::*;
    use std::net::{IpAddr, Ipv4Addr, Shutdown, SocketAddr, SocketAddrV4, TcpListener};
    use std::time::{Duration, Instant};

    use mio::net::{TcpStream, UdpSocket};
    use mio::{Events, Interest, Poll, Token};
    use std::io::prelude::*;

    use std::sync::atomic::{AtomicBool, Ordering};
    use std::sync::Arc;

    use ctrlc;

    use ou2::pdu::*;
    use State::*;

    const UDP: Token = Token(0);
    const SUCCESSOR: Token = Token(1);
    const PREDECESSOR: Token = Token(2);

    #[derive(Debug)]
    enum Source {
        UDP(SocketAddr),
        Predecessor(SocketAddr),
        Successor(SocketAddr),
    }

    #[derive(Debug)]
    enum State {
        Q1,
        Q2,
        Q3,
        Q4,
        Q5,
        Q6,
        Q7,
        Q8,
        Q9,
        Q10,
        Q11,
        Q12,
        Q13,
        Q14,
        Q15,
        Q16,
        Q17,
        Q18,
    }

    pub struct Node {
        state: State,
        own_address: Option<Ipv4Addr>,
        values: Vec<Entry>,
        hash_range: (u8, u8),
        last_alive: Instant,
        last_pdu: Option<PDU>,
        successor_listen: Option<SocketAddr>,
        should_close: Arc<AtomicBool>,
        running: bool,
        tracker_addr: SocketAddr,
        // A
        udp_wrapper: UdpWrapper,
        udp_socket: UdpSocket,
        // B
        successor_wrapper: TcpWrapper,
        successor: Option<TcpStream>,

        // E
        predecessor_wrapper: TcpWrapper,
        predecessor: Option<TcpStream>,
        // D
        listen_socket: TcpListener,
    }

    impl Node {
        pub fn new(tracker_addr: SocketAddr) -> Self {
            let should_close = Arc::new(AtomicBool::new(false));
            let sc = should_close.clone();
            ctrlc::set_handler(move || {
                sc.store(true, Ordering::SeqCst);
            })
            .expect("Error setting sigint handler");

            let n = Node {
                running: true,
                state: Q1,
                tracker_addr,
                udp_socket: UdpSocket::bind("0.0.0.0:0".parse().unwrap()).unwrap(),
                udp_wrapper: UdpWrapper::new(),
                successor_wrapper: TcpWrapper::new(),
                predecessor_wrapper: TcpWrapper::new(),
                successor: None,
                successor_listen: None,
                predecessor: None,
                listen_socket: TcpListener::bind("0.0.0.0:0").unwrap(),
                own_address: None,
                values: Vec::new(),
                hash_range: (0, 0),
                last_alive: Instant::now() - Duration::from_secs(100),
                last_pdu: None,
                should_close,
            };
            println!(
                "Node listening on UDP {:?}, accepts TCP connections on {:?}",
                n.udp_socket.local_addr().unwrap(),
                n.get_listen_addr(),
            );

            n
        }

        pub fn run(&mut self) {
            while self.running {
                match &self.state {
                    Q1 => self.q1(),
                    Q2 => self.q2(),
                    Q3 => self.q3(),
                    Q4 => self.q4(),
                    Q5 => self.q5(),
                    Q6 => self.q6(),
                    Q7 => self.q7(),
                    Q8 => self.q8(),
                    Q9 => self.q9(),
                    Q10 => self.q10(),
                    Q11 => self.q11(),
                    Q12 => self.q12(),
                    Q13 => self.q13(),
                    Q14 => self.q14(),
                    Q15 => self.q15(),
                    Q16 => self.q16(),
                    Q17 => self.q17(),
                    Q18 => self.q18(),
                }
            }
        }
    }

    #[derive(Debug)]
    struct Entry {
        ssn: String,
        name: String,
        email: String,
    }

    impl Entry {
        pub fn new(ssn: String, name: String, email: String) -> Self {
            Entry { ssn, name, email }
        }

        pub fn hash(&self) -> u8 {
            return Entry::hash_ssn(&self.ssn);
        }

        pub fn hash_ssn(ssn: &String) -> u8 {
            let mut hash: u32 = 5381;
            for c in ssn.chars() {
                let (h, _) = (hash << 5)
                    .overflowing_add(hash)
                    .0
                    .overflowing_add(c as u32);
                hash = h;
            }

            (hash % 256) as u8
        }
    }

    #[test]
    fn test_hash() {
        let ssn = String::from("aaaaabbbbbcc");
        assert_eq!(Entry::hash_ssn(&ssn), 26);
    }

    /// States
    impl Node {
        fn q1(&mut self) {
            println!("[Q1]");
            let lookup = StunLookupPdu::new();
            println!(
                "    Node started, sending STUN_LOOKUP to tracker: {:?}",
                self.tracker_addr
            );
            self.udp_wrapper
                .send(&mut self.udp_socket, lookup.into(), self.tracker_addr);
            self.state = Q2;
        }

        fn q2(&mut self) {
            println!("[Q2]");
            if let (PDU::StunResponse(pdu), _) = self.await_pdu_udp() {
                self.own_address = Some(pdu.address.into());
                println!(
                    "    Got STUN_RESPONSE, my address is: {:?}",
                    self.own_address.unwrap()
                );
                self.state = Q3;
            } else {
                panic!("Expected stun response, got something else :(");
            }
        }

        fn q3(&mut self) {
            println!("[Q3]");
            let get_node = NetGetNodePdu::new();
            self.udp_wrapper
                .send(&mut self.udp_socket, get_node.into(), self.tracker_addr);
            if let (PDU::NetGetNodeResponse(pdu), _) = self.await_pdu_udp() {
                if pdu.address == 0 && pdu.port == 0 {
                    println!("    I am the first node to join the network");
                    self.state = Q4;
                } else {
                    self.last_pdu = Some(pdu.into());
                    self.state = Q7;
                }
            } else {
                panic!("Expected NET_GET_NODE_RESPONSE , got something else :(");
            }
        }

        fn q4(&mut self) {
            println!("[Q4]");
            self.hash_range = (0, 255);
            self.state = Q6;
        }

        fn q5(&mut self) {
            println!("[Q5]");

            let next_node = match self.last_pdu.take() {
                Some(PDU::NetJoin(pdu)) => pdu,
                _ => panic!("Invalid state change, last_pdu is not NetGetNodeResponse"),
            };

            let addr = next_node.get_src_socket_addr();
            let mut socket = self.connect_to_successor(addr);
            let (mins, maxs) = self.split_range();
            println!("    Other hash-range is {:?}", (mins, maxs));
            println!("    New hash-range is {:?}", self.hash_range);

            let local = self.get_listen_addr();
            let join_response =
                NetJoinResponsePdu::new(self.own_address.unwrap().into(), local.port(), mins, maxs);
            self.successor_wrapper
                .send(&mut socket, join_response.into());
            self.successor = Some(socket);
            self.transfer(true, mins, maxs);

            self.accept_predecessor();

            self.state = Q6;
        }

        fn q6(&mut self) {
            println!("[Q6] ({} entries stored)", self.values.len());

            self.send_alive();

            let mut poll = Poll::new().unwrap();
            if let Some(s) = self.successor.as_mut() {
                poll.registry()
                    .register(s, SUCCESSOR, Interest::READABLE)
                    .unwrap();
            }
            if let Some(p) = self.predecessor.as_mut() {
                poll.registry()
                    .register(p, PREDECESSOR, Interest::READABLE)
                    .unwrap();
            }

            poll.registry()
                .register(&mut self.udp_socket, UDP, Interest::READABLE)
                .unwrap();

            let mut events = Events::with_capacity(10);

            poll.poll(&mut events, Some(Duration::from_secs(5))).ok();

            for event in events.iter() {
                match event.token() {
                    UDP => {
                        self.udp_wrapper.try_read(&mut self.udp_socket);
                        while let Some((pdu, sender)) = self.udp_wrapper.next_pdu() {
                            self.handle_pdu(pdu, Source::UDP(sender));
                        }
                    }
                    SUCCESSOR => {
                        if let Some(suc) = &mut self.successor {
                            if self.successor_wrapper.try_read(suc) {
                                println!("    Successor disconnected, removing..");
                                self.successor = None;
                            }
                            while let Some((pdu, sender)) = self.successor_wrapper.next_pdu() {
                                self.handle_pdu(pdu, Source::Successor(sender));
                            }
                        }
                    }
                    PREDECESSOR => {
                        if let Some(pred) = &mut self.predecessor {
                            if self.predecessor_wrapper.try_read(pred) {
                                println!("    Predecessor disconnected, removing..");
                            }

                            while let Some((pdu, sender)) = self.predecessor_wrapper.next_pdu() {
                                self.handle_pdu(pdu, Source::Predecessor(sender));
                            }
                        }
                    }
                    _ => panic!("What token is this?"),
                }
            }

            poll.registry().deregister(&mut self.udp_socket).unwrap();
            if let Some(s) = self.successor.as_mut() {
                poll.registry().deregister(s).unwrap();
            }

            if let Some(p) = self.predecessor.as_mut() {
                poll.registry().deregister(p).unwrap();
            }

            if self.should_close.load(Ordering::SeqCst) {
                println!("    Close requested!");
                self.state = Q10;
            }
        }

        fn q7(&mut self) {
            println!("[Q7]");
            let next_node = match self.last_pdu.take() {
                Some(PDU::NetGetNodeResponse(pdu)) => pdu,
                _ => panic!("Invalid state change, last_pdu is not NetGetNodeResponse"),
            };

            let remote = next_node.get_addr();
            println!(
                "    I am not the first node, sending NET_JOIN to {:?}",
                remote
            );

            let local = self.get_listen_addr();
            let net_join = NetJoinPdu::new(
                self.own_address.unwrap().into(),
                local.port(),
                0,
                0,
                0,
            );

            self.udp_wrapper
                .send(&mut self.udp_socket, net_join.into(), remote.into());

            self.accept_predecessor();

            self.state = Q8;
        }

        fn q8(&mut self) {
            println!("[Q8]");
            let (pdu, sender) = match self.await_pdu(false) {
                (PDU::NetJoinResponse(pdu), sender) => (pdu, sender),
                _ => panic!("Expected NET_JOIN_RESPONSE, got something else :("),
            };

            self.hash_range = (pdu.range_start, pdu.range_end);
            println!(
                "    Got NET_JOIN_RESPONSE from {:?}, my range is {:?}",
                sender, self.hash_range
            );
            println!("    Connecting to successor {:?}", pdu.get_next_addr());

            let addr = pdu.get_next_addr();
            let successor = self.connect_to_successor(addr);
            self.successor_listen = Some(addr);

            self.successor = Some(successor);

            self.state = Q6;
        }

        fn q9(&mut self) {
            println!("[Q9]");
            match self.last_pdu.take().unwrap() {
                PDU::ValInsert(p) => {
                    self.handle_val_insert(p);
                }
                PDU::ValRemove(p) => {
                    self.handle_val_remove(p);
                }
                PDU::ValLookup(p) => {
                    self.handle_val_lookup(p);
                }
                _ => panic!("Invalid PDU for state Q9"),
            }
            self.state = Q6;
        }

        fn q10(&mut self) {
            println!("[Q10]");
            if self.successor.is_none() {
                println!("    I am the last node, bye!");
                self.running = false;
                return;
            }

            self.state = Q11;
        }

        fn q11(&mut self) {
            println!("[Q11]");

            let (min, max) = self.hash_range;

            let successor = match self.successor.as_mut() {
                Some(s) => s,
                _ => panic!("Missing successor socket >("),
            };

            let predecessor = match self.predecessor.as_mut() {
                Some(s) => s,
                _ => panic!("Missing predecessor socket >("),
            };

            let new_range = NetNewRangePdu::new(min, max);

            let to_successor = min == 0;
            if to_successor {
                println!("    Sending NET_NEW_RANGE to successor");
                self.successor_wrapper.send(successor, new_range.into());
                successor.flush().unwrap();
            } else {
                println!("    Sending NET_NEW_RANGE to predecessor");
                self.predecessor_wrapper.send(predecessor, new_range.into());
                predecessor.flush().unwrap();
            }


            let (_pdu, _sender) = match self.await_pdu(to_successor) {
                (PDU::NetNewRangeResponse(pdu), sender) => (pdu, sender),
                x => panic!("Expected NET_NEW_RANGE_RESPONSE, got {:?}", x),
            };
            self.state = Q18;
        }

        fn q12(&mut self) {
            println!("[Q12]");

            if self.successor.is_none() {
                println!("    I am alone, moving to Q5");
                self.state = Q5;
                return;
            }

            let pdu = match self.last_pdu.take() {
                Some(PDU::NetJoin(pdu)) => pdu,
                _ => panic!("Invalid state change, last_pdu is not NetJoin"),
            };

            let own: SocketAddr = self.get_listen_addr().into();
            if own == pdu.get_max_socket_addr() {
                println!(
                    "    I am the node with the maximum span! ({})",
                    pdu.max_span
                );
                self.last_pdu = Some(pdu.into());
                self.state = Q13;
                return;
            }
            self.last_pdu = Some(pdu.into());

            self.state = Q14;
        }

        fn q13(&mut self) {
            println!("[Q13]");
            let net_close = NetCloseConnectionPdu::new();
            let successor_addr = self.get_successor_addr();
            let mut socket = self.successor.as_mut().unwrap();

            let pdu = match self.last_pdu.take() {
                Some(PDU::NetJoin(pdu)) => pdu,
                _ => panic!("Invalid state change, last_pdu is not NetJoin"),
            };

            println!("    Sending NET_CLOSE_CONNECTION to successor");
            self.successor_wrapper.send(&mut socket, net_close.into());
            socket.flush().unwrap();
            socket.shutdown(Shutdown::Both).unwrap();
            self.successor = None;
            let addr = pdu.get_src_socket_addr();
            let stream = self.connect_to_successor(addr);
            self.successor_listen = Some(addr);

            println!("    Connected to new successor {:?}", stream.peer_addr());
            self.successor = Some(stream);

            self.last_pdu = None;

            let (mins, maxs) = self.split_range();
            println!("    Other hash-range is {:?}", (mins, maxs));
            println!("    New hash-range is {:?}", self.hash_range);

            let join_response = NetJoinResponsePdu::new(
                (*successor_addr.ip()).into(),
                successor_addr.port(),
                mins,
                maxs,
            );

            let mut socket = self.successor.as_mut().unwrap();
            println!("    Sending join response");
            self.successor_wrapper
                .send(&mut socket, join_response.into());

            //Transfer all between mins and maxs
            self.transfer(true, mins, maxs);

            self.state = Q6;
        }

        fn q14(&mut self) {
            println!("[Q14]");

            let mut pdu = match self.last_pdu.take() {
                Some(PDU::NetJoin(pdu)) => pdu,
                _ => panic!("Invalid state change, last_pdu is not NetJoin"),
            };

            let (min, max) = self.hash_range;
            if max - min > pdu.max_span {
                println!("    Updating max fields");
                pdu.max_span = max - min;
                let a = self.get_listen_addr();
                pdu.max_address = (*a.ip()).into();
                pdu.max_port = a.port();
            }

            println!("    Forwarding to successor");

            if let Some(socket) = &mut self.successor {
                self.successor_wrapper.send(socket, pdu.into());
            } else {
                panic!("Successor is not set, impossible!");
            }

            self.state = Q6;
        }

        fn q15(&mut self) {
            println!("[Q15]");

            let new_range = match self.last_pdu.take() {
                Some(PDU::NetNewRange(pdu)) => pdu,
                _ => panic!("Invalid state change, last_pdu is not NetNewRange"),
            };
            let (min, max) = self.hash_range;
            println!("    Current range is: ({}, {})", min, max);

            let new_range_response = NetNewRangeResponsePdu::new();
            if max != 255 && new_range.range_start == max + 1 {
                println!("    Sending NET_NEW_RANGE_RESPONSE to successor");
                self.hash_range = (min, new_range.range_end);
                self.successor_wrapper.send(self.successor.as_mut().unwrap(), new_range_response.into());
            } else {
                println!("    Sending NET_NEW_RANGE_RESPONSE to predecessor");
                self.hash_range = (new_range.range_start, max);
                self.predecessor_wrapper.send(self.predecessor.as_mut().unwrap(), new_range_response.into());
            }
            println!("    New range is: ({}, {})", self.hash_range.0, self.hash_range.1);


            self.state = Q6;
        }

        fn q16(&mut self) {
            println!("[Q16]");

            let net_leaving = match self.last_pdu.take() {
                Some(PDU::NetLeaving(pdu)) => pdu,
                _ => panic!("Invalid state change, last_pdu is not NetLeaving"),
            };

            if net_leaving.new_address == self.own_address.unwrap().into() &&
               net_leaving.new_port == self.get_listen_addr().port() {
                println!("    I am the last node.");
                self.successor = None;
                self.predecessor = None;

            } else {
                self.successor = None;

                self.successor = Some(self.connect_to_successor(net_leaving.get_new_addr()));
            }


            self.state = Q6;
        }

        fn q17(&mut self) {
            println!("[Q17]");
            if let Some(mut s) = self.predecessor.take() {
                println!("    Disconnecting from predecessor");
                s.flush().unwrap();
                s.shutdown(Shutdown::Both).unwrap();
                self.predecessor = None;
            }
            let (min, max) = self.hash_range;
            if min == 0 && max == 255 {
                println!("    I am the last node");
            } else {
                println!("    Awaiting new predecessor");
                self.accept_predecessor();
            }

            self.state = Q6;
        }

        fn q18(&mut self) {
            println!("[Q18]");

            let (min, max) = self.hash_range;
            let to_successor = min == 0;

            if to_successor {
                println!("    Transferring all entries to successor");
                self.transfer(true, min, max);
            } else {
                println!("    Transferring all entries to predecessor");
                self.transfer(false, min, max);
            }

            let successor = match self.successor.as_mut() {
                Some(s) => s,
                _ => panic!("Missing successor socket >("),
            };

            let close = NetCloseConnectionPdu::new();
            self.successor_wrapper.send(successor, close.into());
            successor.flush().unwrap();
            successor.shutdown(Shutdown::Both).unwrap();

            let to_connect = match self.successor_listen {
                Some(SocketAddr::V4(addr)) => addr,
                _ => panic!("Successor listen address is not set"),
            };

            let leaving = NetLeavingPdu::new((*to_connect.ip()).into(), to_connect.port());

            let predecessor = match self.predecessor.as_mut() {
                Some(s) => s,
                _ => panic!("Missing predecessor socket >("),
            };

            println!("    Sending NET_LEAVING to predecessor");
            self.predecessor_wrapper.send(predecessor, leaving.into());
            predecessor.flush().unwrap();
            predecessor.shutdown(Shutdown::Both).unwrap();

            self.running = false;
        }

        fn handle_pdu(&mut self, pdu: PDU, sender: Source) {
            match pdu {
                PDU::NetJoin(p) => {
                    self.last_pdu = Some(p.into());
                    self.state = Q12;
                }
                PDU::NetCloseConnection(_) => match sender {
                    Source::Predecessor(_) => {
                        self.state = Q17;
                    }
                    _ => {
                        panic!("Got NET_CLOSE from someone other than my predecessor!");
                    }
                },
                PDU::NetNewRange(p) => {
                    self.last_pdu = Some(p.into());
                    self.state = Q15;
                }
                PDU::NetLeaving(p) => {
                    self.last_pdu = Some(p.into());
                    self.state = Q16;
                }
                PDU::ValInsert(p) => {
                    self.last_pdu = Some(p.into());
                    self.q9();
                }
                PDU::ValRemove(p) => {
                    self.last_pdu = Some(p.into());
                    self.q9();
                }
                PDU::ValLookup(p) => {
                    self.last_pdu = Some(p.into());
                    self.q9();
                }
                x => {
                    println!(
                        "Got PDU that node does not accept in the current state (Q6), was: {:?}",
                        x
                    );
                }
            }
        }
    }

    // Util functions
    impl Node {
        fn split_range(&mut self) -> (u8, u8) {
            let (min, max) = self.hash_range;
            let (minp, maxp) = (min, (max - min) / 2 + min);
            let (mins, maxs) = (maxp + 1, max);
            self.hash_range = (minp, maxp);
            (mins, maxs)
        }

        fn get_successor_addr(&self) -> SocketAddrV4 {
            if let SocketAddr::V4(a) = self.successor.as_ref().unwrap().peer_addr().unwrap() {
                return a;
            }

            panic!("Not bound to an IPv4 address!");
        }

        fn get_predecessor_addr(&self) -> SocketAddrV4 {
            if let SocketAddr::V4(a) = self.predecessor.as_ref().unwrap().peer_addr().unwrap() {
                return a;
            }

            panic!("Not bound to an IPv4 address!");
        }

        fn get_listen_addr(&self) -> SocketAddrV4 {
            if let SocketAddr::V4(a) = self.listen_socket.local_addr().unwrap() {
                return a;
            }

            panic!("Not bound to an IPv4 address!");
        }

        fn transfer(&mut self, to_successor: bool, range_start: u8, range_end: u8) {
            let mut socket = if to_successor {
                self.successor.as_mut().unwrap()
            } else {
                self.predecessor.as_mut().unwrap()
            };
            // Could use drain_filter from nightly
            for e in self
                .values
                .iter()
                .filter(|&x| x.hash() >= range_start && x.hash() <= range_end)
            {
                println!("Transferring: {:?}", e);
                let insert = ValInsertPdu::new(e.ssn.clone(), e.name.clone(), e.email.clone());
                self.successor_wrapper.send(&mut socket, insert.into());
            }
            socket.flush().unwrap();
            self.values
                .retain(|x| x.hash() < range_start || x.hash() > range_end);
        }

        fn send_alive(&mut self) {
            if self.last_alive.elapsed().as_secs() > 10 {
                self.last_alive = Instant::now();
                let alive = NetAlivePdu::new();
                self.udp_wrapper
                    .send(&mut self.udp_socket, alive.into(), self.tracker_addr);
            }
        }

        fn await_pdu_udp(&mut self) -> Message {
            let mut poll = Poll::new().unwrap();
            poll.registry()
                .register(&mut self.udp_socket, UDP, Interest::READABLE)
                .unwrap();
            let mut events = Events::with_capacity(1);
            let x = loop {
                poll.poll(&mut events, None).unwrap();
                self.udp_wrapper.try_read(&mut self.udp_socket);

                if let Some(x) = self.udp_wrapper.next_pdu() {
                    break x;
                }
            };

            poll.registry().deregister(&mut self.udp_socket).unwrap();

            x
        }

        fn await_pdu(&mut self, successor: bool) -> Message {
            let mut poll = Poll::new().unwrap();
            if successor {
                poll.registry()
                    .register(
                        self.successor.as_mut().unwrap(),
                        SUCCESSOR,
                        Interest::READABLE,
                    )
                    .unwrap();
            } else {
                poll.registry()
                    .register(
                        self.predecessor.as_mut().unwrap(),
                        PREDECESSOR,
                        Interest::READABLE,
                    )
                    .unwrap();
            }
            let mut events = Events::with_capacity(1);
            loop {
                poll.poll(&mut events, None).unwrap();

                if successor {
                    self.successor_wrapper
                        .try_read(self.successor.as_mut().unwrap());

                    if let Some(x) = self.successor_wrapper.next_pdu() {
                        poll.registry()
                            .deregister(self.successor.as_mut().unwrap())
                            .unwrap();
                        return x;
                    }
                } else {
                    self.predecessor_wrapper
                        .try_read(self.predecessor.as_mut().unwrap());

                    if let Some(x) = self.predecessor_wrapper.next_pdu() {
                        poll.registry()
                            .deregister(self.predecessor.as_mut().unwrap())
                            .unwrap();
                        return x;
                    }
                }
            }
        }

        fn accept_predecessor(&mut self) {
            let predecessor = match self.listen_socket.accept() {
                Ok((socket, addr)) => {
                    println!("    Accepted new predecessor {:?}", addr);
                    socket
                }
                Err(e) => {
                    panic!("Failed to accept connection {:?}", e);
                }
            };
            predecessor.set_nonblocking(true).unwrap();
            self.predecessor = Some(TcpStream::from_std(predecessor));
        }

        fn handle_val_insert(&mut self, pdu: ValInsertPdu) {
            if self.in_my_range(&pdu.ssn) {
                let e = Entry::new(pdu.ssn, pdu.name, pdu.email);
                println!("    Inserting ssn {:?}", e);
                self.values.push(e);
            } else {
                if let Some(socket) = &mut self.successor {
                    self.successor_wrapper.send(socket, pdu.into());
                    println!("    Forwarding val_insert to successor");
                } else {
                    panic!("Successor is not set, impossible!");
                }
            }
        }

        fn handle_val_lookup(&mut self, pdu: ValLookupPdu) {
            if self.in_my_range(&pdu.ssn) {
                let mut found = false;
                for entry in &self.values {
                    if entry.ssn == pdu.ssn {
                        let pdu_response = ValLookupResponsePdu::new(
                            entry.ssn.clone(),
                            entry.name.clone(),
                            entry.email.clone(),
                        );
                        println!("    Value found (ssn: {}).", entry.ssn);
                        let ip = Ipv4Addr::from(pdu.sender_address);
                        let addr = SocketAddr::new(IpAddr::V4(ip), pdu.sender_port.into());
                        self.udp_wrapper
                            .send(&mut self.udp_socket, pdu_response.into(), addr);
                        found = true;
                        break;
                    }
                }

                if !found {
                    println!("    Value does not exist, responding with empty pdu");
                    let pdu_response = ValLookupResponsePdu::new(
                        "000000000000".into(),
                        String::new(),
                        String::new(),
                    );
                    let ip = Ipv4Addr::from(pdu.sender_address);
                    let addr = SocketAddr::new(IpAddr::V4(ip), pdu.sender_port.into());
                    self.udp_wrapper
                        .send(&mut self.udp_socket, pdu_response.into(), addr);
                }
            } else {
                if let Some(socket) = &mut self.successor {
                    self.successor_wrapper.send(socket, pdu.into());
                    println!("    Forwarding val_lookup to successor");
                } else {
                    panic!("Successor is not set, impossible!");
                }
            }
        }

        fn handle_val_remove(&mut self, pdu: ValRemovePdu) {
            if self.in_my_range(&pdu.ssn) {
                println!("    Removing ssn {}", pdu.ssn);
                self.values.retain(|x| x.ssn != pdu.ssn);
            } else {
                if let Some(socket) = &mut self.successor {
                    self.successor_wrapper.send(socket, pdu.into());
                    println!("    Forwarding val_remove to successor");
                } else {
                    panic!("Successor is not set, impossible!");
                }
            }
        }

        fn in_my_range(&mut self, ssn: &String) -> bool {
            let (min, max) = self.hash_range;
            let ssn_hash = Entry::hash_ssn(&ssn);
            min <= ssn_hash && ssn_hash <= max
        }

        fn connect_to_successor(&mut self, addr: SocketAddr) -> TcpStream {
            let socket = std::net::TcpStream::connect(addr).expect("Failed to connect to successor");
            self.successor_listen = Some(addr);

            println!(
                "    Connected to new successor {:?}",
                socket.peer_addr().unwrap()
            );
            socket.set_nonblocking(true).expect("Failed to set socket non-blocking");
            TcpStream::from_std(socket)
        }
    }
}
