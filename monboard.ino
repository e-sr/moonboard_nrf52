#include <bluefruit.h>
#include <Vector.h>
#include <Adafruit_NeoPixel.h>

#define LED_STRIPE_PIN        6
#define LED_NUMBER          200
#define PROBLEM_LEN          60
#define LED_OFFSET           0
#define HOLD_LEN             10
#define ZERO                 (-48)

#define START0     'l'
#define START1     '#'
#define START2STOP '#'
#define DEL        ','

struct hold_{
  uint8_t type;
  uint8_t number; 
};

Adafruit_NeoPixel pixels(LED_NUMBER, LED_STRIPE_PIN, NEO_RGB + NEO_KHZ800);
enum class State:uint32_t {wait0,wait1,acquire,ready,error};
// BLE Service
BLEUart bleuart; // uart over ble
uint32_t connections_since_start;

void setup()
{
  Serial.begin(115200);
  
  Serial.println("MOONBOARD");
  Serial.println("---------\n");
  pixels.begin();
  pixels.show();

  Bluefruit.autoConnLed(true);
  Bluefruit.configPrphBandwidth(BANDWIDTH_MAX);
  Bluefruit.begin();
  Bluefruit.setTxPower(4); 
  Bluefruit.setName("Moonboard A");
  Bluefruit.Periph.setConnectCallback(connect_callback);
  Bluefruit.Periph.setDisconnectCallback(disconnect_callback);

  bleuart.begin();
  startAdv();

  Serial.println("Moonboard Ready");
}

void startAdv(void)
{
  // Advertising packet
  Bluefruit.Advertising.addFlags(BLE_GAP_ADV_FLAGS_LE_ONLY_GENERAL_DISC_MODE);
  Bluefruit.Advertising.addTxPower();
  Bluefruit.Advertising.addService(bleuart);
  Bluefruit.ScanResponse.addName();
  Bluefruit.Advertising.restartOnDisconnect(true);
  Bluefruit.Advertising.setInterval(32, 244);    // in unit of 0.625 ms
  Bluefruit.Advertising.setFastTimeout(30);      // number of seconds in fast mode
  Bluefruit.Advertising.start(0);                // 0 = Don't stop advertising after n seconds  
}

void loop()
{
  enum class State state=State::wait0;
  //Vector continig the hold characters
  char hold_storage_array[HOLD_LEN];
  Vector<char> current_hold(hold_storage_array);
  //Vector containing the problem
  struct hold_ problem_storage_array[PROBLEM_LEN];
  Vector<struct hold_> problem(problem_storage_array);

  while(true)
  { 
    //begin loop
    while ( bleuart.available() )
    {//begin bleuart available
      char ch;
      ch = bleuart.read();
      switch(ch)
      {
      case START0:
        if( (state==State::wait0) || (state == State::error) )
        {
          state=State::wait1;
        }else{
          Serial.println(format_unexpected(ch,state));
          state=State::error;
        }
        break;
    
      case START2STOP:
        if (state==State::wait1)
        {
          state=State::acquire;
          current_hold.clear();
          problem.clear();
        }else if (state==State::acquire)
        {
          problem.push_back(hold_from_char(current_hold));
          current_hold.clear();
          Serial.println("NEW problem.");
          state=State::ready;
        }else{
          Serial.println(format_unexpected(ch,state));
          state=State::error;
        }
        break;

      case DEL:
        if(state==State::acquire)
        {
          problem.push_back(hold_from_char(current_hold));
          current_hold.clear();
        }else
        {
          Serial.println(format_unexpected(ch, state));
          state=State::error;
        }
        break;
        
      default:
        if (state==State::acquire)
        {
          current_hold.push_back(ch);
        }else
        {
          Serial.println(format_unexpected(ch, state));
          state=State::error;
        }
        break;
      }
      Serial.flush();
    }//end bleuart available

    if(state==State::ready)
    {
      show_problem(problem, pixels);
      state=State::wait0;
    }
    
  }//end loop
}

struct hold_ hold_from_char(Vector<char>& hold){
  struct hold_ temp_hold;
  int zero = ZERO;
  temp_hold.number=0;
  int base=1;
  for (size_t i = hold.size()-1; i > 0; i--)
  {
    temp_hold.number += (zero + ((uint8_t)hold.at(i)))*base;
    base*=10;
  }
  temp_hold.type=hold.at(0);
  return temp_hold; 
}

void show_problem(Vector<struct hold_>& problem, Adafruit_NeoPixel& pixels)
{ 
    pixels.clear();
    for (size_t i = 0; i < problem.size(); i++)
    {
      switch (problem[i].type)
      {
      case 'S':
        pixels.setPixelColor(problem[i].number + LED_OFFSET,0,255,0);
        break;
      case 'P':
        pixels.setPixelColor(problem[i].number + LED_OFFSET,0,0,255);
        break;
      case 'E':
        pixels.setPixelColor(problem[i].number + LED_OFFSET,255,0,0);
        break;
      }
    }
    pixels.show();

}

String format_unexpected(char ch, State state)
{
  String uexpected="Unexpected character: ";
  return uexpected + ch + "; state: " + static_cast<uint32_t>(state) + ".";
}

void connect_callback(uint16_t conn_handle)
{
  Serial.print("Device Connected");
  Serial.println(++connections_since_start, DEC);
  Serial.flush();
}

void disconnect_callback(uint16_t conn_handle, uint8_t reason)
{
  (void) conn_handle;
  (void) reason;
}
