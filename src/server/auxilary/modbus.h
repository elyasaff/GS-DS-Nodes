#ifndef MODBUS_AUX_H_
#define MODBUS_AUX_H_

#if defined(ADAFRUIT_QTPY_M0) || defined(SEEED_XIAO_M0)
	#include <Arduino.h>   // required before wiring_private.h
	#include <wiring_private.h> // pinPeripheral() function
	#define RX2 8
	#define TX2 10
	#define MODBUS_RTS_PIN 9
	#if defined(ADAFRUIT_QTPY_M0)
		Uart _modbus_Serial (&sercom2, RX2, TX2, SERCOM_RX_PAD_3, UART_TX_PAD_2);
	#elif defined(SEEED_XIAO_M0)
		Uart _modbus_Serial (&sercom0, RX2, TX2, SERCOM_RX_PAD_3, UART_TX_PAD_2);
	#endif
	Uart *modbus_Serial = &_modbus_Serial;
	void SERCOM0_Handler() { modbus_Serial->IrqHandler(); }
#elif defined(ARDUINO_ADAFRUIT_FEATHER_RP2040)
	#define RX2 D9
	#define TX2 D6
	#define MODBUS_RTS_PIN D10
	SerialUART *modbus_Serial = &Serial2;
#endif

void modbus_setup() {
#if defined(ADAFRUIT_QTPY_M0) || defined(SEEED_XIAO_M0)
	pinPeripheral(RX2, PIO_SERCOM_ALT);
	pinPeripheral(TX2, PIO_SERCOM_ALT);
#elif defined(ARDUINO_ADAFRUIT_FEATHER_RP2040)
	modbus_Serial->setRX(RX2);
	modbus_Serial->setTX(TX2);
#endif
	modbus_Serial->begin(BAUD_RATE);
	pinMode(MODBUS_RTS_PIN, OUTPUT);
	digitalWrite(MODBUS_RTS_PIN, LOW);
	while(modbus_Serial->available())
		modbus_Serial->read();
}

/* https://modbus.org/docs/Modbus_Application_Protocol_V1_1b.pdf */

/*https://ctlsys.com/support/how_to_compute_the_modbus_rtu_message_crc*/
uint16_t _crc16_step(uint16_t b, uint16_t crc) {
	crc ^= b;          // XOR byte into least sig. byte of crc

	for (int i = 8; i != 0; i--) {    // Loop over each bit
		if ((crc & 0x0001) != 0) {      // If the LSB is set
		crc >>= 1;                    // Shift right and XOR 0xA001
		crc ^= 0xA001;
	} else                            // Else LSB is not set
		crc >>= 1;                    // Just shift right
	}
	return crc;
}

uint16_t _modbus_crc16(const byte buf[], int n) {
	uint16_t crc = 0xFFFF;
	for (int pos = 0; pos < n; pos++)
		crc = _crc16_step(buf[pos], crc);
	return (crc << 8) | (crc >> 8); //modbus wants low bits before high bits
}

void modbus_send(const byte *cmd, byte n) {
	Serial.print("modbus_send to device:");
	Serial.println(cmd[0],HEX);
	uint16_t crc = _modbus_crc16(cmd, n);
	byte crc_first = crc >> 8;
	byte crc_second = crc; //trim first 8 bits
	while(modbus_Serial->available()) modbus_Serial->read();
	digitalWrite(MODBUS_RTS_PIN, HIGH);
	delay(5);
	modbus_Serial->write(cmd, n);
	modbus_Serial->write(crc_first);
	modbus_Serial->write(crc_second);
	modbus_Serial->flush();
	digitalWrite(MODBUS_RTS_PIN, LOW);
}

enum error_type {
	MODBUS_NONE = 0,
	BYTES,
	MADDRESS,
	FUNCTION,
	LENGTH,
	CRC
};

