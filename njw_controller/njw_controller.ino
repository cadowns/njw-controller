#include<Arduino.h>
#include <lvgl.h>
#include <LovyanGFX.hpp>
#include "FT6236.h"
#include "ui.h"
#include "Wire.h"
#include "Preferences.h"


Preferences states;


const int i2c_touch_addr = TOUCH_I2C_ADD;

int old_input=0, input=0;

int minVol = 0;
int maxVol = 100;
int dispVol;
int default_vol = 50;

int testVol = 50;

int delta;
int old_delta;

#define LCD_BL 46

#define SDA_FT6236 38
#define SCL_FT6236 39
//FT6236 ts = FT6236();

class LGFX : public lgfx::LGFX_Device
{
    lgfx::Panel_ILI9488 _panel_instance;
    lgfx::Bus_Parallel16 _bus_instance;
  public:
    LGFX(void)
    {
      {
        auto cfg = _bus_instance.config();
        cfg.port = 0;
        cfg.freq_write = 80000000;
        cfg.pin_wr = 18;
        cfg.pin_rd = 48;
        cfg.pin_rs = 45;

        cfg.pin_d0 = 47;
        cfg.pin_d1 = 21;
        cfg.pin_d2 = 14;
        cfg.pin_d3 = 13;
        cfg.pin_d4 = 12;
        cfg.pin_d5 = 11;
        cfg.pin_d6 = 10;
        cfg.pin_d7 = 9;
        cfg.pin_d8 = 3;
        cfg.pin_d9 = 8;
        cfg.pin_d10 = 16;
        cfg.pin_d11 = 15;
        cfg.pin_d12 = 7;
        cfg.pin_d13 = 6;
        cfg.pin_d14 = 5;
        cfg.pin_d15 = 4;
        _bus_instance.config(cfg);
        _panel_instance.setBus(&_bus_instance);
      }

      {
        auto cfg = _panel_instance.config();

        cfg.pin_cs = -1;
        cfg.pin_rst = -1;
        cfg.pin_busy = -1;
        cfg.memory_width = 320;
        cfg.memory_height = 480;
        cfg.panel_width = 320;
        cfg.panel_height = 480;
        cfg.offset_x = 0;
        cfg.offset_y = 0;
        cfg.offset_rotation = 2;
        cfg.dummy_read_pixel = 8;
        cfg.dummy_read_bits = 1;
        cfg.readable = true;
        cfg.invert = false;
        cfg.rgb_order = false;
        cfg.dlen_16bit = true;
        cfg.bus_shared = true;
        _panel_instance.config(cfg);
      }
      setPanel(&_panel_instance);
    }
};

LGFX tft;
/*Change to your screen resolution*/
static const uint16_t screenWidth  = 480;
static const uint16_t screenHeight = 320;
static lv_disp_draw_buf_t draw_buf;
static lv_color_t buf[ screenWidth * screenHeight / 5 ];

/* Display flushing */
void my_disp_flush( lv_disp_drv_t *disp, const lv_area_t *area, lv_color_t *color_p )
{
  uint32_t w = ( area->x2 - area->x1 + 1 );
  uint32_t h = ( area->y2 - area->y1 + 1 );

  tft.startWrite();
  tft.setAddrWindow( area->x1, area->y1, w, h );
  tft.writePixels((lgfx::rgb565_t *)&color_p->full, w * h);
  tft.endWrite();

  lv_disp_flush_ready( disp );
}

void my_touchpad_read( lv_indev_drv_t * indev_driver, lv_indev_data_t * data )
{
  int pos[2] = {0, 0};

  ft6236_pos(pos);
  if (pos[0] > 0 && pos[1] > 0)
  {
    data->state = LV_INDEV_STATE_PR;
   data->point.x = tft.width()-pos[1];
   data->point.y = pos[0];
    // data->point.y = pos[1];
    // data->point.x = pos[0];
    // Serial.printf("x-%d,y-%d\n", data->point.x, data->point.y);
  }
  else {
    data->state = LV_INDEV_STATE_REL;
  }
}


void touch_init()
{
  // I2C init
  Wire.begin(SDA_FT6236, SCL_FT6236);
  byte error, address;

  Wire.beginTransmission(i2c_touch_addr);
  error = Wire.endTransmission();

  if (error == 0)
  {
    Serial.print("I2C device found at address 0x");
    Serial.print(i2c_touch_addr, HEX);
    Serial.println("  !");
  }
  else if (error == 4)
  {
    Serial.print("Unknown error at address 0x");
    Serial.println(i2c_touch_addr, HEX);
  }
}

void sendI2C(byte reg, byte data){
  Wire.beginTransmission(0x5A);
  byte message[] = {reg, data};
  Wire.write(message, sizeof(message));
  Wire.endTransmission();
}

void encVolControl(int neg){
  if (neg == 1){
    volumeDecrement();
  } else {
    volumeIncrement();
  }
}

void volumeIncrement(){
  if (dispVol <= 99) {
    dispVol += 1;
    handleVolume(dispVol);
  } else {
    Serial.println("Max volume reached");
  }
}

