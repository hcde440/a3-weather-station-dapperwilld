//Libraries to include
#include <ESP8266WiFi.h>
#include <PubSubClient.h>
#include <Wire.h>
#include <ArduinoJson.h>            //
#include <Adafruit_Sensor.h>        //
#include <Adafruit_TSL2591.h>       //
#include <Adafruit_MPL115A2.h>      //  
#include <Adafruit_GFX.h>           //  
#include <Adafruit_SSD1306.h>    /////  

//Some immutable definitions/constants
#define ssid "University of Washington"
#define pass ""
#define mqtt_server "mediatedspaces.net"
#define mqtt_name "hcdeiot"
#define mqtt_pass "esp8266"

#define SCREEN_WIDTH 128 // OLED display width, in pixels
#define SCREEN_HEIGHT 32 // OLED display height, in pixels

// Declaration for an SSD1306 display connected to I2C (SDA, SCL pins)
#define OLED_RESET     -1 // Reset pin # (or -1 if sharing Arduino reset pin)
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

Adafruit_MPL115A2 mpl115a2; // defines the mp115a2 sensor object

//Some objects to instantiate
Adafruit_TSL2591 tsl = Adafruit_TSL2591(2591); // pass in a number for the sensor identifier (for your use later)
WiFiClient espClient;
PubSubClient mqtt(espClient);

//Global variables
const int buttonPin = 16;
const int ledPin = 13;
int buttonState = 1;
unsigned long previousMillis;
char mac[18] = "ESP8602";
boolean buttonFlag = false;

//function to configure the light sensor prior to use
void configureSensor(void)
{
  // You can change the gain on the fly, to adapt to brighter/dimmer light situations
  //tsl.setGain(TSL2591_GAIN_LOW);    // 1x gain (bright light)
  tsl.setGain(TSL2591_GAIN_MED);      // 25x gain
  //tsl.setGain(TSL2591_GAIN_HIGH);   // 428x gain
  
  // Changing the integration time gives you a longer time over which to sense light
  // longer timelines are slower, but are good in very low light situtations!
  //tsl.setTiming(TSL2591_INTEGRATIONTIME_100MS);  // shortest integration time (bright light)
  // tsl.setTiming(TSL2591_INTEGRATIONTIME_200MS);
  tsl.setTiming(TSL2591_INTEGRATIONTIME_300MS);
  // tsl.setTiming(TSL2591_INTEGRATIONTIME_400MS);
  // tsl.setTiming(TSL2591_INTEGRATIONTIME_500MS);
  // tsl.setTiming(TSL2591_INTEGRATIONTIME_600MS);

}

//function to do something when a message arrives from mqtt server
void callback(char* topic, byte* payload, unsigned int length) {
  Serial.print("Message arrived [");
  Serial.print(topic);
  Serial.print("] ");
  for (int i = 0; i < length; i++) {
    Serial.print((char)payload[i]);
  }
  Serial.println();
}

//function to reconnect if we become disconnected from the server
void reconnect() {
  // Loop until we're reconnected
  while (!mqtt.connected()) {
    Serial.print("Attempting MQTT connection...");
    if (mqtt.connect(mac, mqtt_name, mqtt_pass)) { //<<---using MAC as client ID, always unique!!!
      Serial.println("connected");
      mqtt.subscribe("will/+"); //we are subscribing to 'theTopic' and all subtopics below that topic
    } else {                        //please change 'theTopic' to reflect your topic you are subscribing to
      Serial.print("failed, rc=");
      Serial.print(mqtt.state());
      Serial.println(" try again in 5 seconds");
      // Wait 5 seconds before retrying
      delay(5000);
    }
  }
}

