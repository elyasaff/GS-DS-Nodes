//Last Update: 12_02_2026  Added LED status indicators: red (pin 13) + green (pin 8) show current stage
//Last Update: 12_02_2026  Configurable battery monitors: MOJO1/MOJO2/MAXLIPO1/MAXLIPO2 enable/disable switches
//Last Update: 12_02_2026  Added DEBUG_MODE: SD-only, no Notecard, no sleep, loops continuously
//Last Update: 12_02_2026  Added guard for negative/zero delay before broadcast
//Last Update: 12_02_2026  Fixed sprintf buffer overflow in delay message
//Last Update: 31_12_2025  Changed PRODUCT_UID, Changed NOTE_FILE_NAME to "readings1.qo"
//Last Update: 30_11_2025  added mojoX2 with multiplexer, removed Max17048 V
//Last Update: 24_11_2025  added Templates, sleep by time, added LTC2959

unsigned long startTime;
unsigned long duration;

#define BLUES
#define WAKE_BEFORE_MINUTE_SEC 14   // wake up this many seconds before the next round minute (for warming sensors) - changed from 16 15_2_26

// Uncomment the line below to enable debug mode:
// - Data goes to SD card only (no Notecard)
// - Client loops continuously (no sleep/reset)
// - JSON body is reset between cycles (no memory leak)
//#define DEBUG_MODE

// ╔════════════════════════════════════════════════════════════════════╗
// ║               BATTERY MONITOR CONFIGURATION                       ║
// ║                                                                    ║
// ║  Set to true to ENABLE, false to DISABLE each monitor              ║
// ║  All monitors connect via PCA9546 I2C multiplexer                  ║
// ║  MOJO1  (LTC2959 #1)   → PCA channel 0                            ║
// ║  MOJO2  (LTC2959 #2)   → PCA channel 1                            ║
// ║  MAXLIPO1 (MAX17048 #1) → PCA channel 2                            ║
// ║  MAXLIPO2 (MAX17048 #2) → PCA channel 3                            ║
// ╚════════════════════════════════════════════════════════════════════╝
#define ENABLE_MOJO1     true    // LTC2959 Coulomb counter on PCA channel 0
#define ENABLE_MOJO2     true    // LTC2959 Coulomb counter on PCA channel 1
#define ENABLE_MAXLIPO1  false   // MAX17048 fuel gauge on PCA channel 2
#define ENABLE_MAXLIPO2  false   // MAX17048 fuel gauge on PCA channel 3

#if defined(ADAFRUIT_FEATHER_M0)
	#define RTS_PIN A1
	#define SERVERS_RELAY_PIN A0
	#define VBAT_PIN A7
	#define VBAT_REF 3.3
#else
	#define RTS_PIN A1
	#define SERVERS_RELAY_PIN 0
	#define VBAT_PIN A7
	#define VBAT_REF 3.3
#endif

// ╔════════════════════════════════════════════════════════════════════╗
// ║                   LED STATUS INDICATORS                            ║
// ║                                                                    ║
// ║  Red (pin 13) + Green (pin 8) show current stage:                  ║
// ║  Boot/setup:       Red ON,   Green OFF                             ║
// ║  Broadcasting:     Red ON,   Green ON                              ║
// ║  Receiving data:   Red OFF,  Green blink                           ║
// ║  Sending to cloud: Red blink, Green OFF                            ║
// ║  Sleep/idle:       Red OFF,  Green OFF                             ║
// ╚════════════════════════════════════════════════════════════════════╝
#define LED_RED   13
#define LED_GREEN 8

void led_setup() {
	pinMode(LED_RED, OUTPUT);
	pinMode(LED_GREEN, OUTPUT);
	digitalWrite(LED_RED, LOW);
	digitalWrite(LED_GREEN, LOW);
}

void led_boot()      { digitalWrite(LED_RED, HIGH); digitalWrite(LED_GREEN, LOW);  }
void led_broadcast() { digitalWrite(LED_RED, HIGH); digitalWrite(LED_GREEN, HIGH); }
void led_receive()   { digitalWrite(LED_RED, LOW);  digitalWrite(LED_GREEN, HIGH); }
void led_send()      { digitalWrite(LED_RED, HIGH); digitalWrite(LED_GREEN, LOW);  }
void led_off()       { digitalWrite(LED_RED, LOW);  digitalWrite(LED_GREEN, LOW);  }

#include "protocol_client.h"
#if defined(BLUES)
	#ifndef OUTBOUND
		#define OUTBOUND "usb:2;high:30;normal:60;low:120;dead:0"
	#endif
	#ifndef INBOUND
		#define INBOUND "usb:2;high:30;normal:60;low:120;dead:0"
	#endif
	#ifndef VOLTAGE_MODE
		#define VOLTAGE_MODE "lipo"
	#endif
	#ifdef DEBUG
		#define SLEEP_SECONDS (2 + PROTOCOL_SECONDS)
		#define NOTE_FILE_NAME "debug.qo"
	#else
		#define SLEEP_SECONDS 120
		#define NOTE_FILE_NAME "GSreadings_v1_0.qo"
	#endif
	static_assert(SLEEP_SECONDS > PROTOCOL_SECONDS);
	#define PRODUCT_UID "il.ac.bgu.levintal:boreholestest"
	#include "blues/blues.h"
#elif defined(DUMMY)
	#include "dummy.h"
#endif

// Battery monitor objects (only create if enabled)
#if ENABLE_MOJO1 || ENABLE_MOJO2
	#include <LTC2959.h>
	LTC2959 batteryMonitor;
#endif

#if ENABLE_MAXLIPO1 || ENABLE_MAXLIPO2
	#include "Adafruit_MAX1704X.h"
	Adafruit_MAX17048 maxlipo;
