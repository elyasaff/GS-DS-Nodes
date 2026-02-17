#ifndef ATLAS_AUX_H_
#define ATLAS_AUX_H_
#include <Wire.h>

#define IIC_MSG_N 40
#define IIC_MAX 127
#define IIC_MIN 1
#define ATLAS_NAME_N 8

//todo:add atsc_calib_man()

void atsc_write(const char *cmd, byte address) {
	Wire.beginTransmission(address);
	Wire.write(cmd, strlen(cmd));
	Wire.endTransmission();
}
#define IIC_NONE 0u
#define IIC_ERROR 1u
#define IIC_ERROR 1u
#define IIC_UTOA 100u
#define IIC_NAME_LEN 101u
#define IIC_READ 102u

char iic_read[IIC_MSG_N+1] = {0}; //+1 for '\0'
unsigned int atsc_read(byte address) {
	Wire.requestFrom(int(address), int(IIC_MSG_N), true);
	byte i = 0;
	byte first_byte = Wire.read(); //first byte is err code
	if (first_byte == 1) {
		while (Wire.available() && i < IIC_MSG_N)
			iic_read[i++] = Wire.read();
		iic_read[i] = 0;
		return IIC_NONE;
	}
	Serial.print("wire error:");
	Serial.println(first_byte);
	return IIC_ERROR;
}

unsigned int atsc_cmd(const char *cmd, byte address, unsigned long d) {
	atsc_write(cmd, address);
	delay(d);
	return atsc_read(address);
}

void atsc_sleep(byte address) {
	atsc_write("sleep", address);
}

unsigned int atsc_name(char *ret, byte ret_n, byte address) {
	*ret = 0;
	unsigned int err = atsc_cmd("i", address,500);
	if (err == IIC_NONE) {
		byte i = 3; /*strlen("?I,")*/
		byte j = 0;
		while (iic_read[i] && iic_read[i] != ',' && j < ret_n - 1)
			ret[j++] = iic_read[i++];
		if (j == ret_n - 1)
			return IIC_NAME_LEN;
		ret[j++] = '_';
		if (!utoa(address, ret + j, 10))
			return IIC_UTOA;
	}
	return err;
}

char *iic_read_end;
unsigned int atsc_sample(float *ret, byte address) {
	unsigned int err = atsc_cmd("r", address, 1500);
	if (err == IIC_NONE) {
		*ret = strtod(iic_read, &iic_read_end);
		err = (*iic_read_end == 0) ? IIC_NONE : IIC_READ;
	}
	return err;
}

bool valid_iic_address(byte a) {
	Wire.beginTransmission(a);
	if (Wire.endTransmission() == 0) {
		while (Wire.available())
			Wire.read();
		return true;
	}
	return false;
}

char device[ATLAS_NAME_N] = {0};
void print_iic_devices() {
	for (byte i = IIC_MIN; i <= IIC_MAX; i++) {
		if(valid_iic_address(i)) {
			if (atsc_name(device, ATLAS_NAME_N, i) != IIC_NONE)
				break;
			Serial.println(device);
		}
	}
}

void atsc_setup() {
	Wire.begin();
#ifdef ARDUINO_ADAFRUIT_FEATHER_RP2040
	Wire.setTimeout(1500u, true); //timeout is available in rp2040
#endif
}

bool atsc_sample(byte address, char *name, float *read, bool *ok) {
	if (!valid_iic_address(address))
		return false;
	byte err;
	err = atsc_name(name, ATLAS_NAME_N, address);
	if (err != IIC_NONE) {
		*read = err;
		*ok = false;
		return true;
	}
	err = atsc_sample(read, address);
	if (err != IIC_NONE) {
		*read = err;
		*ok = false;
		return true;
	}
	*ok = true;
	return true;
}

bool atsc_interact(const char *cmd, unsigned long int d=0) {
	delay(d);
	byte n = strlen(cmd);
	if (n == 1 && cmd[0] == '!') {
		print_iic_devices();
		return true;
	}
	char *endptr;
	byte address = strtoul(cmd, &endptr, 0);
	if (*endptr == 0 || *(endptr+1) == 0 || *endptr != ':' || address < IIC_MIN || address > IIC_MAX) {
		Serial.print("iic address error. ");
		Serial.print("endptr:");
		Serial.print(*endptr);
		Serial.print(". endptr+1:");
		Serial.print(*(endptr+1));
		Serial.print(". address:");
		Serial.print(address);
		Serial.println();
		return false;
	}
	endptr++;
	n -= (endptr - cmd);
	if (!valid_iic_address(address)) {
		Serial.println("invalid address");
		return false;
	}
	if (n == 1 && endptr[0] == 'r') {
		/*if 'r' was send once, send again until other command is set*/
		/*note that 'R' will give only one reading*/
		while (!Serial.available() && !Serial1.available()) {
			if (atsc_cmd(endptr, address, 1500) == IIC_NONE)
				Serial.println(iic_read);
		}
		return true;
	} else if (n && atsc_cmd(endptr, address, 1500) == IIC_NONE) {
		Serial.println(iic_read);
		return true;
	}
	return false;
}

#endif
