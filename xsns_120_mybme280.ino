// Conditional compilation of driver
#ifdef USE_MYBME280

// Define driver ID
#define XSNS_120             120
#define XI2C_10              10  // See I2CDEVICES.md

#define BMP_ADDR1            0x76
#define BMP_ADDR2            0x77

#define BME280_CHIPID        0x60

#define BMP_REGISTER_CHIPID  0xD0

#define BMP_REGISTER_RESET   0xE0  // Register to reset to power on defaults (used for sleep)

#define BMP_CMND_RESET       0xB6  // I2C Parameter for RESET to put BMP into reset state

#define BMP_MAX_SENSORS    2

//Struct for storing sensor attributes/data
typedef struct {
  uint8_t bmp_address;    // I2C address
  uint8_t bmp_bus;        // I2C bus
  char bmp_name[7];       
  uint8_t bmp_type;
  uint8_t bmp_model;
  float bmp_temperature;
  float bmp_pressure;
  float bmp_humidity;
} bmp_sens_t;

#ifdef USE_DEEPSLEEP
uint8_t bmp_dsleep = 0;  // Prevent updating measurments once BMP has been put to sleep (just before ESP enters deepsleep)
#endif

bmp_sens_t *bmp_sens = nullptr;
uint8_t secondCounter = 0;

//https://www.bosch-sensortec.com/media/boschsensortec/downloads/datasheets/bst-bme280-ds002.pdf
//Registers for BME280 sensor
#define BME280_CONTROLHUMID  0xF2
#define BME280_CONTROL       0xF4
#define BME280_CONFIG        0xF5
#define BME280_PRESSUREDATA  0xF7
#define BME280_TEMPDATA      0xFA
#define BME280_HUMIDDATA     0xFD

#define BME280_DIG_T1        0x88
#define BME280_DIG_T2        0x8A
#define BME280_DIG_T3        0x8C
#define BME280_DIG_P1        0x8E
#define BME280_DIG_P2        0x90
#define BME280_DIG_P3        0x92
#define BME280_DIG_P4        0x94
#define BME280_DIG_P5        0x96
#define BME280_DIG_P6        0x98
#define BME280_DIG_P7        0x9A
#define BME280_DIG_P8        0x9C
#define BME280_DIG_P9        0x9E
#define BME280_DIG_H1        0xA1
#define BME280_DIG_H2        0xE1
#define BME280_DIG_H3        0xE3
#define BME280_DIG_H4        0xE4
#define BME280_DIG_H5        0xE5
#define BME280_DIG_H6        0xE7

typedef struct {
  uint16_t dig_T1;
  int16_t  dig_T2;
  int16_t  dig_T3;
  uint16_t dig_P1;
  int16_t  dig_P2;
  int16_t  dig_P3;
  int16_t  dig_P4;
  int16_t  dig_P5;
  int16_t  dig_P6;
  int16_t  dig_P7;
  int16_t  dig_P8;
  int16_t  dig_P9;
  int16_t  dig_H2;
  int16_t  dig_H4;
  int16_t  dig_H5;
  uint8_t  dig_H1;
  uint8_t  dig_H3;
  int8_t   dig_H6;
} Bme280CaliData_t;

Bme280CaliData_t *Bme280CaliData = nullptr;

