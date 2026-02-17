//Last Update:

//12_02_2026 : replaced strcpy with strncpy for server_names to prevent buffer overflow
//12_02_2026 : server_reads[] values are never zeroed between samples
//12_02_2026 : added SENSOR_TIMEOUT_MS 2000 for sdc30. O2 and SCD4x for a non-blocking code
//11_02_2026 : fixed bug with error with SCD4x , fixed parameter names
//03_02_2026 : added configurable sensor enable/disable switches and node address
//03_02_2026 : added INIR2-CD100 with IMPROVED library v1.1.0
//02_02_2026 : added ExplorIR-M-20 CO2 sensor with library interface 
//15_10_2025 : changed SCD30 library and added stopPeriodicMeasurement() to SCD30 and SCD4x
//08_10_2025 : fixed issue with SCD30 and SCD4x not giving results
//15_09_2025 : added SCD4x sensor
//15_09_2025 : fixed bmp390 pressure issue


// ╔════════════════════════════════════════════════════════════════════╗
// ║                    NODE CONFIGURATION                              ║
// ║                                                                    ║
// ║  Configure your node by enabling/disabling sensors below           ║
// ║  Set to true to ENABLE sensor, false to DISABLE sensor             ║
// ╚════════════════════════════════════════════════════════════════════╝

// Sensor Enable/Disable Switches
#define ENABLE_SCD30    true   // SCD30 CO2 sensor (I2C address 0x61)
#define ENABLE_SCD4X    false   // SCD4x CO2 sensor (I2C address 0x62)
#define ENABLE_O2       true   // O2 sensor (I2C address 0x74)
#define ENABLE_BMP390   true   // BMP390 pressure sensor (I2C)
#define ENABLE_EXPLORIR false   // ExplorIR-M-20 CO2 sensor (Serial2)
#define ENABLE_INIR2    false   // INIR2-CD100 CO2 sensor (Serial1)

// Maximum time (ms) to wait for any single sensor before moving on
// Must be > 5000 to cover SCD4x periodic measurement interval (~5s)
#define SENSOR_TIMEOUT_MS 3000

// ═══════════════════════════════════════════════════════════════════════
//  DO NOT MODIFY BELOW THIS LINE (unless you know what you're doing)
// ═══════════════════════════════════════════════════════════════════════


// Include only necessary libraries based on enabled sensors
#if ENABLE_SCD30
  #include <SensirionI2cScd30.h>
#endif

#if ENABLE_SCD4X
  #include <SensirionI2cScd4x.h>
#endif

#if ENABLE_O2
  #include <DFRobot_MultiGasSensor.h>
#endif

#if ENABLE_BMP390
  #include "Adafruit_BMP3XX.h"
#endif

#if ENABLE_EXPLORIR
  #include <ExplorIR_M20.h>
#endif

#if ENABLE_INIR2
  #include <INIR2.h>
#endif


// Serial2 at pins 8 (Rx), 10 (Tx)
Uart Serial2 (&sercom2, 8, 10, SERCOM_RX_PAD_3, UART_TX_PAD_2);
void SERCOM2_Handler() {
  Serial2.IrqHandler();
}

// Global Sensor Objects & Flags
#if ENABLE_SCD30
  SensirionI2cScd30 Scd30;
  bool SCD30_flag = false;
  static int16_t error;
#endif

#if ENABLE_SCD4X
  SensirionI2cScd4x sensor;
  bool SCD4x_flag = false;
  static int16_t error2;
#endif

#if ENABLE_O2
  #define I2C_ADDRESS 0x74
  DFRobot_GAS_I2C gas(&Wire, I2C_ADDRESS);
  bool O2_flag = false;
#endif

#if ENABLE_BMP390
  Adafruit_BMP3XX bmp;
  #define SEALEVELPRESSURE_HPA (1013.25)
  bool Pressurebmp_flag = false;
#endif

#if ENABLE_EXPLORIR
  ExplorIR_M20 explorIR(Serial2);
  bool ExplorIR_flag = false;
#endif

#if ENABLE_INIR2
  INIR2 inir2Sensor(SENSOR_CD100);
  bool INIR2_flag = false;
  const float VOL_TO_PPM = 10000.0;  // Conversion: 1% vol = 10,000 ppm
#endif


// Timing
unsigned long startTimeSample;
unsigned long durationSample;
unsigned long startTimeServer_setup;
unsigned long durationServer_setup;

// Data arrays
#define PARAMETERS_N 19
float server_reads[PARAMETERS_N] = {0};
bool server_ok[PARAMETERS_N] = {0};
char server_names[PARAMETERS_N][13] = {0};


