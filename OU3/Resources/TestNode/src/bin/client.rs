use structopt::StructOpt;

use std::net::{Ipv4Addr, SocketAddr};

use mio::net::UdpSocket;
use ou2::pdu::*;
use ou2::socket_wrapper::{Message, UdpWrapper};

use std::{thread, time};

use std::io;
use std::io::prelude::*;

use serde::Deserialize;
use std::fs::File;

use mio::{Interest, Poll, Token};
use ou2::pdu::{ValInsertPdu, ValLookupPdu, ValRemovePdu, PDU};

#[derive(StructOpt, Debug)]
#[structopt(name = "Debug client")]
struct Opt {
    /// Tracker ip and port, on the format (xx.xx.xx.xx:pp)
    #[structopt(short, long)]
    pub tracker: SocketAddr,

    /// Node ip and port, on the format (xx.xx.xx.xx:pp)
    /// If node is not provided, the client tries to get an active
    /// node from the tracker.
    #[structopt(short, long)]
    node: Option<SocketAddr>,

    /// CSV file to insert from
    #[structopt(long)]
    csv: Option<String>,
    /// Delay between inserts for CSV inserts (seconds)
    #[structopt(long, default_value = "0")]
    delay: u64,
}

#[derive(Debug, Deserialize)]
struct Person {
    ssn: String,
    name: String,
    email: String,
}

fn main() -> std::io::Result<()> {
    let opt = Opt::from_args();
    let mut udp_socket: UdpSocket = UdpSocket::bind("0.0.0.0:0".parse().unwrap()).unwrap();
    let mut udp_wrapper: UdpWrapper = UdpWrapper::new();

    /* Get my own IP from tracker */
    let my_address = get_my_address(&mut udp_wrapper, &mut udp_socket, opt.tracker);
    let my_port = udp_socket.local_addr().unwrap().port();

    let node = if let Some(n) = opt.node {
        n
    } else {
        println!("Sending NET_GET_NODE to tracker to get a node");
        if let Some(n) = get_node(&mut udp_wrapper, &mut udp_socket, opt.tracker) {
            println!("Got NET_GET_NODE_RESPONSE from tracker with node {:?}", n);
            n
        } else {
            println!("NET_GET_NODE_RESPONSE was empty..");
            return Ok(());
        }
    };

    /* Send CSV file if it exists */
    if let Some(csv) = &opt.csv {
        send_csv(csv.into(), &opt, &mut udp_wrapper, &mut udp_socket, node)?;
    }

    let stdin = io::stdin();
    /* Read operations interactively from the user */
    let mut buf = String::new();
    loop {
        println!(
            "Write insert, remove or lookup in order to perform that operation, or exit to exit"
        );
        buf.clear();
        stdin.read_line(&mut buf)?;

        match buf.trim() {
            "insert" => {
                let insert_pdu =
                    ValInsertPdu::new(ask_for("ssn"), ask_for("name"), ask_for("email"));
                udp_wrapper.send(&mut udp_socket, insert_pdu.into(), node);
            }
            "remove" => {
                let remove_pdu = ValRemovePdu::new(ask_for("ssn"));
                udp_wrapper.send(&mut udp_socket, remove_pdu.into(), node);
            }
            "lookup" => {
                let lookup_pdu = ValLookupPdu::new(ask_for("ssn"), my_address.into(), my_port);
                udp_wrapper.send(&mut udp_socket, lookup_pdu.into(), node);

                if let (PDU::ValLookupResponse(pdu), _) =
                    poll_response(&mut udp_wrapper, &mut udp_socket)
                {
                    println!("Got VAL_LOOKUP_RESPONSE");
                    println!("ssn: {}, name: {}, email: {}", pdu.ssn, pdu.name, pdu.email);
                } else {
                    panic!("Expected ValLookUpResponse, got something else ");
                }
            }
            "exit" => return Ok(()),
            _ => {
                println!("Unknown command");
            }
        }
    }
}

fn poll_response(udp_wrapper: &mut UdpWrapper, mut udp_socket: &mut UdpSocket) -> Message {
    let poll = Poll::new().unwrap();
    const RESPONSE: Token = Token(1);

    poll.registry()
        .register(udp_socket, RESPONSE, Interest::READABLE)
        .unwrap();

    loop {
        udp_wrapper.try_read(&mut udp_socket);

        if let Some(x) = udp_wrapper.next_pdu() {
            poll.registry().deregister(udp_socket).unwrap();
            return x;
        }
    }
}

fn get_my_address(
    mut udp_wrapper: &mut UdpWrapper,
    mut udp_socket: &mut UdpSocket,
    tracker_addr: SocketAddr,
) -> Ipv4Addr {
    let lookup = StunLookupPdu::new();
    udp_wrapper.send(&mut udp_socket, lookup.into(), tracker_addr);

    if let (PDU::StunResponse(pdu), _) = poll_response(&mut udp_wrapper, &mut udp_socket) {
        let own_address: Ipv4Addr = pdu.address.into();
        println!("Got STUN_RESPONSE, my address is: {:?}", own_address);
        return own_address;
    } else {
        panic!("Expected stun response, got something else");
    }
}

fn get_node(
    mut udp_wrapper: &mut UdpWrapper,
    mut udp_socket: &mut UdpSocket,
    tracker_addr: SocketAddr,
) -> Option<SocketAddr> {
    let lookup = NetGetNodePdu::new();
    udp_wrapper.send(&mut udp_socket, lookup.into(), tracker_addr);

    if let (PDU::NetGetNodeResponse(pdu), _) = poll_response(&mut udp_wrapper, &mut udp_socket) {
        return if pdu.address == 0 && pdu.port == 0 {
            None
        } else {
            let ip: Ipv4Addr = pdu.address.into();
            Some((ip, pdu.port).into())
        };
    } else {
        panic!("Expected stun response, got something else");
    }
}

fn ask_for(s: &str) -> String {
    print!("{}: ", s);
    io::stdout().flush().ok().expect("Could not flush stdout");
    let mut buf = String::new();
    io::stdin().read_line(&mut buf).unwrap();
    buf.trim().to_owned()
}

fn send_csv(
    csv: String,
    opt: &Opt,
    udp_wrapper: &mut UdpWrapper,
    mut udp_socket: &mut UdpSocket,
    node: SocketAddr,
) -> std::io::Result<()> {
    let file = File::open(csv);
    let mut rdr = csv::Reader::from_reader(file.unwrap());
    let seconds = time::Duration::from_secs(opt.delay);

    for result in rdr.deserialize() {
        let person: Person = result?;
        let insert_pdu = ValInsertPdu::new(person.ssn, person.name, person.email);
        udp_wrapper.send(&mut udp_socket, insert_pdu.into(), node);
        thread::sleep(seconds);
    }

    Ok(())
}