//Function saves the calibration data from the registers provided in the datasheet
//THe function takes no parameters and returns true or false depening on calibration success
bool Bme280Calibrate(void) {
  if (!Bme280CaliData) {
    Bme280CaliData = (Bme280CaliData_t*)malloc(sizeof(Bme280CaliData_t));
  }
  if (!Bme280CaliData) { return false; }

  uint8_t address = bmp_sens->bmp_address;
  uint8_t bus = bmp_sens->bmp_bus;
  Bme280CaliData->dig_T1 = I2cRead16LE(address, BME280_DIG_T1, bus);
  Bme280CaliData->dig_T2 = I2cReadS16_LE(address, BME280_DIG_T2, bus);
  Bme280CaliData->dig_T3 = I2cReadS16_LE(address, BME280_DIG_T3, bus);
  Bme280CaliData->dig_P1 = I2cRead16LE(address, BME280_DIG_P1, bus);
  Bme280CaliData->dig_P2 = I2cReadS16_LE(address, BME280_DIG_P2, bus);
  Bme280CaliData->dig_P3 = I2cReadS16_LE(address, BME280_DIG_P3, bus);
  Bme280CaliData->dig_P4 = I2cReadS16_LE(address, BME280_DIG_P4, bus);
  Bme280CaliData->dig_P5 = I2cReadS16_LE(address, BME280_DIG_P5, bus);
  Bme280CaliData->dig_P6 = I2cReadS16_LE(address, BME280_DIG_P6, bus);
  Bme280CaliData->dig_P7 = I2cReadS16_LE(address, BME280_DIG_P7, bus);
  Bme280CaliData->dig_P8 = I2cReadS16_LE(address, BME280_DIG_P8, bus);
  Bme280CaliData->dig_P9 = I2cReadS16_LE(address, BME280_DIG_P9, bus);
  if (BME280_CHIPID == bmp_sens->bmp_type) {  // #1051
    Bme280CaliData->dig_H1 = I2cRead8(address, BME280_DIG_H1, bus);
    Bme280CaliData->dig_H2 = I2cReadS16_LE(address, BME280_DIG_H2, bus);
    Bme280CaliData->dig_H3 = I2cRead8(address, BME280_DIG_H3, bus);
    Bme280CaliData->dig_H4 = (I2cRead8(address, BME280_DIG_H4, bus) << 4) | (I2cRead8(address, BME280_DIG_H4 + 1, bus) & 0xF);
    Bme280CaliData->dig_H5 = (I2cRead8(address, BME280_DIG_H5 + 1, bus) << 4) | (I2cRead8(address, BME280_DIG_H5, bus) >> 4);
    Bme280CaliData->dig_H6 = (int8_t)I2cRead8(address, BME280_DIG_H6, bus);
    I2cWrite8(address, BME280_CONTROL, 0x00, bus);      // sleep mode since writes to config can be ignored in normal mode (Datasheet 5.4.5/6 page 27)
    // Set before CONTROL_meas (DS 5.4.3)
    I2cWrite8(address, BME280_CONTROLHUMID, 0x01, bus); // 1x oversampling
    I2cWrite8(address, BME280_CONFIG, 0xA0, bus);       // 1sec standby between measurements (to limit self heating), IIR filter off
    I2cWrite8(address, BME280_CONTROL, 0x27, bus);      // 1x oversampling, normal mode
  } else {
    I2cWrite8(address, BME280_CONTROL, 0xB7, bus);      // 16x oversampling, normal mode (Adafruit)
  }
  return true;
}

