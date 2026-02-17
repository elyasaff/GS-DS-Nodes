//Last Update: 12_02_2026 : readResponse() null-terminates on timeout, BROADCAST_DELAY_MILLIS 8000->12000
//Last Update: 11_11_2025 : changed BROADCAST_DELAY_MILLIS and SD_FILE_NAME

#define BAUD_RATE 9600
#define CLIENT_DELAY_MILLIS 250
#define BROADCAST_DELAY_MILLIS 8000             //changed from 12000 15_02_26
#define DELIMITER_CHAR ((byte)31)
#define END_CHAR ((byte)127)
#define PROTOCOL_SECONDS ((BROADCAST_DELAY_MILLIS + (40 * CLIENT_DELAY_MILLIS)) / 1000)

void query(byte address) {
	digitalWrite(RTS_PIN, HIGH);
	delay(5);
	Serial1.write(address);
	digitalWrite(RTS_PIN, LOW);
}

void broadcast() {
	Serial.println("broadcast");
	query(0);
	delay(BROADCAST_DELAY_MILLIS);
	while(Serial1.available()) Serial1.read();
}

bool readResponse(char *msg1, char *msg2, byte max_len) {
	byte len = 0;
	char c, *buf = msg1;
	*msg1 = *msg2 = 0;
	unsigned long start = millis();
	while (millis() - start < CLIENT_DELAY_MILLIS && len < max_len) {
		if (Serial1.available()) {
			switch (c = Serial1.read()) {
				case END_CHAR:
					buf[len] = 0;
					return true;
				case DELIMITER_CHAR:
					buf[len] = 0;
					buf = msg2;
					len = 0;
					break;
				default:
					buf[len++] = c;
			}
		}
	}
	// Timeout or overflow — make sure both buffers are properly terminated
	msg1[max_len] = 0;
	msg2[max_len] = 0;
	return false;
}
