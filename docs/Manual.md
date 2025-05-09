Device Support for General Network Devices (netDev)
===================================================

Table of Contents
=================

* [Overview](#overview)
* [Yokogawa FA-M3 series PLCs ("<strong>Yew Plc</strong>")](#yokogawa-fa-m3-series-plcs-yew-plc)
   * [Configure CPU Properties](#configure-cpu-properties)
   * [Device Type (DTYP) Field](#device-type-dtyp-field)
   * [Input / Output Link (INP/OUT) Fields](#input--output-link-inpout-fields)
   * [Supported Record Types](#supported-record-types)
   * [Accessing Relays and Registers](#accessing-relays-and-registers)
   * [Accessing Special Modules](#accessing-special-modules)

# Overview

Device support for general network devices, netDev, is a device support for network based devices. Following devices are supported:
- Yokogawa FA-M3 series Programmable Logic Controllers ("**Yew Plc**")
- Yokogawa MW100 series Data Acquisition Unit ("**MW100**")
- Yokogawa DARWIN series Data Acquisition Unit ("**Darwin**")
- Keyence KV-1000/3000/... series Programmable Logic Controllers ("**Key Plc**")
- Chino KE3000 series Data Loggers ("**ChinoLogL**")


# Yokogawa FA-M3 series PLCs ("**Yew Plc**")

This device support utilizes not only commands and responses that are
described in **IM 34M06P41-01E** "Personal Computer Link Commands",
but also undocumented features.

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

## Device Type (DTYP) Field

In order to use netDev, device type (DTYP) field must be set to "**Yew Plc**" in the record:

`field(DTYP, "Yew Plc")`


## Input / Output Link (INP/OUT) Fields

General format for input (INP) and output (OUT) link fields are as following:

`field(INP, "@hostname[(port)][:cpu]#type[:]number[&option]")`

e.g.

`field(INP, "@192.168.1.1#I:00100&L")`

- hostname : hostname or IP address of the FA-M3 CPU (or Personal Computer Link Modules).
- port     : Optional port number (defaults to 0x3001).
- cpu      : Optional CPU number for multi-CPU configuration (defaults to 1).
- type     : PLC device such as Input relays and Data registers.
- number   : Device number.
- option   : Option to interpret registers as:
  * &U - unsigned integer (16-bit)
  * &L - long-word (32-bit) access
  * &B - treat as binary-coded-decimal (BCD)
  * &F - Single precision floating point (32-bit)
  * &D - Double precision floating point (64-bit)

Numbers other than IP address will be treated as decimals, or hexadecimals (when prefixed with 0x).

## Supported Record Types

Table below describes supported PLC devices along with the relevant record types:

| PLC&nbsp;device| Description                             | DTYP          | &nbsp;Data&nbsp;width&nbsp; | Supported record types                                                                  |
|----------------|-----------------------------------------|---------------|-----------------------------|-----------------------------------------------------------------------------------------|
| X              | Input relays on input modules           | Yew&nbsp;Plc  | 1-bit, 16-bit               | bi,     longin,          mbbiDirect,             mbbi,       ai                         |
| Y              | Output relays on output modules         | Yew&nbsp;Plc  | 1-bit, 16-bit               | bi, bo, longin, longout, mbbiDirect, mbboDirect, mbbi, mbbo, ai, ao                     |
| I              | Internal relays                         | Yew&nbsp;Plc  | 1-bit, 16-bit               | bi, bo, longin, longout  mbbiDirect, mbboDirect, mbbi, mbbo, ai, ao                     |
| E              | (Extended) Shared relays                | Yew&nbsp;Plc  | 1-bit, 16-bit               | bi, bo, longin, longout, mbbiDirect, mbboDirect, mbbi, mbbo, ai, ao                     |
| M              | Special relays                          | Yew&nbsp;Plc  | 1-bit                       | bi, bo                                                                                  |
| L              | Link relays (for FA Link and FL-net)    | Yew&nbsp;Plc  | 1-bit, 16-bit               | bi, bo, longin, longout, mbbiDirect, mbboDirect, mbbi, mbbo, ai, ao                     |
| D              | Data registers                          | Yew&nbsp;Plc  |        16-bit               |         longin, longout, mbbiDirect, mbboDirect, mbbi, mbbo, ai, ao, waveform, arrayout |
| B              | File registers                          | Yew&nbsp;Plc  |        16-bit               |         longin, longout, mbbiDirect, mbboDirect, mbbi, mbbo, ai, ao, waveform, arrayout |
| F              | Cache registers                         | Yew&nbsp;Plc  |        16-bit               |         longin, longout, mbbiDirect, mbboDirect, mbbi, mbbo, ai, ao, waveform, arrayout |
| R              | (Extended) Shared registers             | Yew&nbsp;Plc  |        16-bit               |         longin, longout, mbbiDirect, mbboDirect, mbbi, mbbo, ai, ao, waveform, arrayout |
| Z              | Special registers                       | Yew&nbsp;Plc  |        16-bit               |         longin, longout, mbbiDirect, mbboDirect, mbbi, mbbo, ai, ao                     |
| W              | Link registers (for FA Link and FL-net) | Yew&nbsp;Plc  |        16-bit               |         longin, longout, mbbiDirect, mbboDirect, mbbi, mbbo, ai, ao, waveform, arrayout |
| T              | Timer relays                            |               | 1-bit, 16-bit               |                                                                                         |
| C              | Counter relays                          |               | 1-bit, 16-bit               |                                                                                         |
| V              | Index registers                         |               |        16-bit               |                                                                                         |

**Note**: Special modules refer only to those modules accessed through
READ/WRITE instruction of the sequence CPU modules, e.g. analog
input/output, temperature control, PID control, high-speed counter,
etc.

The table below shows supported record types with DTYP fields used to
access specific PLC devices.

| Record&nbsp;type | DTYP          | &nbsp;&nbsp;Supported&nbsp;PLC&nbsp;device&nbsp;&nbsp; |                                                 |
|------------------|---------------|------------------------------------|-------------------------------------------------|
| bi               | Yew&nbsp;Plc  | X, Y, I, E, M, L                   |                                                 |
| bo               | Yew&nbsp;Plc  |    Y, I, E, M, L                   |                                                 |
| longin           | Yew&nbsp;Plc  | X, Y, I, E,    L, D, B, F, R, Z, W | Supported options: U, L, B                      |
| longout          | Yew&nbsp;Plc  |    Y, I, E,    L, D, B, F, R, Z, W | Supported options: U, L, B                      |
| mbbiDirect       | Yew&nbsp;Plc  | X, Y, I, E,    L, D, B, F, R, Z, W | Supported options: U, L                         |
| mbboDirect       | Yew&nbsp;Plc  |    Y, I, E,    L, D, B, F, R, Z, W | Supported options: U, L                         |
| mbbi             | Yew&nbsp;Plc  | X, Y, I, E,    L, D, B, F, R, Z, W | Supported options: U, L                         |
| mbbo             | Yew&nbsp;Plc  |    Y, I, E,    L, D, B, F, R, Z, W | Supported options: U, L                         |
| ai               | Yew&nbsp;Plc  | X, Y, I, R,       D, B, F,    Z, W | Supported options: U, L,    F, D                |
| ao               | Yew&nbsp;Plc  |    Y, I, R,       D, B, F,    Z, W | Supported options: U, L,    F, D                |
| waveform         | Yew&nbsp;Plc  | X, Y, I, R,       D, B, F,       W | Supported options: U, L,    F, D <br> Supported FTVL fields: DBF\_SHORT, DBF\_USHORT, DBF\_LONG, DBF\_ULONG, DBF\_FLOAT, DBF\_DOUBLE |
| arrayout         | Yew&nbsp;Plc  |    Y, I, R,       D, B, F,       W | Supported options: U, L,    F, D <br> Supported FTVL fields: DBF\_SHORT, DBF\_USHORT, DBF\_LONG, DBF\_ULONG, DBF\_FLOAT, DBF\_DOUBLE |

## Accessing Relays and Registers

This section illustrates some examples to read/write standard PLC devices (i.e., X, Y, I, E, M, L, D, B, F, R, Z, and W).

**Note**: Colon (:) between type and number is optional.


- Reading an input relay on slot 2, channel 1:
```
record(bi, "YEW:SLOT2:X01") {
    field(DTYP, "Yew Plc")
    field(INP,  "@192.168.1.1#X201")
    field(SCAN, "1 second")
}
```

- Write value to data register D00001:
```
record(longout, "YEW:INTERNAL:D00001") {
    field(DTYP, "Yew Plc")
    field(OUT,  "@192.168.1.1#D00001")
}
```

- Read BCD-coded second as decimal value:
```
record(longin, "YEW:INTERNAL:CLOCK_SEC")
{
    field(DTYP, "Yew Plc")
    field(INP,  "@192.168.1.1#Z054&B")
    field(SCAN, ".2 second")
}
```

## Accessing Special Modules

In order to access registers on special modules, specify `slot number` or `module-unit number,slot number` as **type** in the [Input / Output Link (INP/OUT) Fields](#input--output-link-inpout-fields).
The optional module-unit number defaults to 0.

**Note**: Colon (:) between type and number is **mandatory**.

- Read channel 1 from ADC module on slot 5:
```
record(longin, "YEW:SLOT5:CH1")
{
    field(DTYP, "Yew Plc")
    field(INP,  "@192.168.1.1#5:01")
    field(SCAN, "1 second")
}
```
