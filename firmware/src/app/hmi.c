#include "hmi.h"
#include "key.h"
#include "timer.h"
#include "printf.h"
#include "stdbool.h"
#include "ezled-host.h"
#include "ad5791.h"
#include "adt7420.h"
#include "parameter.h"

#define LOG_TAG              "hmi"
#define LOG_LVL              LOG_LVL_DBG
#include <ulog.h>

/**
 * Get input from key/encoder and control display and peripherals.
*/
float voltref_get_value(void);
float voltref_set_value(float volt);

enum{
 MENU_LEVEL_ROOT = 0,   /**< root menu, used to display the real output voltage. */
 MENU_LEVEL_SHOW_MENU,  /**< setting menu */
 MENU_LEVEL_SHOW_VALUE, /**< show the setting value */
 MENU_LEVEL_ADJ_VALUE,  /**< adjust the setting value */
};

enum{
  MAIN_MENU_SET_VOLT = 0,         /**< set output voltage */
  MAIN_MENU_SET_CODE,             /**< set ad5791 code*/
  MAIN_MENU_CAL_VREF,             /**< calibrate ad5791 reference voltage*/
  MAIN_MENU_SHOW_TEMP,            /**< show internal temperature */
  MAIN_MENU_SET_CONTRAST,         /**< set led contrast. */
  MAIN_MENU_SHOW_VERSION,         /**< show hw/sw version */
  MAIN_MENU_SHOW_POWERUP_COUNT,   /**< show power up count */
};

#define MAIN_MENU_COUNT (sizeof(hmi_menu)/sizeof(struct _menu))
void on_key_set_volt(int8_t);
void on_key_set_code(int8_t);
void on_key_cal_reference(int8_t);
void on_key_show_temperature(int8_t);
void on_key_set_contrast(int8_t);   //set led display contrast.
void on_key_powerup_count(int8_t);
void on_key_show_version(int8_t);
void on_refresh_set_volt(void);
void on_refresh_set_code(void);
void on_refresh_cal_reference(void);
void on_refresh_show_temperature(void);
void on_refresh_set_contrast(void);
void on_refresh_powerup_count(void);
void on_refresh_show_version(void);

static const struct _menu
{
  char *name;
  uint8_t cursor_start;   //the first cursor position
  uint8_t menu_count;     //sub menu number
  void (*on_key)(int8_t); //key process callback
  void (*on_refresh)(void);
}hmi_menu[]={
  {
    .name = "1. sEt uOLt",
    .cursor_start = 2,
    .menu_count = 7,
    .on_key = on_key_set_volt,
    .on_refresh = on_refresh_set_volt,
  },
  {
    .name = "2. sEt CODE",
    .cursor_start = 3,
    .menu_count = 5,
    .on_key = on_key_set_code,
    .on_refresh = on_refresh_set_code,
  },
  {
    .name = "3. CAL rEF",
    .cursor_start = 2,
    .menu_count = 7,
    .on_key = on_key_cal_reference,
    .on_refresh = on_refresh_cal_reference,
  },
  {
    .name = "4. tP ",
    .cursor_start = 0xff,
    .menu_count = 0xff, //not valid(no sub menu)
    .on_key = on_key_show_temperature,
    .on_refresh = on_refresh_show_temperature,
  },
  {
    .name = "5. sEt CONt.",
    .cursor_start = 5,
    .menu_count = 1,
    .on_key = on_key_set_contrast,
    .on_refresh = on_refresh_set_contrast,
  },
  {
    .name = "6. Up COUNt",
    .cursor_start = 0xff,
    .menu_count = 0xff,
    .on_key = on_key_powerup_count,
    .on_refresh = on_refresh_powerup_count,
  },
  {
    .name = "7. About",
    .cursor_start = 2,
    .menu_count = 2,
    .on_key = on_key_show_version,
    .on_refresh = on_refresh_show_version,
  },
};

static double volt_set, volt_disp, volt_vref;
static float board_temp;
static int32_t code_set;
static int16_t disp_contrast = 90;

static uint8_t hw_version = 0xB, sw_version = 0x10;
static uint16_t power_up_count = 0;

