/**
 * @author Neo Xu (neo.xu1990@gmail.com)
 * @license The MIT License (MIT)
 * 
 * Copyright (c) 2019 Neo Xu
 * 
 * @brief ad5791 driver.
*/
#include "ad5791.h"

#define AD5791_SYNC_L() GPIOA->BRR = GPIO_Pin_5
#define AD5791_SYNC_H() GPIOA->BSRR = GPIO_Pin_5

#define AD5791_SCLK_L() GPIOA->BRR = GPIO_Pin_4
#define AD5791_SCLK_H() GPIOA->BSRR = GPIO_Pin_4

#define AD5791_DIN_L() GPIOA->BRR = GPIO_Pin_3
#define AD5791_DIN_H() GPIOA->BSRR = GPIO_Pin_3

#define AD5791_CMD(addr, data) (((uint32_t)(addr&0xf)<<20)|(data&0xfffff))

#define AD5791REG_NOP     0   //no operation
#define AD5791REG_WDATA   1   //data register
#define AD5791REG_CTRL    2   //control register
#define AD5791REG_CLRCODE 3   //clear code register
#define AD5791REG_SCTRL   4   //software control register

//bit1
#define AD5791CTRL_A1_ON  (0<<1)  /*0: turn on internal A1 amplifier */  
#define AD5791CTRL_A1_OFF (1<<1)  /*1: turn off internal A1 amplifier */  
//bit2
#define AD5791CTRL_OPGND_NORMAL (0<<2)  /* 0: DAC operates in normal mode */ 
#define AD5791CTRL_OPGND_TOGND  (1<<2)  /* 1: DAC output is clamped to ground */ 
//bit3
#define AD5791CTRL_OUT_NORMAL   (0<<3)  /* 0: DAC output is in normal mode */
#define AD5791CTRL_OUT_TRISTATE (1<<3)  /* 1: DAC output is in tristate */
//bit4
#define AD5791CTRL_CODE_2SC     (0<<4)  /* 0: two's complement coding */     
#define AD5791CTRL_CODE_BIN     (1<<4)  /* 1: offset binary code */     
//bit5
#define AD5791CTRL_SDO_EN       (0<<5)  /* 0: SDO pin is enabled */
#define AD5791CTRL_SDO_DIS      (1<<5)  /* 1: SDO pin is disabled */
//bit6-9
#define AD5791CTRL_COMP10V      (0<<6)  /* 0: Line compensation input reference up to 10V  */
#define AD5791CTRL_COMP10V_12V  (9<<6)  /* 9: Line compensation 10V to 12V reference  */
#define AD5791CTRL_COMP12V_16V  (10<<6) /* 10: Line compensation 12V to 16V reference  */
#define AD5791CTRL_COMP16V_19V  (11<<6) /* 11: Line compensation 16V to 19V reference  */
#define AD5791CTRL_COMP19V_20V  (12<<6) /* 12: Line compensation 19V to 20V reference  */

//AD5791 software control set
#define AD5791SCTRL_RST         (1<<2)  /* Perform software reset */
#define AD5791SCTRL_CLR         (1<<1)  /* Perform clear operation */
#define AD5791SCTRL_LDAC        (1<<0)  /* Perform LDAC operation */

/**
 * used to track current dac code.
*/
static uint32_t dac_code20b = 0;
static double vref_volt = 10.091741325f; /* 10V by default. */
/**
 * @brief A simple delay function used to meet AD5791 timing.
 * @return none.
*/
static void _ad5791_delay(void){
  volatile uint32_t i = 10;
  while(i--);
}

#define ad5791_delay() _ad5791_delay()
/**
 * @brief send out 24bits though serial port
 * @return none.
*/
static void ad5791_send24b(uint32_t data){
  uint8_t count = 0;
  uint32_t mask = 1<<23;  /* start from MSB */
  AD5791_SYNC_L();
  for(;count<24;count++){
    AD5791_SCLK_H();
    if(mask&data)
      AD5791_DIN_H();
    else 
      AD5791_DIN_L();
    AD5791_SCLK_L();
    ad5791_delay();
    mask>>=1;
  }
  AD5791_SCLK_H();
  AD5791_SYNC_H();
  ad5791_delay(); //delay for signal SYNC
}

