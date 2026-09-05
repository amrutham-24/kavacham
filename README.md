# kavacham

A multi-hop, fault-tolerant underground worker safety network that maintains communication and situational awareness despite loss of conventional connectivity.

The prototype demonstrates the communication and safety logic at small scale; deployment in an actual underground mine would require certified intrinsically-safe hardware, mine-specific gas sensors, RF propagation testing, redundant gateways/relays, and regulatory approval.

---

## BLE Multi-hop Networking

"Individual BLE links are short-to-medium range and highly dependent on mine geometry. The system therefore uses multi-hop networking: workers communicate with nearby nodes, and packets hop through neighbouring workers/nodes until reaching a gateway."

```
Worker Node
    │
    │ BLE packet
    ▼
ESP32 Gateway
    │
    │ application data
    ▼
MQTT client
    │
    │ Wi-Fi/Ethernet
    ▼
MQTT Broker
    │
    ▼
Dashboard
```

---

## LoRa Secondary Path

"LoRa provides a secondary low-data-rate communication path where a LoRa relay/gateway is within RF reach."

In real mine deployment,

```
Worker LoRa
     ↓
LoRa relay
     ↓
LoRa relay
     ↓
Surface gateway
```

---

## Positioning

```
UWB anchors
      ↓
Worker UWB tag
      ↓
Position estimate
```

simulation:

"RSSI-based positioning is used in the prototype. In the production version, this module can be replaced by UWB-based positioning for significantly more precise localization."

```
RSSI
 ↓
estimated distance
 ↓
relative worker location
 ↓
dashboard
```

---

## Gas Sensing

"MQ-6 used as a proof-of-concept flammable-gas sensor; production deployment requires certified mine-specific gas detection."

---

## Fault Tolerance

what if Node 2:NEAREST NODE dies?
There comes concept of multi-hop routing + alternate paths.

---

last known location is being displayed in the rescue dashboard, in cases of workers getting disappeared

---

## Naming Note

If you're implementing your own ESP-NOW-like routing over BLE rather than the Bluetooth Mesh specification, call it something like:

BLE-based multi-hop WSN

rather than claiming full Bluetooth Mesh compliance.

---

## ADD-ONS

### Packet Priority

Priority 0 → telemetry
Priority 1 → warning
Priority 2 → fall
Priority 3 → SOS

A systems-level feature for the system.
if the network is congested:

```
SOS
 ↓
FALL
 ↓
CRITICAL GAS
 ↓
WARNING
 ↓
normal temperature
```

---

WSN2's LED turns red when it receives WSN1's SOS.

This demonstrates:
A worker's emergency is immediately visible to nearby workers, even without the gateway.
an emergency can propagate hop-by-hop even when the worker who triggered it cannot directly reach the gateway.

---

### Worker & Node Health Monitoring

A heartbeat can detect that a node has stopped communicating, which can indicate a system crash, power failure, radio failure, or the worker moving out of range.

If the gateway hasn't received a heartbeat from WSN1 for, say, 3 consecutive intervals:

Heartbeat interval = 5 s

```
0 s   ✓
5 s   ✓
10 s  ✓
15 s  ✗
20 s  ✗
25 s  ✗
        ↓
   LOST CONTACT
        ↓
     Gateway
        ↓
      MQTT
        ↓
 Rescue Dashboard
```

---

### Event-driven Priority

Event-driven priority to not waste battery and bandwidth by sending everything continuously

```
NORMAL       → periodic (5-10 sec) :[ temperature,humidity,gas,motion,battery ]
WARNING      → frequent : say, gas threshold crossed
CRITICAL     → immediate + repeated: [SOS, FALL, CRITICAL GAS etc.]
```

---

### Store-and-forward Method

when a worker temporarily loses the communication path.

```
Worker 3
   │
   │ SOS!
   ▼
No BLE route to gateway
   │
   ▼
Node stores the SOS locally
   │
   │ keeps retrying
   ▼
BLE route becomes available
   │
   ▼
Forward SOS → Gateway → MQTT → Rescue Dashboard
```

So the alert isn't lost just because connectivity was temporarily unavailable.

Heartbeat: tells the network "I'm still alive."

Store-and-forward: ensures an important message "doesn't disappear when communication is temporarily unavailable."
This is especially useful for your isolated-worker scenario.

For your project, I'd make SOS, fall and critical gas alerts persistent, while ordinary temperature/humidity telemetry doesn't necessarily need to be stored.