static int16_t main_menu = 0;
static int16_t sub_menu = 0;
static int16_t menu_level = 0;  //0: root, 1: main menu, 2: sub menu
static bool b_auto_refresh = false;
static bool b_refresh_menu = true;
static uint16_t menu_exit_timer = 0;

void hmi_timer(void){
  if(b_auto_refresh)
    b_refresh_menu = true; //periodically refresh menu
  menu_exit_timer ++;
  if(menu_exit_timer == 60/2){ //6 second time out
    menu_exit_timer = 0;//exit menu automatically if no operation is made
		if(menu_level == MENU_LEVEL_SHOW_MENU){
      main_menu = 0;
      sub_menu = 0;
      menu_level = MENU_LEVEL_ROOT;
      b_refresh_menu = true;
    }
  }
}

void hmi_init(void){
  void disp_uart_char(uint8_t c);
  key_init();
  ezled_host_init((void(*)(char))disp_uart_char);
  timer_register(hmi_timer, 200);  //200 ms timer

  //load parameter
  struct _parameter parameter;
  parameter_load(&parameter);
  parameter.power_up_count ++;
  parameter_save(&parameter);
  ad5791_set_vref(parameter.refer_voltage+10);
  power_up_count = parameter.power_up_count;
  disp_contrast = (parameter.hw_info>>24)&0xff;
  hw_version = (parameter.hw_info>>16)&0xff;
  sw_version = (parameter.hw_info>>8)&0xff;

  volt_disp = ad5791_set_volt(volt_set);
  volt_vref = ad5791_get_vref();
  code_set = ad5791_get_code();
  adt7420_get_tmp(&board_temp);
  ezled_set_global_contrast(disp_contrast);
}

static uint32_t ipow(uint32_t x, uint32_t y){
  uint32_t res = x;
  if(y == 0) return 1;
  if(y == 1) return res;
  y--;
  while(y--)
    res *= x;
  return res;
}

static void _display_cursor(void){
  uint8_t pos = sub_menu+hmi_menu[main_menu].cursor_start;
  if(menu_level == MENU_LEVEL_SHOW_VALUE){
    ezled_hightlight(pos);
    ezled_set_blink(LED_NO_ONE);
  }
  else{//adjusting number now. should blink some led.
    ezled_set_blink(pos);
    ezled_hightlight(LED_NO_ONE);
  }
}

static void _dispaly_menu_name(void){
  ezled_set_blink(LED_NO_ONE);
  ezled_hightlight(LED_NO_ONE);
  ezled_print(hmi_menu[main_menu].name);
}

static void on_refresh_set_volt(void){
  char buff[32];
  if(menu_level == MENU_LEVEL_SHOW_MENU){
    _dispaly_menu_name();
  }
  else
  {
    if(volt_set > 9.999999)
      sprintf(buff, "s%.6fu", volt_set);
    else
      sprintf(buff, "s %.6fu", volt_set);
    ezled_print(buff); //print setting voltage value;
    _display_cursor();
  }
}

static void on_refresh_set_code(void){
  char buff[32];
  if(menu_level == MENU_LEVEL_SHOW_MENU){
    _dispaly_menu_name();
  }
  else  //MENU_LEVEL_SHOW_VALUE or MENU_LEVEL_ADJ_VALUE
  {
    uint32_t code_set = ad5791_get_code();
    sprintf(buff, "0h %05X", code_set);
    ezled_print(buff);
    // display cursor
    _display_cursor();
  }
}

static void on_refresh_cal_reference(void){
  char buff[32];
  if(menu_level == MENU_LEVEL_SHOW_MENU){
    _dispaly_menu_name();
  }
  else
  {
    volt_vref = ad5791_get_vref();
    if(volt_vref > 9.999999)
      sprintf(buff, "r%.6fu", volt_vref);
    else
      sprintf(buff, "r %.6fu", volt_vref);
    ezled_print(buff);
    _display_cursor();
  }
}

static void on_refresh_show_temperature(void){
  char buff[32];
  char *pbuff = buff;
  static uint8_t b_blink = 0;
  if(adt7420_get_tmp(&board_temp))//temperature is updated since last read.
    b_blink = !b_blink;
  if(menu_level == MENU_LEVEL_SHOW_MENU){
    int i=0;
    for(;hmi_menu[main_menu].name[i];i++){
      buff[i] = hmi_menu[main_menu].name[i]; //copy menu
    }
    pbuff = buff + i; //append temperature value to menu.
  }
  sprintf(pbuff, "%.2f%c", board_temp, b_blink?' ':'c');  //format temperature value
  ezled_print(buff); //print setting voltage value;
}

