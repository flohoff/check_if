class EntitySensor {
private:
	SNMP	*snmp;
	int	snmpinstance;

	int	entSensorScale;
	int	entSensorPrecision;
	int	entSensorStatus;
	int	entSensorValue;
	int	entSensorType;

	bool	isvalid;
public:
	EntitySensor(SNMP *snmp, int instance) : snmp(snmp),snmpinstance(instance),
		entSensorScale(0),entSensorPrecision(0),entSensorStatus(0),entSensorValue(0),entSensorType(0) {
		std::vector<std::string>	oidstrings;

		oidstrings.push_back("entSensorScale");
		oidstrings.push_back("entSensorPrecision");
		oidstrings.push_back("entSensorStatus");
		oidstrings.push_back("entSensorValue");
		oidstrings.push_back("entSensorType");

		for(auto &s : oidstrings) {
			s.append(".");
			s.append(std::to_string(snmpinstance));
		}

		SNMPResultList *resultlist=snmp->snmpget(oidstrings);
		isvalid=resultlist->valid();

		if (!isvalid)
			return;

		for(auto &r : *resultlist) {
			if (!r.valid())
				continue;

			if (r.sminame("entSensorScale")) { entSensorScale=r.smivalue_int(); continue; }
			if (r.sminame("entSensorPrecision")) { entSensorPrecision=r.smivalue_int(); continue; }
			if (r.sminame("entSensorStatus")) { entSensorStatus=r.smivalue_int(); continue; }
			if (r.sminame("entSensorValue")) { entSensorValue=r.smivalue_int(); continue; }
			if (r.sminame("entSensorType")) { entSensorType=r.smivalue_int(); continue; }
		}
	}

	double scalepow() const {
		switch(entSensorScale) {
			case(6): { return -9; };
			case(7): { return -6; };
			case(8): { return -3; };
			case(9): { return 0; };
		}
	}

	bool valid() const {
		return isvalid;
	}

	double value() const {
		return entSensorValue*pow(10,-entSensorPrecision);
	}

	std::string printable() const {
		std::ostringstream	msg;

		double value=this->value();
		msg << value;

		switch(entSensorScale) {
			case(6): { msg << "n"; break; }
			case(7): { msg << "µ"; break; }
			case(8): { msg << "m"; break; }
		}

		/* CISCO-ENTITY-SENSOR-MIB */
		switch(entSensorType) {
			case(4): { msg << 'V'; break; }
			case(5): { msg << 'A'; break; }
			case(6): {
				msg << 'W';
				msg << " " << (double) (log10(value/1)*10) << "dBm";
				break;
			}
			case(8): { msg << "°C"; break; }
			case(14): { msg << "dBm"; break; }
		}

		return msg.str();
	}

	double dBm() const {
		double value=this->value();

		/* We have a sensor returning Watt - convert it to dBm */
		if (entSensorType == 6) {
			return (log10(value/1)*10);
		}
		/* Sensor is in dBm */
		if (entSensorType == 14) {
			return value;
		}

		return 0;
	}

	friend std::ostream &operator<<(std::ostream &os, const EntitySensor &es) {

		os << "entSensorType=" << es.entSensorType << " ";
		os << "entSensorStatus=" << es.entSensorStatus << " ";
		os << "entSensorValue=" << es.entSensorValue << " ";
		os << "entSensorScale=" << es.entSensorScale << " ";
		os << "entSensorPrecision=" << es.entSensorPrecision << " ";

		double value=pow(10, (-es.entSensorScale))*es.entSensorValue;

		//os << std::fixed;
		os << "Value: " << es.printable() << " ";

		return os;
	}
};