void server_setup() {
  
  startTimeServer_setup = millis();
  Serial.println("╔════════════════════════════════════════════════╗");
  Serial.print("║  Node Address: ");
  Serial.print(ADDRESS);
  Serial.println("                                  ║");
  Serial.println("║  Starting sensor initialization...             ║");
  Serial.println("╚════════════════════════════════════════════════╝");
  Serial.println();
  
  Wire.begin();
  delay(200);  // allow I2C bus to stabilize
  
  
  // Assign parameter names (using strncpy to prevent buffer overflow)
  #define SENSOR_NAME_MAX (sizeof(server_names[0]) - 1)

  #if ENABLE_SCD30
    strncpy(server_names[0], "Co2_ppm", SENSOR_NAME_MAX);
    strncpy(server_names[1], "Co2_temp", SENSOR_NAME_MAX);
    strncpy(server_names[2], "Co2_Rh", SENSOR_NAME_MAX);
  #endif

  #if ENABLE_O2
    strncpy(server_names[3], "O2_Con", SENSOR_NAME_MAX);
    strncpy(server_names[4], "O2_temp", SENSOR_NAME_MAX);
  #endif

  #if ENABLE_BMP390
    strncpy(server_names[5], "BMP_Pre", SENSOR_NAME_MAX);
    strncpy(server_names[6], "BMP_Temp", SENSOR_NAME_MAX);
    strncpy(server_names[7], "BMP_Alt", SENSOR_NAME_MAX);
  #endif

  #if ENABLE_SCD4X
    strncpy(server_names[8], "Co2x_ppm", SENSOR_NAME_MAX);
    strncpy(server_names[9], "Co2x_temp", SENSOR_NAME_MAX);
    strncpy(server_names[10], "Co2x_Rh", SENSOR_NAME_MAX);
  #endif

  #if ENABLE_EXPLORIR
    strncpy(server_names[11], "ExpIR_filt", SENSOR_NAME_MAX);
    strncpy(server_names[12], "ExpIR_unflt", SENSOR_NAME_MAX);
  #endif

  #if ENABLE_INIR2
    strncpy(server_names[13], "INIR_ppm", SENSOR_NAME_MAX);
    strncpy(server_names[14], "INIR_temp", SENSOR_NAME_MAX);
  #endif
  
  // Initialize only enabled sensors

  #if ENABLE_SCD30
    Serial.print("[SCD30] ");
    Scd30.begin(Wire, SCD30_I2C_ADDR_61);
    error = Scd30.startPeriodicMeasurement(0);
    if (error != 0) {
        Serial.println("Not detected");
    } else {
        Serial.println("✓ Found and initialized");
        SCD30_flag = true;
    }
  #else
    Serial.println("[SCD30] Disabled in configuration");
  #endif

  #if ENABLE_SCD4X
    Serial.print("[SCD4x] ");
    sensor.begin(Wire, SCD41_I2C_ADDR_62);
    delay(30);
    sensor.wakeUp();
    sensor.stopPeriodicMeasurement();
    sensor.reinit();
    delay(1000);
    // Verify sensor is present by reading serial number
    {
        uint64_t serial = 0;
        if (!sensor.getSerialNumber(serial)) {
            SCD4x_flag = true;
            Serial.println("✓ Found (single-shot mode)");
        } else {
            Serial.println("Not detected");
        }
    }
  #else
    Serial.println("[SCD4x] Disabled in configuration");
  #endif
  
  #if ENABLE_O2
    Serial.print("[O2 Sensor] ");
    if (!gas.begin()) {
      Serial.println("Not detected");
    } else {
      O2_flag = true;
      Serial.println("✓ Found and initialized");
      gas.changeAcquireMode(gas.PASSIVITY);
      delay(200);
      gas.setTempCompensation(gas.ON);
    }
  #else
    Serial.println("[O2 Sensor] Disabled in configuration");
  #endif

  #if ENABLE_BMP390
    Serial.print("[BMP390] ");
    if (!bmp.begin_I2C()) {
      Serial.println("Not detected");
      delay(200);
    } else {
      Serial.println("✓ Found and initialized");
      bmp.setTemperatureOversampling(BMP3_OVERSAMPLING_2X);
      bmp.setPressureOversampling(BMP3_OVERSAMPLING_8X);
      bmp.setIIRFilterCoeff(BMP3_IIR_FILTER_COEFF_3);
      bmp.setOutputDataRate(BMP3_ODR_50_HZ);
      Pressurebmp_flag = true;
      bmp.performReading();                                  // discard first measurement
      delay(50); 
    }
  #else
    Serial.println("[BMP390] Disabled in configuration");
  #endif

  #if ENABLE_EXPLORIR
    Serial.print("[ExplorIR-M-20] ");
    if (explorIR.begin()) {
      ExplorIR_flag = true;
      Serial.println("✓ Found and initialized");
    } else {
      Serial.println("Not detected");
    }
  #else
    Serial.println("[ExplorIR-M-20] Disabled in configuration");
  #endif

  #if ENABLE_INIR2
    Serial2.begin(38400, SERIAL_8N2);  // INIR2 requires 8N2 format
    Serial.print("[INIR2-CD100] ");
    if (inir2Sensor.beginSimple(Serial2, 38400)) {                  // Use simple initialization (fast)
      INIR2_flag = true;
      Serial.println("✓ Found and initialized");
    } else {
      Serial.println("Not detected");
      Serial.print("  Error: ");
      Serial.println(inir2Sensor.getErrorString());
    }
  #else
    Serial.println("[INIR2-CD100] Disabled in configuration");
  #endif

  // Print summary
  Serial.println();
  Serial.println("════════════════════════════════════════════════");
  Serial.print("Initialization complete for Node ");
  Serial.println(ADDRESS);
  
  int enabledCount = 0;
  #if ENABLE_SCD30
    if (SCD30_flag) enabledCount++;
  #endif
  #if ENABLE_SCD4X
    if (SCD4x_flag) enabledCount++;
  #endif
  #if ENABLE_O2
    if (O2_flag) enabledCount++;
  #endif
  #if ENABLE_BMP390
    if (Pressurebmp_flag) enabledCount++;
  #endif
  #if ENABLE_EXPLORIR
    if (ExplorIR_flag) enabledCount++;
  #endif
  #if ENABLE_INIR2
    if (INIR2_flag) enabledCount++;
  #endif
  
  Serial.print("Active sensors: ");
  Serial.println(enabledCount);
  Serial.println("════════════════════════════════════════════════");
  Serial.println();
}

