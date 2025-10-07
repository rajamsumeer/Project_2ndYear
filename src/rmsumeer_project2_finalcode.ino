/*
Student Name: Raja Sumeer
Student ID: B00163482
Module: Project ELTCH2022: 2024-25
Program: Greenhouse Environment Controller

## Component List:
Microcontroller: ATMEGA328P
Sensors: DHT20, DS18B20, LDR
Wifi: ESP8266
RELAY, 12V FAN, BC337, GROVE RGB LCD
RESET BUTTON, SWITCH, JACK CONNECTORS, LEDS, FTDI

## Sources: 
- Brightspace Sample Codes: Fergus Maughan
- LCD Colors: TUD Brand Book

## Honors:
- Nicki Mardari 
- Joeseph Oddi Obong
*/

/* ------------------- LIBRARIES ------------------- */
#include <Wire.h>               // I2C protocol - Used for LCD and DHT20
#include <SoftwareSerial.h>     // ESP8266 communication
#include "rgb_lcd.h"            // GROVE LCD Display library
// Sensor libraries
#include <OneWire.h>            // DS18B20 interface
#include <DallasTemperature.h>  // DS18B20 functions
#include "DHT20.h"

/* ------------------- PIN DEFINITIONS ------------------- */
#define ESP_RX 10               // ESP8266 RX pin
#define ESP_TX 11               // ESP8266 TX pin
#define ONE_WIRE_BUS 7          // DS18B20 data pin
#define LDR_PIN 1               // LDR pin
#define RELAY_PIN 8             // Relay - Fan control
#define POT_PIN A0              // Potentiometer pin
#define LED_PIN 13              // Status LED pin

/* ------------------- CONSTANTS ------------------- */
// ADC and sensor calculation constants
#define MAX_ADC 1023
#define ADC_REF_VOLTAGE 5.03
#define REF_RESISTANCE 10160    // LDR divider resistance
#define LUX_CALC_SCALAR 2355175 // Lux formula scalar
#define LUX_CALC_EXPONENT -1.2109 // Lux formula exponent
// LCD Display colors via TUD Brand Book
#define LCD_RED 182, 0, 87      // Alert color
#define LCD_BLUE 0, 76, 108     // Normal color
// Control constants
#define HYSTERESIS 2            // Prevents relay oscillation

/* ------------------- WIFI & THINGSPEAK ------------------- */
const String API_KEY = "";
const String HOST = "api.thingspeak.com";
const String PORT = "80";
const String WIFI_SSID = "mangojuice";
const String WIFI_PASSWORD = "juice";
SoftwareSerial esp8266(ESP_RX, ESP_TX);

/* ------------------- OBJECTS ------------------- */
// Sensor objects
DHT20 dht;
OneWire oneWire(ONE_WIRE_BUS);
DallasTemperature sensors(&oneWire);
// Display object
rgb_lcd lcd;

// Custom characters
byte DegreesC[] = {B11000, B11011, B00100, B00100, B00100, B00100, B00011, B00000}; // °C symbol
byte LXSymbol[] = {B00100, B10101, B01110, B11111, B01110, B10101, B00100, B00000}; // Lux/"Sun" symbol

/* ------------------- STATE VARIABLES ------------------- */
// Timer variables
unsigned long thingUpdatetime = 0;  // ThingSpeak timer
unsigned long lcdUpdatetime = 0;    // LCD update timer
unsigned long potUpdatetime = 0;    // Pot reading timer
// Sensor readings
float SoilTemp = 0;
float AirTemp = 0;
float Humidity = 0;
float Lux = 0;
// Control variables
int rawValue = 0;               // Raw potentiometer value
int setValue = 0;               // Target temp
int lastSetValue = 999;         // Previous target
uint8_t fanLevel = 0;          // Fan state
// WiFi status
uint8_t wifiGood = 0;

/* ------------------- FUNCTION DECLARATIONS ------------------- */
// Setup functions
void initComponents();
uint8_t initializeWiFi();
bool sendCommand(String command, int maxTime, const char* readReply);

// Loop functions
void potprint();                
void sensorReadings();
float readLux();
void print();
void fan();
void sendToThingSpeak();
void blinkLED();

void setup() 
{
	initComponents();
	delay(100);
	wifiGood = initializeWiFi();
	delay(100);
}

