
Usage
=====

	Need address, community and ifname
	Allowed options:
	  -h [ --help ]                     produce help message
	  --address arg                     host address
	  --community arg                   host snmp v2 community
	  --ifname arg                      interface name to monitor
	  --cachedir arg                    cache directory for state files
	  --nolinkstatus                    ifOperStatus down is not critical
	  --ifindiscardsignore arg (=0.1%)  ifInDiscards ignore values
	  --ifoutdiscardsignore arg (=0.1%) ifOutDiscards ignore values
	  --ifinerrorsignore arg (=0.1%)    ifInErrors ignore values
	  --ifouterrorsignore arg (=0.1%)   ifOutErrors ignore values

Limits for in/outerrors or in/outdiscards can be either in percent of the input/output octets or time based:

* 0.1% would be 1 per thousand packets may be broken
* 1s would be 1 packet per Seconds allowed to be broken

The interfaces link will be monitored and be CRITICAL on link down. 

If the interface supports IPv6 Routing for example the Cisco 9001 the ipv6IfAdmin and ipv6OperStatus will be monitored. There are
situations where ipv6 will be operational down although the interface is ipv6 configured e.g. after a DAD Event (Duplicate address detection).


Transceiver Monitoring
======================

*check_if* will also monitor Transceiver healthiness and optical levels if available.

Cisco IOS / IOS-XR
------------------

For Cisco IOS and IOS-XR the "ENTITY-MIB" will be queried for the exported transceiver sensors. Optical TX and RX levels will
be also exported as performance metrics. This works for most platforms e.g. Catalyst 2960S, or IOS-XR based Cisco 9001.

	ENTITY-MIB::entPhysicalName.1038 = STRING: Te1/0/1 Module Temperature Sensor
	ENTITY-MIB::entPhysicalName.1039 = STRING: Te1/0/1 Supply Voltage Sensor
	ENTITY-MIB::entPhysicalName.1040 = STRING: Te1/0/1 Bias Current Sensor
	ENTITY-MIB::entPhysicalName.1041 = STRING: Te1/0/1 Transmit Power Sensor
	ENTITY-MIB::entPhysicalName.1042 = STRING: Te1/0/1 Receive Power Sensor

ArubaOS
-------

ArubaOS exports Transceiver informations in the HP-ICF-TRANSCEIVER-MIB.

	hpicfXcvrRxPower
	hpicfXcvrTxPower
	hpicfXcvrTemp
	hpicfXcvrVoltage
	hpicfXcvrErrors
	hpicfXcvrAlarms

Will be queried. ArubaOS-CX is not supported. I could not find any place the Transceiver informations are beeing exposed in SNMP.

Building check_if
=================

	apt-get install build-essential cmake cmake-data libboost-all-dev protobuf-compiler libprotobuf-dev pkg-config 

	mkdir build
	cd build
	cmake ..
	make


Building snmp++
===============

    apt-get install build-essential libssl-dev libtoolize autoconf pkg-config
    wget https://www.agentpp.com/download/snmp++-3.4.10.tar.gz
    tar -zxvf snmp++-3.4.10.tar.gz
    cd snmp++-3.4.10
    ./configure
    make 
    make install



Todo
====

Reboot detection
----------------
Detect reboot by trying to get sysUptime and monitor for wrap/decrease. If so remove
file and restart discovery. If counter wrap is to unreliable.

Mac address count
-----------------

We might want to check for mac address count on L3 enabled devices e.g.
VLAN interfaces or routed interfaces.

Link member status
------------------

In case we monitor a LAG/LACP/Etherchannel we might want to check if
links = active links and if negotiation worked. Whatever switches might
expose by SNMP.

