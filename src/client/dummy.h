#ifndef DUMMY_H_
#define DUMMY_H_

void client_setup() {
}


bool client_check() {
	return true;
}
void client_add(char *key, float value) {
	Serial.print(key);
	Serial.print(':');
	Serial.println(value);
}
void client_send() {
}

void client_loop() {
}

#endif