void on_refresh_set_contrast(void){
  char buff[32];
  if(menu_level == MENU_LEVEL_SHOW_MENU){
    _dispaly_menu_name();
  }
  else
  {
    sprintf(buff, "CONt. %d", disp_contrast);
    ezled_print(buff);
  }
}

void on_refresh_powerup_count(void){
  char buff[32];
  if(menu_level == MENU_LEVEL_SHOW_MENU){
    _dispaly_menu_name();
  }
  else
  {
    sprintf(buff, "COUNt.%5d", power_up_count);
    ezled_print(buff);
  }
}

void on_refresh_show_version(void){
  char buff[32];
  if(menu_level == MENU_LEVEL_SHOW_MENU){
    _dispaly_menu_name();
  }
  else
  {
    sprintf(buff, "H-r.%x s-%x.%x", hw_version&0xf, sw_version>>4, sw_version&0xf);
    ezled_print(buff);
  }
}

static void menu_refresh(void){
  char buff[32];
  if(!b_refresh_menu) return;
  b_refresh_menu = false;
  if(menu_level == MENU_LEVEL_ROOT){  //root menu
    //show the real volate
    uint8_t len;
    sprintf(&buff[1], "%.6fu .", volt_disp);
    buff[0] = ' ';
    len = strlen(buff);
    if(len == 12)
      ezled_print(buff);
    else
      ezled_print(buff+1);
    ezled_hightlight(LED_NO_ONE);
    ezled_set_blink(9);
  }
  else{
    //we are not in root menu
    if(hmi_menu[main_menu].on_refresh)
      hmi_menu[main_menu].on_refresh();
  }
}

static double float_adjust(double value, double max, int16_t encoder, int16_t position){
  double scale = 1e-6;
  double delta = scale*ipow(10, 6-position)*encoder;
  delta += value;
  if(delta <= max)
    value = delta;
  if(value<0)
    value = 0;
  return value;
}

static int32_t hex5_adjust(int32_t value, uint32_t max, int16_t encoder, int16_t position){
	if(position > 4) position = 4;
  int32_t delta = ipow(16, 4-position)*encoder;
  delta += value;
  if(delta <= max)
    value = delta;
  if(value<0)
    value = 0;
  return value;
}

static void on_key_set_volt(int8_t encoder){
  if(menu_level == MENU_LEVEL_ADJ_VALUE){
    volt_set = float_adjust(volt_set, volt_vref, encoder, sub_menu);
    volt_disp = voltref_set_value(volt_set);
  }
}

static void on_key_set_code(int8_t encoder){
  if(menu_level == MENU_LEVEL_ADJ_VALUE){
    code_set = hex5_adjust(code_set, 0xfffff, encoder, sub_menu);
    volt_disp = ad5791_set_code(code_set);
  }
}

static void on_key_cal_reference(int8_t encoder){
  if(menu_level == MENU_LEVEL_ADJ_VALUE){
    volt_vref = float_adjust(volt_vref, 15.0f, encoder, sub_menu);
    ad5791_set_vref(volt_vref);
    volt_disp = ad5791_set_code(ad5791_get_code());
  }
}

static void on_key_show_temperature(int8_t encoder){
  //directly exit to root menu, if any key is changed.
  if(menu_level != MENU_LEVEL_SHOW_MENU){
    menu_level = MENU_LEVEL_ROOT;
    b_refresh_menu = true;
  }
}

static void on_key_set_contrast(int8_t encoder){
  if(menu_level == MENU_LEVEL_SHOW_MENU){
	}
	else{
		disp_contrast += 10*encoder;
		if(disp_contrast < 10)
			disp_contrast = 10;
		else if(disp_contrast > 90)
			disp_contrast = 90;
		ezled_set_global_contrast(disp_contrast);
	}
}

static void on_key_powerup_count(int8_t encoder){
  //directly exit to root menu, if any key is pressed.
  if(menu_level != MENU_LEVEL_SHOW_MENU){
		menu_level = MENU_LEVEL_ROOT;
		b_refresh_menu = true;
	}
}