void volumeDecrement(){
  if (dispVol >= 1) {
    dispVol -= 1;
    handleVolume(dispVol);
  } else {
    Serial.println("Min volume reached");
  }
  
}

extern "C" {
  void handleVolume(int vol){
    dispVol = vol; // save vol (0-100) to global var
    states.putInt("vol", dispVol); // save EEPROM vol
    byte hexVol = map(dispVol, 0, 100, 0xFE, 0x24); // map displayVol to NJW vol
    sendI2C(0x00, hexVol); //send vol to NJW
    char dispVolChar[16]; //create char for dispVol representation
    itoa(dispVol, dispVolChar, 10); //convert dispVar to char array
    lv_label_set_text(ui_volLabel, dispVolChar); //update volume label on display
    lv_arc_set_value(ui_volIndicator, dispVol); //update volume indicator
  }
}

extern "C" {
  void changeInput(byte input){
    Serial.println("calling changeInput with input\n");
    Serial.write(input);
    sendI2C(0x02,input);
    states.putChar("input", input);
  }
}

extern "C" {
  void handleTreble(int trebleLevel){
    Serial.println("calling handleTreble with treble level: " + trebleLevel);
    sendI2C(0x03,byte(trebleLevel));
    states.putInt("trebleLevel", trebleLevel);
    String treble = "Treble";
    String trebLabel = "Treble: ";
    String trebLabelNum = trebLabel + trebleLevel;
    if (trebleLevel == 0) {
      lv_label_set_text(ui_trebleLabel, treble.c_str());
    } else {
      lv_label_set_text(ui_trebleLabel, trebLabelNum.c_str());
    }
  }
}

extern "C" {
  void handleBass(int bassLevel){
    Serial.println("calling handleBass with treble level: " + bassLevel);
    sendI2C(0x04,byte(bassLevel));
    states.putInt("bassLevel", bassLevel);
    String bass = "Bass";
    String bassLabel = "Bass: ";
    String bassLabelNum = bassLabel + bassLevel;
    if (bassLevel == 0) {
      lv_label_set_text(ui_bassLabel, bass.c_str());
    } else {
      lv_label_set_text(ui_bassLabel, bassLabelNum.c_str());
    }
  }
}


void reloadLastState(){
  bool doesVolExist = states.isKey("vol");
  if (doesVolExist) {
    handleVolume(states.getInt("vol"));
  } else {
    handleVolume(default_vol);
  }
  
  bool doesInputExist = states.isKey("input");
  if (doesInputExist) {
    selectInput(states.getChar("input"));
  } else {
    selectInput(1);
  }

  bool doesBassLevelExist = states.isKey("bassLevel");
  if (doesBassLevelExist) {
    handleBass(states.getInt("bassLevel"));
  } else {
    handleBass(0);
  }

  bool doesTrebleLevelExist = states.isKey("trebleLevel");
  if (doesTrebleLevelExist) {
    handleTreble(states.getInt("trebleLevel"));
  } else {
    handleTreble(0);
  }

}

void setup()
{
  

  Serial.begin( 115200 ); /* prepare for possible serial debug */
  states.begin("State", false); //EEPROM for saving state between reboots





  //IO口引脚
  pinMode(19, OUTPUT);
  digitalWrite(19, LOW);

  tft.begin();          /* TFT init */
  tft.setRotation( 1 ); /* Landscape orientation, flipped */
  tft.fillScreen(TFT_BLACK);
  delay(500);
  pinMode(LCD_BL, OUTPUT);
  digitalWrite(LCD_BL, HIGH);

  touch_init();

  lv_init();
  lv_disp_draw_buf_init( &draw_buf, buf, NULL, screenWidth * screenHeight / 5 );

  /*Initialize the display*/
  static lv_disp_drv_t disp_drv;
  lv_disp_drv_init( &disp_drv );
  /*Change the following line to your display resolution*/
  disp_drv.hor_res = screenWidth;
  disp_drv.ver_res = screenHeight;
  disp_drv.flush_cb = my_disp_flush;
  disp_drv.draw_buf = &draw_buf;
  lv_disp_drv_register( &disp_drv );

  /*Initialize the (dummy) input device driver*/
  static lv_indev_drv_t indev_drv;
  lv_indev_drv_init( &indev_drv );
  indev_drv.type = LV_INDEV_TYPE_POINTER;
  indev_drv.read_cb = my_touchpad_read;
  lv_indev_drv_register( &indev_drv );


  ui_init();
  reloadLastState();

  
}

void loop()
{
  if (input != old_input) {
    Serial.println(input);
    old_input = input;
  }

  Wire.requestFrom(0x5A, 1);
  if (Wire.available()) {
    byte d = Wire.read();
    if (d != 0){
      Serial.println(d);
      Serial.println(d, BIN);
      int neg = bitRead(d,7);
      Serial.println(neg);
      encVolControl(neg);
      neg = 0;
    }

  }

  lv_timer_handler(); /* let the GUI do its work */
  delay( 5 );
}