void server_loop() {
}


void interact(const char *cmd) {
  Serial.println(cmd);
}


void sample() {
  startTimeSample = millis();

  // Clear all readings and flags at the start of each sample
  // This prevents stale data from a previous cycle being used if a sensor fails
  for (int i = 0; i < PARAMETERS_N; i++) {
      server_reads[i] = 0;
      server_ok[i] = false;
  }

  #if ENABLE_BMP390
    if (Pressurebmp_flag) {
        unsigned long bmpStart = millis();
        bool bmpOk = bmp.performReading();
        unsigned long bmpDuration = millis() - bmpStart;

        if (bmpOk && bmpDuration < SENSOR_TIMEOUT_MS) {
            server_ok[5] = server_ok[6] = server_ok[7] = true;
            server_reads[5] = bmp.pressure / 100.0f;
            server_reads[6] = bmp.temperature;
            server_reads[7] = bmp.readAltitude(SEALEVELPRESSURE_HPA);
        }
    }
  #endif

  #if ENABLE_O2
    if (O2_flag) {
        unsigned long o2Start = millis();
        while (millis() - o2Start < SENSOR_TIMEOUT_MS) {
            float o2 = gas.readGasConcentrationPPM();
            float t  = gas.readTempC();
            if (o2 > 0.1f) {
                server_reads[3] = o2;
                server_reads[4] = t;
                server_ok[3] = server_ok[4] = true;
                break;
            }
            delay(150);
        }
    }
  #endif

  #if ENABLE_SCD30
    if (SCD30_flag) {
        float co2, temp, hum;
        uint16_t dataReady = 0;
        unsigned long scd30Start = millis();
        while (millis() - scd30Start < SENSOR_TIMEOUT_MS) {
            error = Scd30.getDataReady(dataReady);
            if (error == 0 && dataReady) {
                error = Scd30.readMeasurementData(co2, temp, hum);
                if (error == 0 && co2 > 0.1f) {
                    server_reads[0] = co2;
                    server_reads[1] = temp;
                    server_reads[2] = hum;
                    server_ok[0] = server_ok[1] = server_ok[2] = true;
                }
                break;
            }
            delay(100);
        }
    }
  #endif

  #if ENABLE_SCD4X
    if (SCD4x_flag) {
        // Single-shot mode: one clean I2C transaction, no polling loop
        // measureSingleShot triggers a measurement (~5s internal wait)
        // then readMeasurement reads the result
        uint16_t co2;
        float temp, rh;
        error2 = sensor.measureSingleShot();
        if (error2 == 0) {
            error2 = sensor.readMeasurement(co2, temp, rh);
            if (error2 == 0 && co2 > 0) {
                server_reads[8] = co2;
                server_reads[9] = temp;
                server_reads[10] = rh;
                server_ok[8] = server_ok[9] = server_ok[10] = true;
            }
        }
    }
  #endif

  #if ENABLE_EXPLORIR
    if (ExplorIR_flag) {
        #if ENABLE_BMP390
          // Set pressure compensation from BMP390 if available
          if (Pressurebmp_flag && server_ok[5]) {
              explorIR.setPressure(server_reads[5]);
          }
        #endif
        
        if (explorIR.readSensor()) {
            if (explorIR.isCompensationEnabled()) {
                server_reads[11] = explorIR.getCompensatedFilteredCO2();
                server_reads[12] = explorIR.getCompensatedUnfilteredCO2();
            } else {
                server_reads[11] = explorIR.getFilteredCO2();
                server_reads[12] = explorIR.getUnfilteredCO2();
            }
            server_ok[11] = server_ok[12] = true;
        }
    }
  #endif

  #if ENABLE_INIR2
    if (INIR2_flag) {
        INIR2_Data data;
        
        if (inir2Sensor.readData(data)) {
            if (data.errorCode == ERROR_NONE) {
                server_reads[13] = data.concentration * VOL_TO_PPM;
                server_reads[14] = data.temperature;
                server_ok[13] = server_ok[14] = true;
            }
        }
    }
  #endif

  unsigned long endTimeSample = millis();
  durationSample = endTimeSample - startTimeSample;
  Serial.print("void (sample) took: ");
  Serial.print(durationSample);
  Serial.println(" milliseconds");
}