static void on_key_show_version(int8_t encoder){
  //directly exit to root menu, if any key is pressed.
  if(menu_level != MENU_LEVEL_SHOW_MENU){
		menu_level = MENU_LEVEL_ROOT;
		b_refresh_menu = true;
	}
}

static void menu_navigate(int8_t encoder, uint8_t key){
  if(encoder || key){
    menu_exit_timer = 0;  //clear exit timer.
    b_refresh_menu = true;
  }
  // else //function is only called if there is key activity.
  //   return;
  if(key == KEY_OK){
    if(menu_level == MENU_LEVEL_ADJ_VALUE){
      menu_level = MENU_LEVEL_SHOW_VALUE; //exit to upper menu
    }
    else{
      if(menu_level == MENU_LEVEL_SHOW_MENU)
        sub_menu = 0; //reset sub menu before entering.
      menu_level ++;
    }
    LOG_D("menu level:%d",menu_level);
  }
  else if(key == (KEY_OK|KEY_PRESS_L)){
    //parameter could have changed, save it.
    if(menu_level == MENU_LEVEL_SHOW_VALUE || menu_level == MENU_LEVEL_ADJ_VALUE){
      struct _parameter parameter;
      parameter.signature = VALID_SIGNATURE;
      parameter.hw_info = HW_INFO(disp_contrast, hw_version, sw_version);
      parameter.power_up_count = power_up_count;
      parameter.refer_voltage = volt_vref-10; //store the voltage error.
      parameter_save(&parameter);
    }
    menu_level = MENU_LEVEL_ROOT; //return to root menu
    LOG_D("menu level:%d",menu_level);
  }
  else{
    //check encoder
    if(encoder){//encoder changed
      if(menu_level == MENU_LEVEL_ROOT){
        //enter to menu directly.
        if(encoder > 0){
          menu_level ++;
          main_menu = 0;
          sub_menu = 0;
        }
      }
      else if(menu_level == MENU_LEVEL_SHOW_MENU){
        //adjust main menu
        main_menu += encoder;
        if(main_menu < 0){
          main_menu = 0;
          menu_level --;
        }
        else if(main_menu >= MAIN_MENU_COUNT){
          main_menu = MAIN_MENU_COUNT-1; //set to last menu.
        }
        LOG_D("main menu:%d",main_menu);
      }
      else if(menu_level == MENU_LEVEL_SHOW_VALUE){
        if(hmi_menu[main_menu].cursor_start == 0xff){

        }
        else{
          //adjust sub menu
          sub_menu += encoder;
          //assume the maximum value has 10 position.
          if(sub_menu < 0)sub_menu = 0;
          else if(sub_menu >= hmi_menu[main_menu].menu_count) sub_menu = hmi_menu[main_menu].menu_count-1;
          LOG_D("sub menu:%d",sub_menu);
        }
      }
      //call menu on-key callback to process mainly the menu-level-adj-value
      if(hmi_menu[main_menu].on_key)
        hmi_menu[main_menu].on_key(encoder);
    }
  }
	//check if we need to refresh dislay periodically.
	b_auto_refresh = false;
	if(menu_level == MENU_LEVEL_SHOW_VALUE){
		if(main_menu == MAIN_MENU_SHOW_TEMP){//this is the temperature menu
			b_auto_refresh = true;
		}
	}
	else if(menu_level == MENU_LEVEL_SHOW_MENU){
		if(main_menu == MAIN_MENU_SHOW_TEMP){//temperature menu has temperature value, should refresh periodically.
			b_auto_refresh = true;
		}
	}
}

void hmi_disp_update(float volt){
  b_refresh_menu = true;
  volt_disp = volt_set = volt;
}

void hmi_poll(void){
  static uint8_t key_pre;
  static uint8_t encoder_pre;
  uint8_t key = get_key();
  uint8_t encoder = get_encoder();

  if((encoder != encoder_pre) || (key != key_pre)){
    int8_t temp;
    temp = (int8_t)(encoder - encoder_pre);
    encoder_pre = encoder;
    key_pre = key;
    // hmi_process_key(temp, key);
    menu_navigate(temp, key);
    LOG_D("Encode delta:%d\n", temp);
  }
  menu_refresh();
}
