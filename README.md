
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