//////////////////////////////////////SETUP
void setup()
{
  Serial.begin(115200);//for debugging code, comment out for production
  pinMode(buttonPin, INPUT);
  pinMode(ledPin, OUTPUT);

    // wait for serial monitor to open
  while (! Serial);
    // Checks whether display is connected through I2C
    if(!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) { // Address 0x3C for 128x32
    Serial.println(F("SSD1306 allocation failed"));
    for(;;); // Don't proceed, loop forever
  }

  // Show initial display buffer contents on the screen --
  // the library initializes this with an Adafruit splash screen.
  display.display();
  delay(2000); // Pause for 2 seconds
  // Clear the buffer
  display.clearDisplay();

  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, pass);

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }

  Serial.println(); Serial.println("WiFi connected"); Serial.println();
  Serial.print("Your ESP has been assigned the internal IP address ");
  Serial.println(WiFi.localIP());
  WiFi.macAddress().toCharArray(mac,18);

  /* Initialise the sensor */
  if (!tsl.begin())
  {
    /* There was a problem detecting the TSL2561 ... check your connections */
    Serial.print("Ooops, no TSL2561 detected ... Check your wiring or I2C ADDR!");
    while (1);
  }
  // initializes mp1115a2 sensor object
  mpl115a2.begin();
  // calls configure function, initializes TSL2591
  configureSensor();
  //connects to MQTT server
  mqtt.setServer(mqtt_server, 1883);
  mqtt.setCallback(callback);
}

////////////////////////////LOOP
void loop() {
  unsigned long currentMillis = millis();

  mqtt.loop();
  if (!mqtt.connected()) {
    reconnect();
  }

  buttonState = digitalRead(buttonPin);

  if (buttonState == HIGH && buttonFlag == false) {
    Serial.println("Motion detected");
    digitalWrite(ledPin, HIGH);
    char message[20];
    sprintf(message, "{\"uuid\": \"%s\", \"button\": \"%s\" }", mac, "1");
    mqtt.publish("will/A3", message);
    buttonFlag = true;
  }
  if (buttonState == LOW && buttonFlag == true) {
    Serial.println("Area is clear.");
    digitalWrite(ledPin, LOW);
    char message[20];
    sprintf(message, "{\"uuid\": \"%s\", \"button\": \"%s\" }", mac, "0");
    mqtt.publish("will/A3", message);
    buttonFlag = false;
  }
  
  if (currentMillis - previousMillis > 5000) { //a periodic report, every 5 seconds
    sensors_event_t event;
    tsl.getEvent(&event);
    int celsius = mpl115a2.getTemperature(); // assigns temperature to celsius variable
    int fahrenheit = (celsius * 1.8) + 32; // celsius to fahrenheit conversion
    float light = event.light;
    float pressure = mpl115a2.getPressure();

    char str_press[6];    
    char str_light[7];
    char message[50];         // 50 length char array for whole MQTT message


    dtostrf(pressure, 4, 2, str_press);   // Convert's float readings to char arrays and stores them in
    dtostrf(light, 4, 2, str_light);     // variables
    //Serial.println(temperature); //debug line to check content of char array variables

    sprintf(message, "{\"mac\": \"%s\", \"temp\": \"%i F\", \"pressure\": \"%s kPa\", \"light\": \"%s lux\" }", mac, fahrenheit, str_press, str_light); //sprintf function concatinating all readings into the MQTT message
    Serial.println("publishing");// Serial message indicating publishing
    mqtt.publish("will/A3", message);//MQTT publish to the server
    //Serial.println(message);//debug line to check content of message char array
    previousMillis = currentMillis;// reset timer
    outputReading(light, pressure, fahrenheit, buttonFlag); //Output to OLED
  }

 
}

void outputReading(float light, float pressure, int temperature, boolean button) {
  display.clearDisplay();

  display.setTextSize(1);      // Normal 1:1 pixel scale
  display.setTextColor(WHITE); // Draw white text
  display.setCursor(0, 0);     // Start at top-left corner
  display.cp437(true);         // Use full 256 char 'Code Page 437' font

  display.print("light = ");
  display.println(light);      // prints humidity reading to display
  display.print("pressure = ");
  display.println(pressure);      // prints pressure reading to display
  display.print("temperature = ");
  display.println(temperature);   // prints temperature reading to display
  display.print("button state = ");
  display.println(button);   // prints temperature reading to display
  
  display.display();
  
}
