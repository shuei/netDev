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
   * [Conversion Specifier](#conversion-specifier)

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

`field(INP, "@hostname[(port)][:cpu]#type[:]number[&conversion]")`

e.g.

`field(INP, "@192.168.1.1#I00100&L")`

- hostname   : hostname or IP address of the FA-M3 CPU (or Personal Computer Link Modules).
- port       : Optional port number (defaults to 0x3001).
- cpu        : Optional CPU number for multi-CPU configuration (defaults to 1).
- type       : PLC device such as Input relays and Data registers.
- number     : Device number.
- conversion : Conversion specifier to interpret the byte sequence in the registers (or relays):
  * <no specifier> - Treat 16-bit data as signed 16-bit integer
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

| Record&nbsp;type | DTYP          | &nbsp;&nbsp;Supported&nbsp;PLC&nbsp;device&nbsp;&nbsp; |                            |
|------------------|---------------|------------------------------------|------------------------------------------------|
| bi               | Yew&nbsp;Plc  | X, Y, I, E, M, L                   |                                                |
| bo               | Yew&nbsp;Plc  |    Y, I, E, M, L                   |                                                |
| longin           | Yew&nbsp;Plc  | X, Y, I, E,    L, D, B, F, R, Z, W | Supported conversion specifiers: U, L, B       |
| longout          | Yew&nbsp;Plc  |    Y, I, E,    L, D, B, F, R, Z, W | Supported conversion specifiers: U, L, B       |
| mbbiDirect       | Yew&nbsp;Plc  | X, Y, I, E,    L, D, B, F, R, Z, W | Supported conversion specifiers: U, L          |
| mbboDirect       | Yew&nbsp;Plc  |    Y, I, E,    L, D, B, F, R, Z, W | Supported conversion specifiers: U, L          |
| mbbi             | Yew&nbsp;Plc  | X, Y, I, E,    L, D, B, F, R, Z, W | Supported conversion specifiers: U, L          |
| mbbo             | Yew&nbsp;Plc  |    Y, I, E,    L, D, B, F, R, Z, W | Supported conversion specifiers: U, L          |
| ai               | Yew&nbsp;Plc  | X, Y, I, E,       D, B, F, R, Z, W | Supported conversion specifiers: U, L,    F, D |
| ao               | Yew&nbsp;Plc  |    Y, I, E,       D, B, F, R, Z, W | Supported conversion specifiers: U, L,    F, D |
| waveform         | Yew&nbsp;Plc  | X, Y, I, E,       D, B, F, R,    W | Supported conversion specifiers: U, L,    F, D <br> Supported FTVL fields: DBF\_SHORT, DBF\_USHORT, DBF\_LONG, DBF\_ULONG, DBF\_FLOAT, DBF\_DOUBLE |
| arrayout         | Yew&nbsp;Plc  |    Y, I, E,       D, B, F, R,    W | Supported conversion specifiers: U, L,    F, D <br> Supported FTVL fields: DBF\_SHORT, DBF\_USHORT, DBF\_LONG, DBF\_ULONG, DBF\_FLOAT, DBF\_DOUBLE |

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

## Conversion Specifier

The conversion option in the INP/OUT field modifies interpretation of the byte sequence in the register (or relays) as follows:
 * <no option> - Treat 16-bit data as signed 16-bit integer
 * &U - Treat 16-bit data as an unsigned 16-bit integer
 * &L - Access the device with 32-bit width and treat as 32-bit signed integer
 * &B - Treat 16-bit data as binary-coded-decimal (BCD)
 * &F - Access the device with 32-bit width and treat the data as single precision floating point
 * &D - Access the device with 64-bit width and treat the data as double precision floating point

The conversion behaves like integer promotions in C language.
Examples of conversion specifiers in input records are given in the table below:

| record type                 | INP                  | D0002  | D0001  | get value    |
|-----------------------------|----------------------|--------|--------|--------------|
| longin                      | @192.168.1.1#D0001   | -      | 0xffff | -1           |
| longin                      | @192.168.1.1#D0001&U | -      | 0xffff |  65535       |
| longin                      | @192.168.1.1#D0001&L | 0x0000 | 0xffff |  65535       |
| longin                      | @192.168.1.1#D0001&B | -      | 0x1234 |  1234        |
| waveform(FTVL = DBF_SHORT)  | @192.168.1.1#D0001   | -      | 0xffff | -1           |
| waveform(FTVL = DBF_SHORT)  | @192.168.1.1#D0001&U | -      | 0xffff | -1           |
| waveform(FTVL = DBF_USHORT) | @192.168.1.1#D0001   | -      | 0xffff |  65535       |
| waveform(FTVL = DBF_USHORT) | @192.168.1.1#D0001&U | -      | 0xffff |  65535       |
| waveform(FTVL = DBF_LONG)   | @192.168.1.1#D0001   | -      | 0xffff | -1           |
| waveform(FTVL = DBF_LONG)   | @192.168.1.1#D0001&U | -      | 0xffff |  65535       |
| waveform(FTVL = DBF_LONG)   | @192.168.1.1#D0001&L | 0x0000 | 0xffff |  65535       |
| waveform(FTVL = DBF_LONG)   | @192.168.1.1#D0001&L | 0xffff | 0xffff | -1           |
| waveform(FTVL = DBF_ULONG)  | @192.168.1.1#D0001   | -      | 0xffff |  4294967295  |
| waveform(FTVL = DBF_ULONG)  | @192.168.1.1#D0001&U | -      | 0xffff |  65535       |
| waveform(FTVL = DBF_ULONG)  | @192.168.1.1#D0001&L | 0x0000 | 0xffff |  65535       |
| waveform(FTVL = DBF_ULONG)  | @192.168.1.1#D0001&L | 0xffff | 0xffff |  4294967295  |

If a value that cannot be expressed as a signed or unsigned 16-bit integer is set to a 32-bit integer output record (i.e. a longout or an arrayout with FTVL=LONG/ULONG), the uppser 16-bit are silently ignored and the lower 16-bits only are written to the hardware.
Examples of conversion specifiers in output records are given in the table below:

| record type                 | OUT                  | put value    | D0002  | D0001  |
| longout                     | @192.168.1.1#D0001   | -1           | -      | 0xffff |
| longout                     | @192.168.1.1#D0001   |  65535       | -      | 0xffff |
| longout                     | @192.168.1.1#D0001   |  65537       | -      | 0x0001 |
| longout                     | @192.168.1.1#D0001&U | -1           | -      | 0xffff |
| longout                     | @192.168.1.1#D0001&U |  65535       | -      | 0xffff |
| longout                     | @192.168.1.1#D0001&U |  65537       | -      | 0x0001 |
| longout                     | @192.168.1.1#D0001&L | -1           | 0xffff | 0xffff |
| longout                     | @192.168.1.1#D0001&L |  65535       | 0x0000 | 0xffff |
| longout                     | @192.168.1.1#D0001&L |  65537       | 0x0001 | 0x0001 |
| longout                     | @192.168.1.1#D0001&B |  1234        | -      | 0x1234 |
| waveform(FTVL = DBF_SHORT)  | @192.168.1.1#D0001   | -1           | -      | 0xffff |
| waveform(FTVL = DBF_SHORT)  | @192.168.1.1#D0001&U | -1           | -      | 0xffff |
| waveform(FTVL = DBF_USHORT) | @192.168.1.1#D0001   | -1           | -      | 0xffff |
| waveform(FTVL = DBF_USHORT) | @192.168.1.1#D0001   |  65535       | -      | 0xffff |
| waveform(FTVL = DBF_USHORT) | @192.168.1.1#D0001&U | -1           | -      | 0xffff |
| waveform(FTVL = DBF_USHORT) | @192.168.1.1#D0001&U |  65535       | -      | 0xffff |
| waveform(FTVL = DBF_LONG)   | @192.168.1.1#D0001   | -1           | -      | 0xffff |
| waveform(FTVL = DBF_LONG)   | @192.168.1.1#D0001   |  65535       | -      | 0xffff |
| waveform(FTVL = DBF_LONG)   | @192.168.1.1#D0001   |  65537       | -      | 0x0001 |
| waveform(FTVL = DBF_LONG)   | @192.168.1.1#D0001&U | -1           | -      | 0xffff |
| waveform(FTVL = DBF_LONG)   | @192.168.1.1#D0001&U |  65535       | -      | 0xffff |
| waveform(FTVL = DBF_LONG)   | @192.168.1.1#D0001&U |  65537       | -      | 0x0001 |
| waveform(FTVL = DBF_LONG)   | @192.168.1.1#D0001&L | -1           | 0xffff | 0xffff |
| waveform(FTVL = DBF_LONG)   | @192.168.1.1#D0001&L |  65535       | 0x0000 | 0xffff |
| waveform(FTVL = DBF_ULONG)  | @192.168.1.1#D0001   | -1           | -      | 0xffff |
| waveform(FTVL = DBF_ULONG)  | @192.168.1.1#D0001   |  65535       | -      | 0xffff |
| waveform(FTVL = DBF_ULONG)  | @192.168.1.1#D0001   |  65537       | -      | 0x0001 |
| waveform(FTVL = DBF_ULONG)  | @192.168.1.1#D0001&U | -1           | -      | 0xffff |
| waveform(FTVL = DBF_ULONG)  | @192.168.1.1#D0001&U |  65535       | -      | 0xffff |
| waveform(FTVL = DBF_ULONG)  | @192.168.1.1#D0001&U |  65537       | -      | 0x0001 |
| waveform(FTVL = DBF_ULONG)  | @192.168.1.1#D0001&L | -1           | 0xffff | 0xffff |
| waveform(FTVL = DBF_ULONG)  | @192.168.1.1#D0001&L |  65535       | 0x0000 | 0xffff |
| waveform(FTVL = DBF_ULONG)  | @192.168.1.1#D0001&L |  65537       | 0x0001 | 0x0001 |
| waveform(FTVL = DBF_ULONG)  | @192.168.1.1#D0001&L |  4294967295  | 0xffff | 0xffff |
