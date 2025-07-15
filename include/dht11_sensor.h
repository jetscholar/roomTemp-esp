#ifndef DHT11_SENSOR_H
#define DHT11_SENSOR_H

#include <DHT.h>

#define DHTPIN  D5         // D4 on NodeMCU = GPIO 2
#define DHTTYPE DHT11     // Define as DHT11

DHT dht(DHTPIN, DHTTYPE);

bool readDHTData(float &temperature, float &humidity) {
    temperature = dht.readTemperature();
    humidity = dht.readHumidity();
    return !isnan(temperature) && !isnan(humidity);
}

#endif

