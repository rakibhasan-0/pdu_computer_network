# Building

1. Install [`rustup`](https://rustup.rs/)
2. Run `cargo build --release`
3. Copy built files to current directory: `cp target/release/{node, tracker, client} .`

# Running the programs

After building, you should have three binaries: tracker, node and client. Below follows a short description of how they are to be used.

## Tracker

The tracker takes one mandatory argument, the port to listen for incoming UDP traffic.
Optional parameters are shown by running the tracker with the `--help` parameter.

## Node

The `node` binary is an implementation of the described node. Use the `--help` parameter
to see what the parameters are. The node can be shutdown by sending `SIGINT`, either via
terminal or by pressing `CTRL+C` in the terminal. If the node crashes due to a malformed 
PDU or similar, the node can be shut down by sending `SIGQUIT`, via `CTRL+\`.


## Client

The `client` allows you to perform value operations to a node network, as well as bulk inserts
via a `csv` file. A sample `csv` is provided in [data.csv](data.csv). The client can either
connect to a specific node, or use the tracker to get a random node from the network.
Use the `--help` parameter to see the available parameters.

