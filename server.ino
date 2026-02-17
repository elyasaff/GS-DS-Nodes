// Server / Node code (QTpy's)

//Last Update:
//12_02_2026 : moved flush() to after the send loop instead of per-parameter to reduce blocking time
//15_09_2025 : added printing at server_sample() - so results can be see with client control

#include <Arduino.h>
#include <wiring_private.h>

#define Serial2_RX  8
#define Serial2_TX  10

unsigned long startTimeServer = 0;
unsigned long durationTimeServer = 0;

#define ADDRESS 2    // Node address: set a unique number 1-30 for each node
#include "auxilary/protocol_server.h"
//#include "GSNode_V2_0.h"
#include "GSNode_V2_0_Debug.h"

#define TIMEOUT 5

#if defined(SEEED_XIAO_M0) || defined(ARDUINO_SEEED_XIAO_RP2040) || defined(ADAFRUIT_QTPY_M0)
	#define RELAY_PIN 0
	#define RTS_PIN 1
#elif defined(ARDUINO_ADAFRUIT_FEATHER_RP2040)
	#define RELAY_PIN A1
	#define RTS_PIN A2
#else
	#define RELAY_PIN A0
	#define RTS_PIN A1
#endif


void setup() {
	Serial.begin(115200);
	Serial.println("server setup begin");
	Serial1.begin(BAUD_RATE);
	Serial1.setTimeout(TIMEOUT);

	pinPeripheral(Serial2_RX, PIO_SERCOM_ALT);
	pinPeripheral(Serial2_TX, PIO_SERCOM_ALT);

	server_setup();
}

void server_sample() {
	startTimeServer = millis();
	digitalWrite(RELAY_PIN, HIGH);
	delay(10u);
	sample();
	digitalWrite(RELAY_PIN, LOW);
}

#define USER_COMMAND_N 41
char user_command[USER_COMMAND_N] = {0};
byte user_command_i = 0;

void server_send(Stream *s, char delimiter, char end) {
	unsigned long endTimeServer = millis();
	durationTimeServer = endTimeServer - startTimeServer;
	Serial.print("cycle of server.ino took: ");
	Serial.print(durationTimeServer);
	Serial.println(" milliseconds");

	for (byte i = 0; i < PARAMETERS_N; i++) {
		if (server_names[i][0]) {
			s->print(ADDRESS, DEC);
			s->print('_');
			s->print(server_names[i]);
			if (!server_ok[i]) {
				s->print("_err");
			}
			s->write(delimiter);
			s->print(server_reads[i], 3);
			s->write(end);
		}
	}
	s->flush();
	Serial.println("sending Data to Client");
}

void loop() {
	server_loop();

	// Handle CLIENT commands (Serial1)
	while (Serial1.available()) {
		byte cmd = Serial1.read();
		if (cmd == 0) {
			Serial.println("Got broadcast command from Client");
			server_sample();
		}
		else if (cmd == ADDRESS) {
			digitalWrite(RTS_PIN, HIGH);
			delay(10u);
			server_send(&Serial1, DELIMITER_CHAR, END_CHAR);
			digitalWrite(RTS_PIN, LOW);
		}
	}

	// Handle USER commands (USB Serial)
	while (Serial.available()) {
		char c = Serial.read();
		if (c == '\n') {
			user_command[user_command_i] = 0;
			if (strcmp("0", user_command) == 0) {
				server_sample();
				server_send(&Serial, ',', '\n');
			} else {
				digitalWrite(RELAY_PIN, HIGH);
				delay(10u);
				interact(user_command);
				digitalWrite(RELAY_PIN, LOW);
			}
			user_command_i = 0;
		}
		else {
			if (user_command_i < USER_COMMAND_N - 1) {
				user_command[user_command_i++] = (c == '\r') ? 0 : c;
			}
		}
	}
}
