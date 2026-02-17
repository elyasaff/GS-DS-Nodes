//Last Update: 12_02_2026 - added nc_env_get_bool() and nc_env_set() for remote env variable control
//Last Update: 12_02_2026 - fixed req vs mode_req bug in nc_begin, fixed nc_lipo triple send, fixed dead code in nc_template_add

#ifndef nc_h_
#define nc_h_

#include <Notecard.h>
#include <Arduino.h>

#ifndef NC_MODE
	#define NC_MODE "periodic"
#endif

Notecard notecard;

bool nc_begin(const char* productuid, const char *voutbound, const char *vinbound, const char *voltage_mode, Uart* s) {
	if (!productuid)
		return false;
	if (s) /*uart*/
		notecard.begin(*s); //TODO:check for return
	else /*i2c*/
		notecard.begin(); //TODO:check for return

	J* req;
        if (!(req = notecard.newRequest("hub.set")))
	       	return false;

	JAddStringToObject(req, "product", productuid);
	JAddStringToObject(req, "mode", NC_MODE);
	
	if (voutbound)
		JAddStringToObject(req, "voutbound", voutbound);
	if (vinbound)
		JAddStringToObject(req, "vinbound", vinbound);

	// sendRequestWithRetry handles cold-boot race condition with the Notecard
	bool ret = notecard.sendRequestWithRetry(req, 5);

	if (voltage_mode) {
		J *mode_req = notecard.newRequest("card.voltage");
		JAddStringToObject(mode_req, "mode", voltage_mode);
	        ret &= notecard.sendRequest(mode_req);
	}

	return ret;
}

bool nc_template_verify(const char *name) {
	J *req = notecard.newRequest("note.template");
	JAddStringToObject(req, "file", name);
	JAddBoolToObject(req, "verify", true);

	return JGetBool(notecard.requestAndResponse(req), "template");
}

bool nc_template_add(const char *name) {
	J *req = notecard.newRequest("note.template");
	J *body = JAddObjectToObject(req, "body");
	JAddStringToObject(req, "file", name);

	JAddStringToObject(body, "datetime", "15:12:24 17/08/2023");
  JAddNumberToObject(body, "epoch", 24);
  JAddNumberToObject(body, "Mojo1_V", 12.1);
  JAddNumberToObject(body, "Mojo1_C", 12.1);
  JAddNumberToObject(body, "Mojo2_V", 12.1);
  JAddNumberToObject(body, "Mojo2_C", 12.1);
  JAddNumberToObject(body, "MaxL1_V", 12.1);
  JAddNumberToObject(body, "MaxL2_V", 12.1);
  JAddNumberToObject(body, "NC_V", 12.1);
  JAddNumberToObject(body, "NC_T", 12.1);
  
  JAddNumberToObject(body, "1_Co2_Rh", 12.1);
  JAddNumberToObject(body, "1_Co2_temp", 12.1);
  JAddNumberToObject(body, "1_Co2_ppm", 12.1);
  JAddNumberToObject(body, "1_Co2x_Rh", 12.1);
  JAddNumberToObject(body, "1_Co2x_temp", 12.1);
  JAddNumberToObject(body, "1_Co2x_ppm", 12.1);
  JAddNumberToObject(body, "1_O2_Con", 14.1);
  JAddNumberToObject(body, "1_O2_temp", 12.1);
  JAddNumberToObject(body, "1_BMP_Alt", 14.1);
  JAddNumberToObject(body, "1_BMP_Pre", 14.1);
  JAddNumberToObject(body, "1_BMP_Temp", 12.1);
  
  JAddNumberToObject(body, "2_Co2_Rh", 12.1);
  JAddNumberToObject(body, "2_Co2_temp", 12.1);
  JAddNumberToObject(body, "2_Co2_ppm", 12.1);
  JAddNumberToObject(body, "2_Co2x_Rh", 12.1);
  JAddNumberToObject(body, "2_Co2x_temp", 12.1);
  JAddNumberToObject(body, "2_Co2x_ppm", 12.1);
  JAddNumberToObject(body, "2_O2_Con", 14.1);
  JAddNumberToObject(body, "2_O2_temp", 12.1);
  JAddNumberToObject(body, "2_BMP_Alt", 14.1);
  JAddNumberToObject(body, "2_BMP_Pre", 14.1);
  JAddNumberToObject(body, "2_BMP_Temp", 12.1);
  
  JAddNumberToObject(body, "3_Co2_Rh", 12.1);
  JAddNumberToObject(body, "3_Co2_temp", 12.1);
  JAddNumberToObject(body, "3_Co2_ppm", 12.1);
  JAddNumberToObject(body, "3_Co2x_Rh", 12.1);
  JAddNumberToObject(body, "3_Co2x_temp", 12.1);
  JAddNumberToObject(body, "3_Co2x_ppm", 12.1);
  JAddNumberToObject(body, "3_O2_Con", 14.1);
  JAddNumberToObject(body, "3_O2_temp", 12.1);
  JAddNumberToObject(body, "3_BMP_Alt", 14.1);
  JAddNumberToObject(body, "3_BMP_Pre", 14.1);
  JAddNumberToObject(body, "3_BMP_Temp", 12.1);
	Serial.print("built template!,   ");
	return notecard.sendRequest(req);
}

