
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


Tranceiver Monitoring
=====================

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


