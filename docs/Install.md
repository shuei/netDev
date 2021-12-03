Installing Device Support for General Network Devices (netDev)
==============================================================

Table of Contents
=================
* [Introduction](#introduction)
* [Prerequisite](#prerequisite)
* [Building netDev](#building-netdev)
   * [Extracting distribution file](#extracting-distribution-file)
   * [Edit site-specific build configuration](#edit-site-specific-build-configuration)
   * [Build](#build)
* [Using the Device Support with IOC Application](#using-the-device-support-with-ioc-application)
   * [RELEASE file](#release-file)
   * [src/Makefile](#srcmakefile)

# Introduction

This document describes instructions for installing the device support
for general network devices (netDev).

# Prerequisite

This device sipport works with EPICS base 3.14.12 on, tested up to R3.15.8.

# Building netDev

## Extracting distribution file

Untar the tabell, e.g., `netDev-1.0.7.tar.gz`, to an appropriate direcory, e.g., `${EPICS_BASE}/../modules/src`:
```shell

mkdir -p ${EPICS_BASE}/../modules/src
tar -C ${EPICS_BASE}/../modules/src -x -f netDev-1.0.7.tar.gz
```

Go to the top-level directory of the device support:
```shell
cd ${EPICS_BASE}/../modules/src/netDev-1.0.7
```
## Edit site-specific build configuration

Edit `${EPICS_BASE}/../modules/src/netDev-1.0.7/configure/RELEASE` (or `RELESE.local`)
so that `EPICS_BASE` variable points your `$EPICS_BASE` correctly:

```makefile
EPICS_BASE=/path/to/epics/base
```

## Build

Build the device support with:
```shell
cd ${EPICS_BASE}/../modules/src/netDev-1.0.7
make
```

# Using the Device Support with IOC Application

This section explains how to use netDev in your IOC application.

## RELEASE file

Edit `/path/to/your_ioc/configure/RELEASE` (or `RELEASE.local`) and add definition for `netDev`:

```makefile
NETDEV = ${EPICS_BASE}/../modules/src/netDev-1.0.7
```

## src/Makefile

Edit `/path/to/your_ioc/<appname>App/src/Makefile`:
```makefile
...
<appname>_DBD += netDev.dbd
...
<appname>_LIBS += netDev
```
