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

Make sure that FA-M3 CPU is properly configured using WideField, in CPU Properties edit window:

- Ethernet Configurations in "ETHERNET" pane.
- Higher-level link commands in "HIGHER-LEVEL_LINK_SERVICE" pane. This device support employs UDP / binary data format to communicate with the CPU. Note that ASCII mode is **not** supported.
  - HLINK_PROTOCOL_A    : 1 (=UDP)
  - HLINK_DATA_FORMAT_A : 1 (=binary)
  - HLINK_PROTOCOL_B    : 1 (=UDP)
  - HLINK_DATA_FORMAT_B : 1 (=binary)
  - HLINK_PROTEC        : 0 (=write permitted)

For details, refer to **IM 34M06Q16-03** "FA-M3 Programming Tool WideField3 (Online)", H5 "CPU Properties".

## Device Type

In order to use netDev, device type (DTYP) field must be set to "**Yew Plc**" in the record.

## INP/OUT Fields

General format for input/output link field is as following:

@hostname[(port)][:unit]#type[:]number[&option]

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

| **Device** | **Description**                         | **DTYP** | **data width** | **Supported record types**                                                    |
|------------|-----------------------------------------|----------|----------------|-------------------------------------------------------------------------------|
| X          | Input relays on input module            | Yew Plc  | 1-bit, 16-bit  | mbbiDirect, longin, bi, ai                                              |
| Y          | Output relays on output module          | Yew Plc  | 1-bit, 16-bit  | mbbiDirect, mbboDirect, longin, longout, bi, bo, ai, ao           |
| R          | (Extended) Shared registers             | Yew Plc  | 16-bit         | mbbiDirect, mbboDirect, longin, longout, ai, ao, waveform, arrayout         |
| E          | (Extended) Shared relays                | Yew Plc  | 1-bit, 16-bit  | mbbiDirect, mbboDirect, bi, bo, longin, longout                   |
| L          | Link relays (for FA Link and FL-net)    | Yew Plc  | 1-bit, 16-bit  | mbbiDirect, mbboDirect, bi, bo, longin, longout                   |
| W          | Link registers (for FA Link and FL-net) | Yew Plc  | 16-bit         | mbbiDirect, mbboDirect, longin, longout, ai, ao, waveform, arrayout         |
| D          | Data registers                          | Yew Plc  | 16-bit         | mbbiDirect, mbboDirect, longin, longout, ai, ao, waveform, arrayout         |
| B          | File registers                          | Yew Plc  | 16-bit         | mbbiDirect, mbboDirect, longin, longout, ai, ao, waveform, arrayout         |
| F          | Cache registers                         | Yew Plc  | 16-bit         | mbbiDirect, mbboDirect, longin, longout, ai, ao, waveform, arrayout         |
| Z          | Special registers                       | Yew Plc  | 16-bit         | mbbiDirect, mbboDirect, longin, longout, ai, ao                   |
| I          | Internal relays                         | Yew Plc  | 1-bit, 16-bit  | bi, bo, longin, longout                                                       |
| M          | Special relays                          | Yew Plc  | 1-bit          | bi, bo                                                                        |
| V          | Index registers                         |          | 16-bit         |                                                                               |
| T          | Timers                                  |          | 1-bit, 16-bit  |                                                                               |
| C          | Counters                                |          | 1-bit, 16-bit  |                                                                               |

The table below shows supported record types with DTYP fields used to
access specific devices.

| **Record type** | **DTYP** | **Supported device**             |                                                      |
|-----------------|----------|----------------------------------|------------------------------------------------------|
| bi              | Yew Plc  | X, Y, E, L, I, M                 |                                                      |
| bo              | Yew Plc  |    Y, E, L, I, M                 |                                                      |
| longin          | Yew Plc  | X, Y, R, E, L, W, D, B, F, Z, I  | **Supported options**: U, L, B                       |
| longout         | Yew Plc  |    Y, R, E, L, W, D, B, F, Z, I  | **Supported options**: U, L, B                       |
| mbbiDirect      | Yew Plc  | X, Y, R, E, L, W, D, B, F, Z     |                                                      |
| mbboDirect      | Yew Plc  |    Y, R, E, L, W, D, B, F, Z     |                                                      |
| ai              | Yew Plc  | X, Y, R,       W, D, B, F, Z     | **Supported options**: U, L, F, D                    |
| ao              | Yew Plc  |    Y, R,       W, D, B, F, Z     | **Supported options**: U, L, F, D                    |
| waveform        | Yew Plc  |       R,       W, D, B, F        | **FTVL field**: DBF\_ULONG, DBF\_USHORT, DBF\_SHORT  |
| arrayout        | Yew Plc  |       R,       W, D, B, F        | **FTVL field**: DBF\_ULONG, DBF\_USHORT, DBF\_SHORT  |