Transceiver Status Dell DNOS
----------------------------

	DNOS-BOXSERVICES-PRIVATE-MIB::boxServicesFiberPortIndex.49 = Gauge32: 49
	DNOS-BOXSERVICES-PRIVATE-MIB::boxServicesFiberPortIndex.50 = Gauge32: 50
	DNOS-BOXSERVICES-PRIVATE-MIB::boxServicesFiberPortOpticsTemperature.49 = INTEGER: 18.5 DEGREES
	DNOS-BOXSERVICES-PRIVATE-MIB::boxServicesFiberPortOpticsTemperature.50 = INTEGER: 18.2 DEGREES
	DNOS-BOXSERVICES-PRIVATE-MIB::boxServicesFiberPortOpticsVoltage.49 = INTEGER: 3.246 Volts
	DNOS-BOXSERVICES-PRIVATE-MIB::boxServicesFiberPortOpticsVoltage.50 = INTEGER: 3.241 Volts
	DNOS-BOXSERVICES-PRIVATE-MIB::boxServicesFiberPortOpticsCurrent.49 = INTEGER: 5.7 Milliamps
	DNOS-BOXSERVICES-PRIVATE-MIB::boxServicesFiberPortOpticsCurrent.50 = INTEGER: 5.6 Milliamps
	DNOS-BOXSERVICES-PRIVATE-MIB::boxServicesFiberPortOpticsPowerOut.49 = INTEGER: -2.994 dBm
	DNOS-BOXSERVICES-PRIVATE-MIB::boxServicesFiberPortOpticsPowerOut.50 = INTEGER: -3.035 dBm
	DNOS-BOXSERVICES-PRIVATE-MIB::boxServicesFiberPortOpticsPowerIn.49 = INTEGER: -40.000 dBm
	DNOS-BOXSERVICES-PRIVATE-MIB::boxServicesFiberPortOpticsPowerIn.50 = INTEGER: -40.000 dBm
	DNOS-BOXSERVICES-PRIVATE-MIB::boxServicesFiberPortOpticsTxFault.49 = INTEGER: false(2)
	DNOS-BOXSERVICES-PRIVATE-MIB::boxServicesFiberPortOpticsTxFault.50 = INTEGER: false(2)
	DNOS-BOXSERVICES-PRIVATE-MIB::boxServicesFiberPortOpticsLos.49 = INTEGER: true(1)
	DNOS-BOXSERVICES-PRIVATE-MIB::boxServicesFiberPortOpticsLos.50 = INTEGER: true(1)
	DNOS-BOXSERVICES-PRIVATE-MIB::boxServicesFiberPortOpticsFaultStatus.49 = STRING: "Local Fault"
	DNOS-BOXSERVICES-PRIVATE-MIB::boxServicesFiberPortOpticsFaultStatus.50 = STRING: "Local Fault"
	DNOS-BOXSERVICES-PRIVATE-MIB::boxServicesFiberPortsOpticsInfoPortIndex.49 = Gauge32: 49
	DNOS-BOXSERVICES-PRIVATE-MIB::boxServicesFiberPortsOpticsInfoPortIndex.50 = Gauge32: 50
	DNOS-BOXSERVICES-PRIVATE-MIB::boxServicesFiberPortsOpticsInfoVendorName.49 = STRING: "FLEXOPTIX       "
	DNOS-BOXSERVICES-PRIVATE-MIB::boxServicesFiberPortsOpticsInfoVendorName.50 = STRING: "FLEXOPTIX       "
	DNOS-BOXSERVICES-PRIVATE-MIB::boxServicesFiberPortsOpticsInfoLinkLength50um.49 = Gauge32: 8
	DNOS-BOXSERVICES-PRIVATE-MIB::boxServicesFiberPortsOpticsInfoLinkLength50um.50 = Gauge32: 8
	DNOS-BOXSERVICES-PRIVATE-MIB::boxServicesFiberPortsOpticsInfoLinkLength62dot5um.49 = Gauge32: 3
	DNOS-BOXSERVICES-PRIVATE-MIB::boxServicesFiberPortsOpticsInfoLinkLength62dot5um.50 = Gauge32: 3
	DNOS-BOXSERVICES-PRIVATE-MIB::boxServicesFiberPortsOpticsInfoSerialNumber.49 = STRING: "FOO1JV1         "
	DNOS-BOXSERVICES-PRIVATE-MIB::boxServicesFiberPortsOpticsInfoSerialNumber.50 = STRING: "FOO1JV2         "
	DNOS-BOXSERVICES-PRIVATE-MIB::boxServicesFiberPortsOpticsInfoPartNumber.49 = STRING: "P.8596.02       "
	DNOS-BOXSERVICES-PRIVATE-MIB::boxServicesFiberPortsOpticsInfoPartNumber.50 = STRING: "P.8596.02       "
	DNOS-BOXSERVICES-PRIVATE-MIB::boxServicesFiberPortsOpticsInfoNominalBitRate.49 = Gauge32: 10300
	DNOS-BOXSERVICES-PRIVATE-MIB::boxServicesFiberPortsOpticsInfoNominalBitRate.50 = Gauge32: 10300
	DNOS-BOXSERVICES-PRIVATE-MIB::boxServicesFiberPortsOpticsInfoRevision.49 = STRING: "1.0 "
	DNOS-BOXSERVICES-PRIVATE-MIB::boxServicesFiberPortsOpticsInfoRevision.50 = STRING: "1.0 "
	DNOS-BOXSERVICES-PRIVATE-MIB::boxServicesFiberPortsOpticsInfoCompliance.49 = STRING: "10GBase-SR"
	DNOS-BOXSERVICES-PRIVATE-MIB::boxServicesFiberPortsOpticsInfoCompliance.50 = STRING: "10GBase-SR"