void loop() 
{
	unsigned long currentMillis = millis();
	
	// Check pot every 250ms
	if (currentMillis - potUpdatetime >= 250) 
	{
		rawValue = analogRead(POT_PIN);
		setValue = map(rawValue, 0, 1023, 0, 40); // Convert to temperature range
		if (setValue != lastSetValue) 
		{
			potprint();
		}
		potUpdatetime = currentMillis;
  }
	
	// Update sensors every 1s
	if (currentMillis - lcdUpdatetime >= 1000) 
	{
		sensorReadings();
		lcdUpdatetime = currentMillis;
    }

	// Upload data every 60s
	if (currentMillis - thingUpdatetime >= 60000) 
	{
		sendToThingSpeak();
		thingUpdatetime = currentMillis;
	}

	if (wifiGood == 0)
	{
		initializeWiFi();
	}
	
	blinkLED();
}

/* ------------------- FUNCTIONS DEFINITONS ------------------- */

/* Initialize Components */
void initComponents()
{
	Serial.begin(9600);
	esp8266.begin(9600);

	pinMode(RELAY_PIN, OUTPUT);
	digitalWrite(RELAY_PIN, LOW);  // Ensure fan is off
	pinMode(LED_PIN, OUTPUT);

	lcd.begin(16, 2);
	lcd.setRGB(LCD_BLUE);
	Wire.begin();
	lcd.createChar(0, DegreesC);
	lcd.createChar(1, LXSymbol);

	dht.begin();
  delay(100);
	sensors.begin();

	// Display welcome message
	lcd.setCursor(0, 0); lcd.print("Environment");
	lcd.setCursor(0, 1); lcd.print("Controller"); delay(2500); lcd.clear();
}

/* WiFi Initialization */
uint8_t initializeWiFi() {
	bool success = true;
	
	if (success &= sendCommand("AT", 5, "OK"))  // Test ESP if alive for response
	{
		delay(200);
		if (success &= sendCommand("AT+RST", 5, "OK")) // Reset ESP
		{
			delay(200);
			if (success &= sendCommand("AT+CWMODE=1", 5, "OK"))  // Station mode
			{
				delay(200);
				if (success &= sendCommand("AT+CWJAP=\"" + WIFI_SSID + "\",\"" + WIFI_PASSWORD + "\"", 20, "OK"))  // Connect to Wifi
				{
					delay(200);
					if (success &= sendCommand("AT+CIPMUX=1", 5, "OK"))  // Multiple connections
					{
						delay(200);
						if (success &= sendCommand("AT+CIPSTART=0,\"TCP\",\"" + HOST + "\"," + PORT, 15, "OK"))  // Start TCP
						{
							delay(200);
							return 1;
						}
						if (success &= sendCommand("AT+CIPSTART=0,\"TCP\",\"" + HOST + "\"," + PORT, 15, "ALREADY CONNECT"))
						{
							delay(200);
							return 1;
						}
					}
				}
			}
		}
	}
	Serial.println("== Failed Wifi Initialization ==");
	return 0;
}

/* Send ESP8266 Command */
bool sendCommand(String command, int maxTime, const char* readReply)
{
  Serial.print("[ESP] Sending Command: ");
  Serial.println(command);
  bool found = false;
  
  int countTimeCommand = 0;
  while (countTimeCommand < maxTime) 
  {
	esp8266.println(command);
	// if (esp8266.find(readReply))
  if (esp8266.find((char*)readReply))
	{
		found = true;
		break;
	}
	countTimeCommand++;
	delay(1000);
  }
  return found;
}

/* Potentiometer Display Function */
void potprint()
{
	lcd.clear();
	lcd.setCursor(0,0);
	lcd.print("Pot Set: ");
	lcd.print(setValue);
	lcd.write(byte(0));  // Degrees symbol
	lastSetValue = setValue;
}

/* Read All Sensors */
void sensorReadings()
{
	sensors.requestTemperatures();
  dht.read();
	SoilTemp = sensors.getTempCByIndex(0);
	AirTemp = dht.getTemperature();
	Humidity = dht.getHumidity();
	Lux = readLux();
	print();
	fan();
}

