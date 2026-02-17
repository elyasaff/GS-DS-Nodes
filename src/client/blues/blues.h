//Last Update: 12_02_2026 - added DEBUG_MODE support: SD-only, no sleep, JSON body reset between cycles
//Last Update: 12_02_2026 - sleep calculation based on clock time, wakes ~8s before next round minute
//Last Update: 30_09_2025 - results in integer and not float

#ifndef BLUES_H_
#define BLUES_H_
#include "nc.h"

#include <RTClib.h>  

// SD card (8.3 file names only)
#include <SPI.h>
#include <SD.h>
File sd_file;
#define SD_CHIP 4
#define SD_FILE_NAME "29122025.txt"

#ifndef NO_DATETIME
#include <AceTime.h>
using namespace ace_time;
char datetime_buf[64] = {0};
static ace_time::ExtendedZoneProcessor sZoneProcessor; 


// Convert epoch to "HH:MM:SS DD-MM-YYYY" in Asia/Jerusalem
void epochToIsraelString(uint32_t epoch, char* buffer, size_t bufSize) {
	const TimeZone tz = TimeZone::forZoneInfo(&zonedbx::kZoneAsia_Jerusalem, &sZoneProcessor);
	ZonedDateTime zdt = ZonedDateTime::forUnixSeconds64((int64_t)epoch, tz);
	snprintf(buffer, bufSize, "%02d:%02d:%02d %02d/%02d/%04d",
		zdt.hour(), zdt.minute(), zdt.second(),
		zdt.day(), zdt.month(), zdt.year());
}
#endif

J *body;

void client_setup() {
	nc_begin(PRODUCT_UID, OUTBOUND, INBOUND, VOLTAGE_MODE, NULL);
	Serial.println("NoteCard Hub.set command sent");
	nc_enable_motion();
	nc_disable_gps();
	nc_debug(&Serial);
	body = JCreateObject();
	nc_template_add(NOTE_FILE_NAME);

	SD.begin(SD_CHIP);
}

unsigned long time;
float temperature, voltage;

bool client_check() {
	if ((time = nc_time())) {
		if (nc_temperature(&temperature))
			JAddNumberToObject(body, "NC_T", temperature);
		if (nc_voltage(&voltage))
			JAddNumberToObject(body, "NC_V", voltage);
		return true;
	}
	return false;
}

// Record the timestamp — call this after warm-up so epoch matches measurement time
void client_stamp() {
	time = nc_time();
	JAddNumberToObject(body, "epoch", time);
#ifndef NO_DATETIME
	epochToIsraelString(time, datetime_buf, sizeof(datetime_buf));
	JAddStringToObject(body, "datetime", datetime_buf);
#endif
}
void client_add(const char *key, float value) {
	JAddNumberToObject(body, key, value);
}
void client_send() {
	Serial.println(JPrintUnformatted(body));

	// Always write to SD card
	if ((sd_file = SD.open(SD_FILE_NAME, FILE_WRITE))) {
		sd_file.println(JPrintUnformatted(body));
		sd_file.close();
	}

#ifndef DEBUG_MODE
	// Normal mode: also send to Notecard
	// Note: nc_note_add takes ownership of the body pointer (frees it internally),
	// so we pass a duplicate and keep the original for us to delete below.
	J *body_copy = JDuplicate(body, true);
	nc_note_add(body_copy, NOTE_FILE_NAME);
#endif

	delay(1000);  // let nc_debug print before sleep

	// Reset JSON body for next cycle
	JDelete(body);
	body = JCreateObject();
}

void client_loop() {
	long interval_seconds = nc_interval();
	if (interval_seconds <= PROTOCOL_SECONDS) // some default value if didn't get env variable, or the env variable is too small
		interval_seconds = SLEEP_SECONDS;

	unsigned long endTime = millis();  // Record the end time
    duration = endTime - startTime;    // Calculate the duration
	Serial.print("Client took: "); Serial.print(duration); Serial.println(" milliseconds");

	// Calculate sleep: wake up WAKE_BEFORE_MINUTE_SEC before the next interval boundary
	// Step 1: how far are we into the current interval?
	// Step 2: how many seconds until the next boundary?
	// Step 3: subtract WAKE_BEFORE so we wake up early (for sensor warm-up)
	unsigned long now = nc_time();
	long into = now % interval_seconds;                          // seconds into current interval
	long until_boundary = interval_seconds - into;               // seconds until next boundary
	long sleep_seconds = until_boundary - WAKE_BEFORE_MINUTE_SEC;

	// If we're already in the warm-up zone (sleep would be < 10s),
	// skip this boundary and aim for the next one
	if (sleep_seconds < 10) sleep_seconds += interval_seconds;

	/*
	Serial.print("now="); Serial.print(now);
	Serial.print(" into="); Serial.print(into);
	Serial.print(" sleep="); Serial.print(sleep_seconds); Serial.println("s");
    */ 
	 
#ifdef DEBUG_MODE
	// Debug mode: stay awake, just delay until next cycle
	Serial.print("DEBUG: waiting "); Serial.print(sleep_seconds); Serial.println("s until next cycle");
	delay(sleep_seconds * 1000UL);
#else
	// Normal mode: sleep (resets MCU on wake)
	nc_sleep(sleep_seconds);
	delay(1000u);
#endif
}
#endif