//Function to read data from the BME280 sensor. Reads once every 10 seconds
//The read data is calibrated using the previously saved register values
//Takes no parameters.
void Bme280R(void) {
  if (!Bme280CaliData) { return; }
  secondCounter = (secondCounter + 1)%10;

  if(secondCounter == 9){
    uint8_t address = bmp_sens->bmp_address;
    uint8_t bus = bmp_sens->bmp_bus;

    uint8_t mode = I2cRead8(address, BME280_CONTROL, bus);
    //Log if device is active or sleeping
    AddLog(LOG_LEVEL_INFO, "BME280 mode: %02X", mode);

    int32_t adc_T = I2cRead24(address, BME280_TEMPDATA, bus);
    adc_T >>= 4;

    //Forumla from the BME280 datasheet
    int32_t vart1 = ((((adc_T >> 3) - ((int32_t)Bme280CaliData->dig_T1 << 1))) * ((int32_t)Bme280CaliData->dig_T2)) >> 11;
    int32_t vart2 = (((((adc_T >> 4) - ((int32_t)Bme280CaliData->dig_T1)) * ((adc_T >> 4) - ((int32_t)Bme280CaliData->dig_T1))) >> 12) *
      ((int32_t)Bme280CaliData->dig_T3)) >> 14;
    int32_t t_fine = vart1 + vart2;
    float T = (t_fine * 5 + 128) >> 8;
    bmp_sens->bmp_temperature = T / 100.0f;

    int32_t adc_P = I2cRead24(address, BME280_PRESSUREDATA, bus);
    adc_P >>= 4;

    int64_t var1 = ((int64_t)t_fine) - 128000;
    int64_t var2 = var1 * var1 * (int64_t)Bme280CaliData->dig_P6;
    var2 = var2 + ((var1 * (int64_t)Bme280CaliData->dig_P5) << 17);
    var2 = var2 + (((int64_t)Bme280CaliData->dig_P4) << 35);
    var1 = ((var1 * var1 * (int64_t)Bme280CaliData->dig_P3) >> 8) + ((var1 * (int64_t)Bme280CaliData->dig_P2) << 12);
    var1 = (((((int64_t)1) << 47) + var1)) * ((int64_t)Bme280CaliData->dig_P1) >> 33;
    if (0 == var1) {
      return; // avoid exception caused by division by zero
    }
    int64_t p = 1048576 - adc_P;
    p = (((p << 31) - var2) * 3125) / var1;
    var1 = (((int64_t)Bme280CaliData->dig_P9) * (p >> 13) * (p >> 13)) >> 25;
    var2 = (((int64_t)Bme280CaliData->dig_P8) * p) >> 19;
    p = ((p + var1 + var2) >> 8) + (((int64_t)Bme280CaliData->dig_P7) << 4);
    bmp_sens->bmp_pressure = (float)p / 25600.0f;

    int32_t adc_H = I2cRead16(address, BME280_HUMIDDATA, bus);

    int32_t v_x1_u32r = (t_fine - ((int32_t)76800));
    v_x1_u32r = (((((adc_H << 14) - (((int32_t)Bme280CaliData->dig_H4) << 20) -
      (((int32_t)Bme280CaliData->dig_H5) * v_x1_u32r)) + ((int32_t)16384)) >> 15) *
      (((((((v_x1_u32r * ((int32_t)Bme280CaliData->dig_H6)) >> 10) *
      (((v_x1_u32r * ((int32_t)Bme280CaliData->dig_H3)) >> 11) + ((int32_t)32768))) >> 10) +
      ((int32_t)2097152)) * ((int32_t)Bme280CaliData->dig_H2) + 8192) >> 14));
    v_x1_u32r = (v_x1_u32r - (((((v_x1_u32r >> 15) * (v_x1_u32r >> 15)) >> 7) *
      ((int32_t)Bme280CaliData->dig_H1)) >> 4));
    v_x1_u32r = (v_x1_u32r < 0) ? 0 : v_x1_u32r;
    v_x1_u32r = (v_x1_u32r > 419430400) ? 419430400 : v_x1_u32r;
    float h = (v_x1_u32r >> 12);
    bmp_sens->bmp_humidity = h / 1024.0f;
  }  
}

//Set up sensor I2C bus.
//Function takes no arguments.
void BmpD(void) {
    if (!bmp_sens) {
        bmp_sens = (bmp_sens_t*)calloc(1, sizeof(bmp_sens_t)); // just 1 for BME280
    }
    if (!bmp_sens) { return; }

    uint8_t bus = 0; 
    if (!I2cSetDevice(BMP_ADDR1, bus)) { return; }//Save to 0x76(BME280 I2C)

    uint8_t bmp_type = I2cRead8(BMP_ADDR1, BMP_REGISTER_CHIPID, bus);

    //BME280 found
    if (bmp_type) {
        bmp_sens->bmp_address = BMP_ADDR1;
        bmp_sens->bmp_bus = bus;
        bmp_sens->bmp_type = bmp_type;
        bmp_sens->bmp_model = 0;
        

        bool success = false;
        
        bmp_sens->bmp_model = 2;  // 2 for Bme280
        success = Bme280Calibrate();

        if (success) {

          strlcpy(bmp_sens->bmp_name, "BME28", sizeof(bmp_sens->bmp_name));
          I2cSetActiveFound(bmp_sens->bmp_address, bmp_sens->bmp_name, bmp_sens->bmp_bus);
        }
    }
}

