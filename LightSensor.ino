#include <Wire.h>
#include "TSl2581.h"
#include "DHTesp.h"
#include "BluetoothSerial.h"

#define LIGHT_SENSOR_PIN 13
#define FAN_RELAY_PIN 22
#define LED_RELAY_PIN 23
#define LED_BUILTIN 2
#define DOOR_PIN 33
#define TEMP_INDOOR 32
#define TEMP_OUTDOOR 21
#define BUTTON_PIN 15

BluetoothSerial ESP_BT; //Object for Bluetooth
DHTesp indoor;
DHTesp outdoor;

int incoming_bt;
WaveShare_TSL2581 tsl = WaveShare_TSL2581();
unsigned long time_actual = 0;
unsigned long time_diff = 0;
unsigned long time_saved = 0;//3600000UL;
unsigned long fan_time_expected = 10000UL;
unsigned long door_time_expected = 10000UL;
bool led_relay_control = true;
bool fan_relay_control = true;
bool indoor_fan_state = false;
bool light_state = false;
bool bottom_state = false;
int lux_limiter = 30;
int doorValue;
int buttonValue;

void read_id(void)
{
  int id;
  int a;
  id = tsl.TSL2581_Read_ID();
  a = id & 0xf0;      //The lower four bits are the silicon version number
  if ((a == 144))    //ID = 90H = 144D
  {
    delay(500);
  }
}

void Read_gpio_interrupt(uint16_t mindata, uint16_t maxdata)
{
  tsl.SET_Interrupt_Threshold(mindata, maxdata);
  int val = digitalRead(LIGHT_SENSOR_PIN);
  if (val == 1)
  {
    //Serial.print("interrupt = 1 \n");
  } else {
    //Serial.print("interrupt = 0 \n");
    tsl.Reload_register();
  }
}
void Turn_on_Light(){
  if(!bottom_state){
  digitalWrite(LED_RELAY_PIN, HIGH);
  light_state = true;
  }
}
void Turn_off_Light(){
  if(!bottom_state){
  digitalWrite(LED_RELAY_PIN, LOW);
  light_state = false;
  }
}
void Change_Light(){
  if(light_state){
    Turn_off_Light();
  }else{
    Turn_on_Light();
  }
}

void setup(void)
{
  Serial.begin(115200);
  ESP_BT.begin("Garage");  //Name of your Bluetooth Signal
  indoor.setup(TEMP_INDOOR, DHTesp::DHT22);
  outdoor.setup(TEMP_OUTDOOR, DHTesp::DHT22);
  
  Wire.begin(25,26); //i2c config
  pinMode (LIGHT_SENSOR_PIN, INPUT);      // sets the digital pin 7 as input
  pinMode (DOOR_PIN, INPUT_PULLUP);
  pinMode (BUTTON_PIN, INPUT_PULLUP);
  pinMode (LED_BUILTIN, OUTPUT);   //Specify that LED pin is output
  pinMode (FAN_RELAY_PIN, OUTPUT);
  pinMode (LED_RELAY_PIN, OUTPUT);
  read_id();
  /* Setup the sensor power on */
  tsl.TSL2581_power_on();
  delay(2000);
  //  /* Setup the sensor gain and integration time */
  tsl.TSL2581_config();
}

void loop(void)
{
  
  unsigned long Lux;
  tsl.TSL2581_Read_Channel();
  doorValue = digitalRead(DOOR_PIN); 
  buttonValue = digitalRead(BUTTON_PIN);
  Lux = tsl.calculateLux(2, NOM_INTEG_CYCLE);
  if (ESP_BT.available()) //Check if we receive anything from Bluetooth
  {
    incoming_bt = ESP_BT.read(); //Read what we recevive 
    switch (incoming_bt)
  { 
    case 48: //0
        digitalWrite(LED_BUILTIN, LOW);
    break;
    case 49://1
        digitalWrite(LED_BUILTIN, HIGH);
    break;
    case 50: //2
        digitalWrite(FAN_RELAY_PIN, HIGH);
    break;
    case 51: //3
        digitalWrite(FAN_RELAY_PIN, LOW);
    break;
    case 52: //4
        Turn_on_Light();
    break;
    case 53: //5
        Turn_off_Light();
    break;
    case 54: //6
        ESP_BT.println(indoor.getTemperature());
    break;
    case 55: //7
        ESP_BT.println(indoor.getHumidity());
    break;
    case 56: //8
        led_relay_control = !led_relay_control;
        break;
    case 57: //9
        ESP_BT.println(outdoor.getTemperature());
    break;
    case 58: //:
        ESP_BT.println(outdoor.getHumidity());
    break;
    case 65: //A
        fan_relay_control = !fan_relay_control;
    break;
    case 66: //B
        ESP_BT.println(time_actual);
    break;
    case 76: //L
        ESP_BT.println(Lux);
    break;
    case 81: //Q
        ESP_BT.println(doorValue);
    }
  }else{
  if(buttonValue == LOW){
      Change_Light();
      bottom_state = !bottom_state; 
    }
  if(led_relay_control){
    if((doorValue == HIGH) & (Lux < lux_limiter)){
      Turn_on_Light();
    }else{
      Turn_off_Light();
      }
    }
  if(fan_relay_control){  
    time_actual = millis();
    Serial.println(time_actual);
      time_diff = time_actual - time_saved; 
      if(time_diff >= fan_time_expected){
        if(indoor.getHumidity()>outdoor.getHumidity()){
          indoor_fan_state = true; 
        }else{
          indoor_fan_state = false;
          }
         time_saved = time_actual;
        }
  
    if(indoor_fan_state){
      digitalWrite(FAN_RELAY_PIN, HIGH);
  
    }else{
      digitalWrite(FAN_RELAY_PIN, LOW);
      }
    }
  }
  Read_gpio_interrupt(2000, 50000);
  delay(50);
}