/* Calculate Light Level */
float readLux() {
  int ldrRawData = analogRead(LDR_PIN);
  float resistorVoltage = (float)ldrRawData / MAX_ADC * ADC_REF_VOLTAGE;
  float ldrVoltage = ADC_REF_VOLTAGE - resistorVoltage;
  float ldrResistance = ldrVoltage / resistorVoltage * REF_RESISTANCE;
  float ldrLux = LUX_CALC_SCALAR * pow(ldrResistance, LUX_CALC_EXPONENT);
  return ldrLux;
}

/* Display Readings */
void print()
{
	// Serial output
	Serial.println("\n===== SENSOR DATA =====");
	Serial.print("Potentiometer: "); Serial.print(rawValue); Serial.print(" | ");
	Serial.print(setValue); Serial.println(" °C");
	Serial.print("Soil Temperature: "); Serial.print(SoilTemp); Serial.println(" °C");
	Serial.print("Air Temperature: "); Serial.print(AirTemp); Serial.println(" °C");
	Serial.print("Humidity: "); Serial.print(Humidity); Serial.println(" %");
	Serial.print("Light Intensity: "); Serial.print(Lux); Serial.println(" lux");
	Serial.print("Fan Status: "); Serial.println(fanLevel ? "ON" : "OFF");

	// LCD display
	lcd.setCursor(0, 0);
	lcd.print("H:"); lcd.print(Humidity, 1); lcd.print("%");
	lcd.setCursor(9, 0);
	lcd.print("A:"); lcd.print(AirTemp, 1); lcd.write(byte(0));
	lcd.setCursor(0, 1);
	lcd.print("L:"); lcd.print(Lux, 1); lcd.write(byte(1)); lcd.print(" ");
	lcd.setCursor(9, 1);
	lcd.print("S:"); lcd.print(SoilTemp, 1); lcd.write(byte(0));
}

/* Fan Control */
void fan()
{
	// Turn on fan if too hot
	if (fanLevel == 0 && AirTemp > setValue)
	{
		digitalWrite(RELAY_PIN, HIGH);
		lcd.setRGB(LCD_RED);
		fanLevel = 1;
		Serial.println("[FAN] Fan Switched ON - Threshold Exceeded.");
	}
	// Turn off fan when cool enough with hysteresis
	if(fanLevel == 1 && AirTemp <= setValue - HYSTERESIS)
	{
		digitalWrite(RELAY_PIN, LOW);
		lcd.setRGB(LCD_BLUE);
		fanLevel = 0;
		Serial.println("[FAN] Fan Switched OFF - Temperature Normal.");
    }	
}

/* Send Data to ThingSpeak */
void sendToThingSpeak() 
{
    if (wifiGood == 1)
    {
        bool success = true;
        
        // Build data string
        String data;
        data.reserve(150);  // Pre-allocate memory
        
        data = "GET /update?api_key=";
        data += API_KEY;
        data += "&field1=";
        data += String(SoilTemp,1);
        data += "&field2=";
        data += String(AirTemp,1);
        data += "&field3=";
        data += String(Humidity,1);
        data += "&field4=";
        data += String(Lux);
				data += "&field5=";
        data += String(fanLevel);
        data += "\r\n\r\n";

				// Re-open TCP connection before each send
        if (!(sendCommand("AT+CIPSTART=0,\"TCP\",\"" + HOST + "\"," + PORT, 15, "OK") ||
          sendCommand("AT+CIPSTART=0,\"TCP\",\"" + HOST + "\"," + PORT, 15, "ALREADY CONNECT"))) {
          Serial.println("== Failed to reconnect to ThingSpeak. ==");
          wifiGood = 0;
          return;
        }

        delay(300);  // Give some time before sending

        String cipCommand = "AT+CIPSEND=0,";
        cipCommand += String(data.length()+4);
        
        if (success &= sendCommand(cipCommand, 10, ">"))
        {
            delay(300);
            esp8266.print(data);
            delay(1500);
						Serial.println("== Successfully updated data for Thingspeak. ==");
            if (success &= sendCommand("AT+CIPCLOSE=0", 5, "OK"))
            {
                delay(100);
                return;
            }
        }
        Serial.println("== Cipsend failed, Cancelling Thingspeak update. ==");
        return;
    }
    Serial.println("== Wifi not connected. Unable to update Thingspeak. ==");
    return;
}

/* Status LED Function */
void blinkLED() {
  digitalWrite(LED_PIN, HIGH);
  delay(100);
  digitalWrite(LED_PIN, LOW);
  delay(100);
}