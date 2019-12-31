/**
 * @author Neo Xu (neo.xu1990@gmail.com)
 * @license The MIT License (MIT)
 * 
 * Copyright (c) 2019 Neo Xu
 * 
 * @brief use stm32 flash to store application parameters.
*/
#ifndef _PARAMETER_H_
#define _PARAMETER_H_
#include "stdint.h"

#define VALID_SIGNATURE     0x1234a55a

#define HW_INFO(contrast, hw, sw) (((uint32_t)(contrast&0xff)<<24)|\
                                   ((uint32_t)(hw&0xff)<<16)|\
                                   ((uint32_t)(sw&0xff)<<8)|\
                                   ((uint32_t)(0&0xff))\
                                   )
struct _parameter{
  uint32_t signature;
  float refer_voltage;    //reference voltage
  uint32_t hw_info; //MSB<--8bit contrast, 8bit hw version, 8bit software version, 8bit reserved.-->LSB
  uint32_t power_up_count;
};

void parameter_load(struct _parameter *p);
void parameter_save(const struct _parameter *p);

#endif