void nc_debug(Stream *s) {
	if (s)
		notecard.setDebugOutputStream(*s);
	else
		notecard.clearDebugOutputStream();
}

static bool nc_motion(bool state) {
	J* req;
        if (! (req = notecard.newRequest("card.motion.mode")))
        	return false;
        JAddBoolToObject(req, "stop", state);
        return notecard.sendRequest(req);
}
bool nc_disable_motion() {
	return nc_motion(true);
}
bool nc_enable_motion() {
	return nc_motion(false);
}

static bool nc_gps(const char* state) {
	J* req;
	if (! (req = NoteNewRequest("card.location.mode")))
		return false;
	JAddStringToObject(req, "mode", state);
        return notecard.sendRequest(req);
}
bool nc_disable_gps() {
	return nc_gps("off");
}
bool nc_enable_gps() {
	return nc_gps("on");
}

unsigned long nc_time() {
	unsigned long linux_epoch;
	J *req, *rsp;
	if ((req = notecard.newRequest("card.time")) &&
		(rsp = notecard.requestAndResponse(req))){
		linux_epoch = JGetNumber(rsp, "time");
		notecard.deleteResponse(rsp);
		return linux_epoch;
	}
	return 0;
}

bool nc_value(const char *type, float *val) {
	J *req, *rsp;
	if ((req = NoteNewRequest(type)) &&
		(rsp = NoteRequestResponse(req))) {
		*val = JGetNumber(rsp, "value");
		notecard.deleteResponse(rsp);
		return true;
	}
	return false;
}

bool nc_temperature(float *temp) {
	return nc_value("card.temp", temp);
}
bool nc_voltage(float *voltage) {
	return nc_value("card.voltage", voltage);
}

bool nc_lipo() {
	J *req = notecard.newRequest("card.voltage");
	JAddStringToObject(req, "mode", "lipo");
	bool ret = notecard.sendRequest(req);
	Serial.println(ret);
	return ret;
}

bool nc_note_add(J *body, const char *file = NULL) {
	J* req;
	if (! (req = notecard.newRequest("note.add")))
		return false;
	JAddStringToObject(req, "file", file);
	JAddItemToObject(req, "body", body);
	return notecard.sendRequest(req);
}

bool nc_hub_sync_now() {
	J* req;
	if(! (req = notecard.newRequest("hub.sync")))
		return false;
	return notecard.sendRequest(req);
}

bool nc_sleep(unsigned long seconds) {
	J* req;
	if (! (req = notecard.newCommand("card.attn")))
		return false;

	JAddStringToObject(req, "mode", "sleep");
	JAddNumberToObject(req, "seconds", seconds);
	return notecard.sendRequest(req);
}

// Read a NoteHub environment variable as bool (returns true if value is "1")
bool nc_env_get_bool(const char *name) {
  J *req = notecard.newRequest("env.get");
  if (req == NULL) return false;
  JAddStringToObject(req, "name", name);
  J *rsp = notecard.requestAndResponse(req);
  bool val = (atoi(JGetString(rsp, "text")) == 1);
  notecard.deleteResponse(rsp);
  return val;
}

// Set a NoteHub environment variable
bool nc_env_set(const char *name, const char *value) {
  J *req = notecard.newRequest("env.set");
  if (req == NULL) return false;
  JAddStringToObject(req, "name", name);
  JAddStringToObject(req, "text", value);
  return notecard.sendRequest(req);
}

// Returns reading_interval from NoteHub environment variables
long nc_interval() {
  long IntervalSeconds = 0;
  J *req = notecard.newRequest("env.get");
  if (req != NULL) {
      JAddStringToObject(req, "name", "reading_interval");
      J* rsp = notecard.requestAndResponse(req);
      int readingIntervalEnvVar = atoi(JGetString(rsp, "text"));
      if (readingIntervalEnvVar > 0) {
        IntervalSeconds = readingIntervalEnvVar;
      }
      notecard.deleteResponse(rsp);
  }
  return IntervalSeconds;
}
#endif //nc_h_