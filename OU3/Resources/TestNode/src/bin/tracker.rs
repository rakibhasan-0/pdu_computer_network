use std::net::SocketAddr;
use structopt::StructOpt;

use ou2::pdu::PDU::*;
use ou2::pdu::*;
use ou2::socket_wrapper::UdpWrapper;

use mio::net::UdpSocket;
use mio::{Events, Interest, Poll, Token};

use std::collections::HashMap;
use std::time;
use time::Instant;

#[derive(StructOpt, Debug)]
#[structopt(name = "Node")]
struct Opt {
    /// Tracker listen port
    tracker_port: u16,

    /// Tracker node timeout (in seconds)
    #[structopt(long, short, default_value = "30")]
    timeout: u64,
}

struct Node {
    pub last_alive: Instant,
}

impl Node {
    pub fn new() -> Self {
        Node {
            last_alive: Instant::now(),
        }
    }
}

fn main() {
    let opt = Opt::from_args();

    let mut socket =
        UdpSocket::bind(format!("0.0.0.0:{}", opt.tracker_port).parse().unwrap()).unwrap();
    let mut nodes = HashMap::new();
    println!("Tracker listening on {:?}", socket.local_addr().unwrap());
    let mut poll = Poll::new().unwrap();
    let mut events = Events::with_capacity(1);
    let timeout = std::time::Duration::from_secs(5);
    let mut wrapper = UdpWrapper::new();
    poll.registry()
        .register(&mut socket, Token(0), Interest::READABLE)
        .unwrap();

    loop {
        poll.poll(&mut events, Some(timeout)).unwrap();
        for _ in events.iter() {
            wrapper.try_read(&mut socket);
            while let Some((pdu, sender)) = wrapper.next_pdu() {
                match pdu {
                    StunLookup(_) => {
                        println!("Got STUN_LOOKUP from {:?}", sender);
                        if let SocketAddr::V4(s) = sender {
                            let r = StunResponsePdu::new((*s.ip()).into());
                            wrapper.send(&mut socket, r.into(), sender);
                        } else {
                            panic!("Somehow i got contacted over IPV6");
                        }
                    }
                    NetAlive(_) => {
                        println!("Got NET_ALIVE from {:?}", sender);
                        let node = nodes.entry(sender).or_insert(Node::new());
                        node.last_alive = Instant::now();
                    }
                    NetGetNode(_) => {
                        println!("Got NET_GET_NODE from {:?}", sender);

                        let r = if nodes.is_empty() {
                            println!("    No nodes connected. Giving empty response.");
                            NetGetNodeResponsePdu::new(0, 0)
                        } else {
                            println!("    {} nodes connected.", nodes.len());
                            let (k, _) = nodes.iter().next().unwrap();
                            if let SocketAddr::V4(k) = k {
                                println!("    Responding with {:?}", k);
                                NetGetNodeResponsePdu::new((*k.ip()).into(), k.port())
                            } else {
                                panic!("Somehow i got contacted over IPV6");
                            }
                        };

                        wrapper.send(&mut socket, r.into(), sender);
                    }
                    _ => {
                        println!("What did I just receive??");
                    }
                }
            }
        }

        nodes.retain(|&k, v| {
            if v.last_alive.elapsed().as_secs() >= opt.timeout {
                println!("Dropping {:?} due to inactivity.", k);
                false
            } else {
                true
            }
        });
    }
}