//Sends sensor data to webserver and MQTT broker. Handles data conversions for messaging
//Takes a boolean as an argument for broadcasting MQTT messages 
void BmpS(bool json) {
    if (bmp_sens->bmp_type) {
      //Pressure and temp
      float bmp_sealevel = ConvertPressureForSeaLevel(bmp_sens->bmp_pressure);
      float bmp_temperature = ConvertTemp(bmp_sens->bmp_temperature);
      float bmp_pressure = ConvertPressure(bmp_sens->bmp_pressure);

      char name[16];
      strlcpy(name, bmp_sens->bmp_name, sizeof(bmp_sens->bmp_name));  

      char pressure[33];
      dtostrfd(bmp_pressure, Settings->flag2.pressure_resolution, pressure);
      char sea_pressure[33];
      dtostrfd(bmp_sealevel, Settings->flag2.pressure_resolution, sea_pressure);

      //Humidity
      float bmp_humidity = ConvertHumidity(bmp_sens->bmp_humidity);
      char humidity[33];
      dtostrfd(bmp_humidity, Settings->flag2.humidity_resolution, humidity);
      float f_dewpoint = CalcTempHumToDew(bmp_temperature, bmp_humidity);
      char dewpoint[33];
      dtostrfd(f_dewpoint, Settings->flag2.temperature_resolution, dewpoint);

      //If broadcast MQTT
      if (json) {
        char json_humidity[100];
        snprintf_P(json_humidity, sizeof(json_humidity), PSTR(",\"" D_JSON_HUMIDITY "\":%s,\"" D_JSON_DEWPOINT "\":%s"), humidity, dewpoint);
        char json_sealevel[40];
        snprintf_P(json_sealevel, sizeof(json_sealevel), PSTR(",\"" D_JSON_PRESSUREATSEALEVEL "\":%s"), sea_pressure);
        ResponseAppend_P(PSTR(",\"%s\":{\"" D_JSON_TEMPERATURE "\":%*_f%s,\"" D_JSON_PRESSURE "\":%s%s}"),
           name, Settings->flag2.temperature_resolution, &bmp_temperature, (bmp_sens->bmp_model >= 2) ? json_humidity : "", pressure, (Settings->altitude != 0) ? json_sealevel : "");

#ifdef USE_WEBSERVER//If not MQTT and there is a webserver
      } else {
        WSContentSend_Temp(name, bmp_temperature);
        if (bmp_sens->bmp_model >= 2) {
          WSContentSend_PD(HTTP_SNS_HUM, name, humidity);
          WSContentSend_PD(HTTP_SNS_DEW, name, dewpoint, TempUnit());
        }
        WSContentSend_PD(HTTP_SNS_PRESSURE, name, pressure, PressureUnit().c_str());
        if (Settings->altitude != 0) {
          WSContentSend_PD(HTTP_SNS_SEAPRESSURE, name, sea_pressure, PressureUnit().c_str());
        }

#endif  // USE_WEBSERVER
      }
    }
  
}

//Function calls BME280 read if not asleep. Skips otherwise
void BmpR(void) {
#ifdef USE_DEEPSLEEP
  // Prevent updating measurments once BMP has been put to sleep (just before ESP enters deepsleep)
  if (bmp_dsleep) return;
#endif

  Bme280R();
}

#ifdef USE_DEEPSLEEP
//Function for putting device to sleep
void BMP_Sleep(void) {
  if (DeepSleepEnabled()) {
    switch (bmp_sens->bmp_type) {
        case BME280_CHIPID:
          I2cWrite8(bmp_sens->bmp_address, BMP_REGISTER_RESET, BMP_CMND_RESET, bmp_sens->bmp_bus);
          break;
        default:
          break;
    }
  }
}
#endif //USE_DEEPSLEEP

//Main driver function
bool Xsns120(uint32_t callback_id) {
  // Set return value to `false`
    bool result = false;

  // Check if I2C interface mode
    if (!I2cEnabled(XI2C_10)) { return false; }

  // Check which callback ID is called by Tasmota
    if (FUNC_INIT == callback_id) {
        BmpD();
    }
    else {
      switch (callback_id) {
        case FUNC_EVERY_SECOND:
            BmpR();
            break;
        case FUNC_JSON_APPEND:
            BmpS(1);
            break;
#ifdef USE_WEBSERVER
        case FUNC_WEB_SENSOR:
            BmpS(0);
            break;
#endif // USE_WEBSERVER
#ifdef USE_DEEPSLEEP
        case FUNC_SAVE_BEFORE_RESTART:
            BMP_Sleep();
            bmp_dsleep = 1;
            break;
#endif // USE_DEEPSLEEP
      }
    }

  // Return boolean result
  return result;
}
#endif // USE_MYBME280