#endif

#define PCAADDR 0x70
#define MAX_MSG 40
char key[MAX_MSG+1] = {0};
char value[MAX_MSG+1] = {0};


void setup () {
	delay(1000);
	led_setup();
	led_boot();
	Serial1.begin(BAUD_RATE);
	Serial1.setTimeout(CLIENT_DELAY_MILLIS);
	Serial.setTimeout(CLIENT_DELAY_MILLIS);
	client_setup();

	// Initialize battery monitors on their PCA channels
#if ENABLE_MOJO1
	pcaselect(0);
	if (batteryMonitor.begin()) {
		Serial.print("[MOJO1] ✓ Found, ");
		if (batteryMonitor.enableCounter(LTC2959::LOW_DEADBAND))
			Serial.println("counter enabled");
		else
			Serial.println("counter failed");
	} else {
		Serial.println("[MOJO1] Not detected");
	}
#endif

#if ENABLE_MOJO2
	pcaselect(1);
	if (batteryMonitor.begin()) {
		Serial.print("[MOJO2] ✓ Found, ");
		if (batteryMonitor.enableCounter(LTC2959::LOW_DEADBAND))
			Serial.println("counter enabled");
		else
			Serial.println("counter failed");
	} else {
		Serial.println("[MOJO2] Not detected");
	}
#endif

#if ENABLE_MAXLIPO1
	pcaselect(2);
	if (maxlipo.begin()) {
		Serial.println("[MAXLIPO1] ✓ Found");
	} else {
		Serial.println("[MAXLIPO1] Not detected");
	}
#endif

#if ENABLE_MAXLIPO2
	pcaselect(3);
	if (maxlipo.begin()) {
		Serial.println("[MAXLIPO2] ✓ Found");
	} else {
		Serial.println("[MAXLIPO2] Not detected");
	}
#endif

	pinMode(SERVERS_RELAY_PIN, OUTPUT);
	digitalWrite(SERVERS_RELAY_PIN, LOW);
	pinMode(RTS_PIN, OUTPUT);
	digitalWrite(RTS_PIN, LOW);
}


byte address;
char *value_end;
float v;

void loop () {
	bool user = (Serial.available() && Serial.read() == '0');
	bool non_user = !user & client_check();
	if (user) Serial.println("user");
	else if (non_user) Serial.println("non user command");

	if (user | non_user) {
	#if ENABLE_MOJO1
		pcaselect(0);
		if (batteryMonitor.enableCounter(LTC2959::LOW_DEADBAND)) {
			client_add("Mojo1_V", batteryMonitor.readVoltage());
			client_add("Mojo1_C", batteryMonitor.readCharge_mAh());
		}
	#endif

	#if ENABLE_MOJO2
		pcaselect(1);
		if (batteryMonitor.enableCounter(LTC2959::LOW_DEADBAND)) {
			client_add("Mojo2_V", batteryMonitor.readVoltage());
			client_add("Mojo2_C", batteryMonitor.readCharge_mAh());
		}
	#endif

	#if ENABLE_MAXLIPO1
		pcaselect(2);
		client_add("MaxL1_V", maxlipo.cellVoltage());
	#endif

	#if ENABLE_MAXLIPO2
		pcaselect(3);
		client_add("MaxL2_V", maxlipo.cellVoltage());
	#endif

		// Remote Coulomb counter reset via NoteHub environment variables
	#if ENABLE_MOJO1
		if (nc_env_get_bool("mojo1_reset")) {
			pcaselect(0);
			batteryMonitor.resetChargeCounter();
			nc_env_set("mojo1_reset", "0");
			Serial.println("[MOJO1] Counter reset (remote)");
		}
	#endif
	#if ENABLE_MOJO2
		if (nc_env_get_bool("mojo2_reset")) {
			pcaselect(1);
			batteryMonitor.resetChargeCounter();
			nc_env_set("mojo2_reset", "0");
			Serial.println("[MOJO2] Counter reset (remote)");
		}
	#endif

		digitalWrite(SERVERS_RELAY_PIN, HIGH);
    Serial.println("Servers On");

		// Wait until the top of the next minute (sensors warm up during this time)
		DateTime a(nc_time());
		uint8_t secq = a.second();
		int delayq = 60 - secq;
		if (delayq < 1) delayq = 1;
		char buffer1[40];
		snprintf(buffer1, sizeof(buffer1), "warming sensors, wait %d seconds...", delayq);
		Serial.println(buffer1);
		delay(delayq * 1000);

		client_stamp();  // record epoch/datetime now (at the round minute)
		startTime = millis();
		led_broadcast();
		broadcast();
		led_receive();
		Serial.println("query");
		for (address = 1; address <= 6; address++) {
			query(address);
			while(readResponse(key,value,MAX_MSG)) {
				Serial.print("key:");
				Serial.print(key);
				Serial.print(',');
				Serial.print("value:");
				Serial.println(value);
				if (*key && *value) {
					v = strtof(value, &value_end);
					if (*value_end == 0) {
					#ifdef DEBUG_MODE
						client_add(key, v);  // debug: always store data
					#else
						if (non_user) client_add(key, v);
					#endif
					} else {
						Serial.println("error in strtod(value):");
					}
				}
			}
		}
		led_send();
	#ifdef DEBUG_MODE
		client_send();  // debug: always send (SD only)
	#else
		if (non_user) client_send();
	#endif
		digitalWrite(SERVERS_RELAY_PIN, LOW);
		Serial.println("Servers Off");
	}
	led_off();
	client_loop();
}


void pcaselect(uint8_t i) {
  if (i > 3) return;
  Wire.beginTransmission(PCAADDR);
  Wire.write(1 << i);
  Wire.endTransmission();
}