/**
 * @brief Write 24bit data to AD5791
 * @return none.
*/
static void ad5791_write_data(uint32_t data){
  ad5791_send24b(AD5791_CMD(AD5791REG_WDATA, data));
  dac_code20b = data;
}

/**
 * @brief ad5791 control registers setting.
 * @return none.
*/
static void ad5791_ctrl(uint32_t ctrl_set){
  ad5791_send24b(AD5791_CMD(AD5791REG_CTRL, ctrl_set));
}

/**
 * @brief set AD5791 clear code, the dac output data when CLR command is valid.
 * @return none.
*/
static void ad5791_set_clrcode(uint32_t data){
  ad5791_send24b(AD5791_CMD(AD5791REG_CLRCODE, data));
}

/**
 * @brief Software control of AD5791 like LDAC and RESET.
 * @return none.
*/
static void ad5791_sctrl(uint32_t ctrl_set){
  ad5791_send24b(AD5791_CMD(AD5791REG_SCTRL, ctrl_set));
}

/**
 * @brief Init AD5791 related GPIO peripheral etc.
 * @return none.
 * SYNC-->PA4, SCLK-->PA5 DIN-->PA3
*/
void ad5791_init(void){
  GPIO_InitTypeDef gpio_init;
  RCC_AHBPeriphClockCmd(RCC_AHBPeriph_GPIOA, ENABLE);
  gpio_init.GPIO_Mode = GPIO_Mode_OUT;
  gpio_init.GPIO_OType = GPIO_OType_PP;
  gpio_init.GPIO_Pin = GPIO_Pin_3|GPIO_Pin_4|GPIO_Pin_5;
  gpio_init.GPIO_PuPd = GPIO_PuPd_UP;
  gpio_init.GPIO_Speed = GPIO_Speed_50MHz;
  GPIO_Init(GPIOA, &gpio_init);
  AD5791_SYNC_H();
  AD5791_SCLK_H();
  AD5791_DIN_L();

  ad5791_sctrl(AD5791SCTRL_RST|AD5791SCTRL_LDAC);
  ad5791_ctrl(AD5791CTRL_A1_OFF|AD5791CTRL_CODE_BIN|AD5791CTRL_COMP10V|AD5791CTRL_OPGND_NORMAL|\
                AD5791CTRL_OUT_NORMAL|AD5791CTRL_SDO_DIS);
  ad5791_ctrl(AD5791CTRL_A1_OFF|AD5791CTRL_CODE_BIN|AD5791CTRL_COMP10V|AD5791CTRL_OPGND_NORMAL|\
                AD5791CTRL_OUT_NORMAL|AD5791CTRL_SDO_DIS);
  ad5791_set_clrcode(0);
  ad5791_write_data(0x00000);
}

/**
 * @brief set the volatage. unit is V
 * @param volt: the desired voltage in V
 * @return the real voltage in V.
*/
float ad5791_set_volt(float volt){
  uint32_t code;
  code = (uint32_t)(volt/vref_volt*0xfffff + 0.5f);
  if(code > 0xfffff) code = 0xfffff;
  ad5791_write_data(code);
  code &= 0xfffff;
  return code*vref_volt/0xfffff;
}

/**
 * @brief set ad5791 output code directly. This doesn't include calibration correction.
 * @return none.
*/
float ad5791_set_code(uint32_t code)
{
  ad5791_write_data(code&0xfffff);
  return code*vref_volt/0xfffff;
}

/**
 * @brief get ad5791 output code directly. This doesn't include calibration correction.
 * @return none.
*/
int32_t ad5791_get_code(void)
{
  return dac_code20b;
}

/**
 * Return reference voltage
*/
double ad5791_get_vref(void){
  return vref_volt;
}

/**
 * Set the ad5791 reference voltage value
*/
void ad5791_set_vref(double volt){
  vref_volt = volt;
}
