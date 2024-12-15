# High Level Iterative Approach

The desired outcome of this refactor will be splitting the existing code up into a stack of new components. A layer hides all functionality of the layer below it to reduce the complexity like the OSI stack intends to.
The refactor starts at the top layer, wiring up the old implementation piecewise to the top layer.
Once the top layer is wired up to the old implementation we will move down to the next layer.
This will repeat until we reach the bottom layer.
Once the old implementation is wired up into these new clearly defined layers, we can fixup or replace different parts of each layer one at a time as needed.

Working down from each layer will let us pick apart the old implementation (if needed) that we would wire up to the new base classes for that layer we are defining now without worrying about what is below it (yet).

This refactor is very able to be split up into small work units that (ideally) do not conflict with each other.


PDU: https://en.wikipedia.org/wiki/Protocol_data_unit

# The New Layers

From top to bottom the new layers are:

* Platform Layer
* Flow Layer
* Routing Layer
* Onion Layer
* Link Layer
* Wire Layer


## Platform Layer

This is the top layer, it is responsibile ONLY to act as a handler of reading data from the "user" (via tun interface or whatever) to forward to the flow layer as desired, and to take data from the flow layer and send it to the "user".
Any kind of IP/dns mapping or traffic isolation details are done here. Embedded lokinet would be implemented in this layer as well, as it is without a full tun interface.

Platform layer PDU are what the OS gives us and we internally convert them into flow layer PDU and hand them off to the flow layer.


## Flow Layer

This layer is tl;dr mean to multiplex data from the platform layer across the routing layer and propagating PDU from the routing to the platform layer if needed.

The flow layer is responsible for sending platform layer PDU across path we have already established.
This layer is informed by the routing layer below it of state changes in what paths are available for use.
The flow layer requests from the layer below to make new paths if it wishes to get new ones on demand.
This layer will recieve routing layer PDU from the routing layer and apply any congestion control needed to buffer things to the os if it is needed at all.

Flow layer PDU are (data, ethertype, src-pubkey, dst-pubkey, isolation-metric) tuples.
Data is the datum we are tunneling over lokinet. ethertype tells us what kind of datum this is, e.g. plainquic/ipv4/ipv6/auth/etc.
src-pubkey and dst-pubkey are public the ed25519 public keys of each end of the flow in use.
The isolation metric is a piece of metadata we use to distinguish unique flows (convotag). in this new seperation convotags explicitly do not hand over across paths.


## Routing Layer

This layer is tl;dr meant for path management but not path building.

The routing layer is responsible for sending/recieving flow layer PDU, DHT requests/responses, latency testing PDU and any other kind of PDU we send/recieve over the onion layer.
This layer will be responsible for managing paths we have already built across lokinet.
The routing layer will periodically measure path status/latency, and do any other kinds of perioidic path related tasks post build.
This layer when asked for a new path from the flow layer will use one that has been prebuilt already and if the number of prebuilt paths is below a threshold we will tell the onion layer to build more paths.
The routing layer will recieve path build results be their success/fail/timeout from the onion layer that were requested and apply any congestion control needed at the pivot router.

Routing layer PDU are (data, src-path, dst-path) tuples.
Data is the datum we are transferring between paths.
src-path and dst-path are (pathid, router id) tuples, the source being which path this routing layer PDU originated from, destination being which path it is going to.
In the old model, router id is always the router that recieves it as the pivot router, this remains the same unless we explicitly provide the router-id.
This lets us propagate hints to DHT related PDU held inside the datum.


## Onion Layer

The onion layer is repsonsible for path builds, path selection logic and low level details of encrypted/decrypting PDU that are onion routed over paths.
This layer is requested by the routing layer to build a path to a pivot router with an optional additional constraints (e.g. unique cidr/operator/geoip/etc, latency constaints, hop length, path lifetime).
The onion layer will encrypt PDU and send them to link layer as (frame/edge router id) tuples, and recieve link layer frames from edge routers, decrypt them and propagate them as needed to the routing layer.
This layer also handles transit onion traffic and transit path build responsibilities as a snode and apply congestion control as needed per transit path.

The onion layer PDU are (data, src-path, dst-path) tuples.
src-path and dst-path are (router-id, path-id) tuples which contain the ed25519 pubkey of the node and the 128 bit path-id it was associated with.
Data is some datum we are onion routing that we would apply symettric encryption as needed before propagating to upper or lower layers.


## Link Layer

The link layer is responsbile for transmission of frames between nodes.
This layer will handle queuing and congestion control between wire proto sessions between nodes.
The link layer is will initate and recieve wire session to/from remote nodes.

The link layer PDU is (data, src-router-id, dst-router-id) tuples.
Tata is a datum of a link layer frame.
src-router-id and dst-router-id are (ed25519-pubkey, net-addr, wire-proto-info) tuples.
The ed25519 pubkey is a .snode address, (clients have these too but they are ephemeral).
net-addr is an (ip, port) tuple the node is reachable via the wire protocol.
wire-proto-info is dialect specific wire protocol specific info.

## Wire Layer

The wire layer is responsible for transmitting link layer frames between nodes.
All details here are specific to each wire proto dialect.
