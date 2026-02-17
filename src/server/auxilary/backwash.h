#ifndef BACKWASH_AUX_H_
#define BACKWASH_AUX_H_

#if defined(ADAFRUIT_QTPY_M0) || defined(SEEED_XIAO_M0)
#define BUOY 0
#define BWBUOY 1
#define OPEN 5
#define CLOSE 6
#define PUMP 7
#define BWPUMP 8
#elif defined(ARDUINO_ADAFRUIT_FEATHER_RP2040)
#define BUOY A3
#define BWBUOY D24
#define OPEN 13
#define CLOSE 12
#define PUMP 11
#define BWPUMP 10
#endif

/* we use relays that work on LOW */
bool enough_water_for_backwash = true;
bool pump = false;
unsigned long now;

#define COUNT 2
byte count = 0;

#define LOOPS 250
bool float_(byte pin) {
	byte ret = 0;
	for(byte i = 0; i < LOOPS; delay(1), i++)
		ret += digitalRead(pin);
	return ret < (LOOPS >> 2);
}
bool float_buoy() { return float_(BUOY); }
bool float_bwbuoy() { return float_(BWBUOY); }

void valve_(byte pin) {
	digitalWrite(pin, LOW);
	delay(20 * 1000);
	digitalWrite(pin, HIGH);
}
void backwash_valve_open() { Serial.println("backwash valve open"); valve_(OPEN); }
void backwash_valve_close() { Serial.println("backwash valve close"); valve_(CLOSE); }


void backwash_setup() {
	pinMode(BUOY, INPUT_PULLUP);
	pinMode(BWBUOY, INPUT_PULLUP);

	pinMode(OPEN, OUTPUT);
	digitalWrite(OPEN, HIGH);
	pinMode(CLOSE, OUTPUT);
	digitalWrite(CLOSE, HIGH);

	pinMode(PUMP, OUTPUT);
	digitalWrite(PUMP, HIGH);

	pinMode(BWPUMP, OUTPUT);
	digitalWrite(BWPUMP, HIGH);

	backwash_valve_close();
}

void backwash() {
	backwash_valve_open();
	Serial.println("backwash");
	now = millis();
	digitalWrite(BWPUMP, LOW);
	while (
		(enough_water_for_backwash = float_bwbuoy())
		&&
		millis() - now < 7 * 1000
	);
	digitalWrite(BWPUMP, HIGH);
	backwash_valve_close();
}

void backwash_loop () {
	if (pump != float_buoy()) {
		pump = !pump;
		Serial.println((pump) ? "pump on" : "pump off");
		digitalWrite(PUMP, (pump) ? LOW : HIGH);
		if (!pump && enough_water_for_backwash) {
			count = (count + 1) % COUNT;
			if (count == 0) {
				backwash();
			}
		}
	}
	delay(10);
}
#endif