byte _error_byte;
char const *_error_str;
error_type _modbus_read(const byte *cmd, const byte cmd_n, byte *response, const byte response_n) {
	uint16_t crc = 0xFFFF;
	if (modbus_Serial->available() < 3) {
		Serial.print("modbus: not enough bytes- ");
		Serial.println(modbus_Serial->available());
		return BYTES;
	}
	byte address, function_code, n;
	address = modbus_Serial->read();
	if (address != cmd[0]) {
		Serial.print("modbus: not the same address- ");
		Serial.print(address,HEX);
		Serial.print(',');
		Serial.println(cmd[0],HEX);
		return MADDRESS;
	}
	crc = _crc16_step(address,crc);
	function_code = modbus_Serial->read();
	if (function_code != cmd[1]) {
		Serial.print("modbus: not the same function code- ");
		Serial.print(function_code,HEX);
		Serial.print(',');
		Serial.println(cmd[1],HEX);
		return FUNCTION;
	}
	crc = _crc16_step(function_code,crc);
	n = modbus_Serial->read();
	if (n != response_n) {
		Serial.print("modbus: not the same length- ");
		Serial.print(n);
		Serial.print(',');
		Serial.println(response_n);
		return LENGTH;
	}
	crc = _crc16_step(n,crc);
	for (byte i = 0; i < n; i++) {
		response[i] = modbus_Serial->read();
		crc = _crc16_step(response[i],crc);
	}
	if (modbus_Serial->available() == 2 &&
		crc == ((uint16_t)modbus_Serial->read() << 8 |
			(uint16_t)modbus_Serial->read())
	) {
		Serial.println("modbus: invalid crc");
		return CRC;
	}
	return MODBUS_NONE;
}
byte modbus_response[8]; /*8 for 64 bit if needed*/
enum byte_order {
	BE32,
	BS32,
	BEU16
};
#define MODBUS_SEND_RECV_DELAY 100u
bool modbus_send_recv(const byte *cmd, byte cmd_n) {
	modbus_send(cmd, cmd_n);
	delay(MODBUS_SEND_RECV_DELAY);
	bool ret = modbus_Serial->available();
	if (modbus_Serial->read() != cmd[0]) /*address*/
		return false;
	if (modbus_Serial->read() != cmd[1]) /*function code*/
		return false;
	while(modbus_Serial->available())
		modbus_Serial->read();
	return ret;
}

bool _modbus_float(const byte *cmd, const byte cmd_n, float *ret, const byte_order type) {
	modbus_send(cmd, cmd_n);
	delay(MODBUS_SEND_RECV_DELAY);
	byte response_n;
	switch (type) {
	case BE32:
	case BS32:
		response_n = 4;
		break;
	case BEU16:
		response_n = 2;
		break;
	default:
		return false;
	}
	error_type err = _modbus_read(cmd, cmd_n, modbus_response, response_n);
	if (err == MODBUS_NONE) {
		uint32_t ieee754;
		switch (type) {
		case BE32:
			/*big endian*/
			ieee754 = uint32_t(modbus_response[0])<<24 |
				uint32_t(modbus_response[1])<<16 |
				uint32_t(modbus_response[2])<<8 |
				uint32_t(modbus_response[3]);
				*ret = *(float *)&ieee754;
			break;
		case BS32:
			/*byte swap*/
			ieee754 = uint32_t(modbus_response[2])<<24 |
				uint32_t(modbus_response[3])<<16 |
				uint32_t(modbus_response[0])<<8 |
				uint32_t(modbus_response[1]);
				*ret = *(float *)&ieee754;
			break;
		case BEU16:
			*ret = float(
				uint16_t(modbus_response[0])<<8 |
				uint16_t(modbus_response[1])
			) /100;
			break;
		default:
			return false;
		}
		return true;
	} else {
		*ret = err;
		return false;
	}
}
bool _modbus_int16(const byte *cmd, const byte cmd_n, int16_t *ret) {
	modbus_send(cmd, cmd_n);
	delay(MODBUS_SEND_RECV_DELAY);
	error_type err = _modbus_read(cmd, cmd_n, modbus_response, 2);
	if (err == MODBUS_NONE) {
		*ret = uint16_t(modbus_response[0])<<8 |
			uint16_t(modbus_response[1]);
		return true;
	} else {
		*ret = err;
		return false;
	}
}
bool _modbus_float_be32(const byte *cmd, const byte cmd_n, float *ret) {
	return _modbus_float(cmd, cmd_n, ret, BE32);
}
bool _modbus_float_bs32(const byte *cmd, const byte cmd_n, float *ret) {
	return _modbus_float(cmd, cmd_n, ret, BS32);
}
bool _modbus_float_be16(const byte *cmd, const byte cmd_n, float *ret) {
	return _modbus_float(cmd, cmd_n, ret, BEU16);
}
bool modbus_interact(const char *a, bool (**functions)(float *), const char **names, const byte n) {
	float t;
	for (byte i = 0; i < n; i++) {
		if (strcmp(a, names[i]) == 0) {
			if (functions[i](&t)) {
				Serial.println(t);
			}
			return true;
		}
	}
	return false;
}

#endif
