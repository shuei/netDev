Device Support for General Network Devices (netDev)
===================================================

# Overview

# Device Types

- "**Yew Plc**"
- "**MW100**"
- "**Darwin**"
- "**Key Plc**"
- "**ChinoLogL**"

# Yokogawa FA-M3 series PLCs ("**Yew Plc**")

This device support utilises commands and responses that are described
in **IM 34M06P41-01E** "Personal Computer Link Commands, as well as
undocumented features.

## Configure CPU Properties

Make sure that FA-M3 CPU is properly configured using WideField, in CPU Properties edit window.
- Ethernet Configurations in "ETHERNET" pane.
- Higher-level link commands in "HIGHER-LEVEL_LINK_SERVICE" pane. This device support employs UDP / binary data format to communicate with the CPU. Note that ASCII mode is **not** supported.
  - HLINK_PROTOCOL_A    : 1 (=UDP)
  - HLINK_DATA_FORMAT_A : 1 (=binary)
  - HLINK_PROTOCOL_B    : 1 (=UDP)
  - HLINK_DATA_FORMAT_B : 1 (=binary)
  - HLINK_PROTEC        : 0 (=write permitted)

For details, refer to **IM 34M06Q16-03** "FA-M3 Programming Tool WideField3 (Online)", H5 "CPU Properties".

## Device Type Field (DTYP)

In order to use netDev, device type (DTYP) field must be set to "**Yew Plc**" in the record:

`field(DTYP, "Yew Plc")`


## Input / Output Link Fields (INP/OUT)

General format for input (INP) and output (OUT) link fields are as following:

`field(INP, "@hostname[(port)][:unit]#type[:]number[&option]")`
e.g.
`field(INP, "@192.168.1.1#I:00100&L")`

- hostname : hostname or IP address of the FA-M3 CPU (or Personal Computer Link Modules).
- port     : Optional port number (defaults to 0x3001).
- unit     : Optional Unit number which identifies CPU in case of multi-CPU configuration (defaults to 1).
- type     : PLC device such as Input relays and Data registers.
- number   : Device number.
- option   : Option to interpret registers as:
  * &U - unsigned integer (16-bit)
  * &L - long-word (32-bit) access
  * &B - treat as binary-coded-decimal (BCD)
  * &F - Single precision floating point (32-bit)
  * &D - Double precision floating point (64-bit)

Numbers other than IP address will be treated as decimals, or hexadecimals (when prefixed with 0x).
Colon between type and number is optional.

## Supported Record Types

Table below describes supported PLC devices along with the relevant record types:

| **PLC device** | **Description**                         | **DTYP** | **data width** | **Supported record types**                                                  |
|----------------|-----------------------------------------|----------|----------------|-----------------------------------------------------------------------------|
| X              | Input relays on input modules           | Yew Plc  | 1-bit, 16-bit  | bi,     longin,          mbbiDirect,             ai                         |
| Y              | Output relays on output modules         | Yew Plc  | 1-bit, 16-bit  | bi, bo, longin, longout, mbbiDirect, mbboDirect, ai, ao                     |
| I              | Internal relays                         | Yew Plc  | 1-bit, 16-bit  | bi, bo, longin, longout                                                     |
| E              | (Extended) Shared relays                | Yew Plc  | 1-bit, 16-bit  | bi, bo, longin, longout, mbbiDirect, mbboDirect                             |
| M              | Special relays                          | Yew Plc  | 1-bit          | bi, bo                                                                      |
| L              | Link relays (for FA Link and FL-net)    | Yew Plc  | 1-bit, 16-bit  | bi, bo, longin, longout, mbbiDirect, mbboDirect                             |
| D              | Data registers                          | Yew Plc  | 16-bit         |         longin, longout, mbbiDirect, mbboDirect, ai, ao, waveform, arrayout |
| B              | File registers                          | Yew Plc  | 16-bit         |         longin, longout, mbbiDirect, mbboDirect, ai, ao, waveform, arrayout |
| F              | Cache registers                         | Yew Plc  | 16-bit         |         longin, longout, mbbiDirect, mbboDirect, ai, ao, waveform, arrayout |
| R              | (Extended) Shared registers             | Yew Plc  | 16-bit         |         longin, longout, mbbiDirect, mbboDirect, ai, ao, waveform, arrayout |
| Z              | Special registers                       | Yew Plc  | 16-bit         |         longin, longout, mbbiDirect, mbboDirect, ai, ao                     |
| W              | Link registers (for FA Link and FL-net) | Yew Plc  | 16-bit         |         longin, longout, mbbiDirect, mbboDirect, ai, ao, waveform, arrayout |
| A              | Registers on special modules            | Yew Plc  | 16-bit         |         longin, longout, mbbiDirect, mbboDirect, ai, ao, waveform, arrayout |
| T              | Timers                                  |          | 1-bit, 16-bit  |                                                                             |
| C              | Counters                                |          | 1-bit, 16-bit  |                                                                             |
| V              | Index registers                         |          | 16-bit         |                                                                             |

The table below shows supported record types with DTYP fields used to
access specific devices.

| **Record type** | **DTYP** | **Supported PLC device**              |                                                     |
|-----------------|----------|---------------------------------------|-----------------------------------------------------|
| bi              | Yew Plc  | X, Y, I, E, M, L                      |                                                     |
| bo              | Yew Plc  |    Y, I, E, M, L                      |                                                     |
| longin          | Yew Plc  | X, Y, I, E,    L, D, B, F, R, Z, W, A | **Supported options**: U, L, B                      |
| longout         | Yew Plc  |    Y, I, E,    L, D, B, F, R, Z, W, A | **Supported options**: U, L, B                      |
| mbbiDirect      | Yew Plc  | X, Y,    E,    L, D, B, F, R, Z, W, A |                                                     |
| mbboDirect      | Yew Plc  |    Y,    E,    L, D, B, F, R, Z, W, A |                                                     |
| ai              | Yew Plc  | X, Y,    R,       D, B, F,    Z, W, A | **Supported options**: U, L, F, D                   |
| ao              | Yew Plc  |    Y,    R,       D, B, F,    Z, W, A | **Supported options**: U, L, F, D                   |
| waveform        | Yew Plc  |          R,       D, B, F,       W, A | **FTVL field**: DBF\_ULONG, DBF\_USHORT, DBF\_SHORT |
| arrayout        | Yew Plc  |          R,       D, B, F,       W, A | **FTVL field**: DBF\_ULONG, DBF\_USHORT, DBF\_SHORT |
