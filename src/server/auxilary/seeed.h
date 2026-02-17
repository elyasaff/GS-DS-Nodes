#ifndef SEEED_H_
#define SEEED_H_
#include "modbus.h"

#define PH_ADDRESS 0x01
const byte _seeed_ad[6] = {PH_ADDRESS, 0x03, 0x00, 0x02, 0x00, 0x01};
void seeed_calib(const char * i, const byte a_read[6], byte a_write[6]) {
	int16_t t;
	if (!_modbus_int16(a_read,6,&t))
		return;
	Serial.print("calib ");
	Serial.print(i);
	Serial.print(": ad was ");
	Serial.print(t);
	if (!_modbus_int16(_seeed_ad,6,&t))
		return;
	Serial.print(" and now is ");
	Serial.print(t);
	a_write[4] = t>>8;
	a_write[5] = t&0x00FF;
	if (!modbus_send_recv(a_write,6))
		return;
	Serial.println(", calib completed");
}

const byte _seeed_calib_4_read[6] = {PH_ADDRESS, 0x03, 0x00, 0x30, 0x00, 0x01};
byte _seeed_calib_4_write[6] = {PH_ADDRESS, 0x06, 0x00, 0x30, 0x00, 0x01};
void seeed_calib_4() {
	seeed_calib("4", _seeed_calib_4_read, _seeed_calib_4_write);
}

const byte _seeed_calib_7_read[6] = {PH_ADDRESS, 0x03, 0x00, 0x31, 0x00, 0x01};
byte _seeed_calib_7_write[6] = {PH_ADDRESS, 0x06, 0x00, 0x31, 0x00, 0x01};
void seeed_calib_7() {
	seeed_calib("7", _seeed_calib_7_read, _seeed_calib_7_write);
}

const byte _seeed_calib_10_read[6] = {PH_ADDRESS, 0x03, 0x00, 0x32, 0x00, 0x01};
byte _seeed_calib_10_write[6] = {PH_ADDRESS, 0x06, 0x00, 0x32, 0x00, 0x01};
void seeed_calib_10() {
	seeed_calib("10", _seeed_calib_10_read, _seeed_calib_10_write);
}

const byte _seeed_ph[6] = {PH_ADDRESS, 0x03, 0x00, 0x01, 0x00, 0x01};
bool seeed_ph(float *ret) {
	bool ok = _modbus_float_be16(_seeed_ph, 6, ret);
	return ok;
}
#endif
