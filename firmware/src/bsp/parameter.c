/**
 * @author Neo Xu (neo.xu1990@gmail.com)
 * @license The MIT License (MIT)
 * 
 * Copyright (c) 2019 Neo Xu
 * 
 * @brief use stm32 flash to store application parameters.
*/
#include "parameter.h"
#include "stm32f0xx_flash.h"

#define LOG_TAG              "flash"
#define LOG_LVL              LOG_LVL_DBG
#include <ulog.h>

#define PARAMETER_PAGE_ADDR 0x08007c00  //the last page of stm32f070f6
#define PARAMETER_PAGE_SIZE 1024        //flash is organized to 1kB per page.

/**
 * Parameter is stored in last page of MCU. The old parameter won't be erased until
 * there is no empty space in page.
 * The method to get latest parameter is to check if the next area to store parameter
 * is all '0xff'.
*/
const struct _parameter default_parameter = {
  .signature = VALID_SIGNATURE,
  .refer_voltage = 10,
  .hw_info = HW_INFO(90, 0xb, 0x10),
  .power_up_count = 0,
};

/**
 * Find the latest parameter address. return 0 if not found.
*/
static const struct _parameter *parameter_find_latest(void){
  const struct _parameter *pflash = (const struct _parameter *)PARAMETER_PAGE_ADDR;
  uint32_t b_found = 0;
  /**
   * [0]
   * [1]
   * ...
   * [last one]
   * [EMPTY ONE]  the last parameter is always reserved in flash and is always 0xff
  */
  while((uint32_t)(pflash+1) < (PARAMETER_PAGE_ADDR + PARAMETER_PAGE_SIZE)){
    if(pflash->signature == VALID_SIGNATURE){
      //is this the latest parameter?
      if(pflash[1].signature != VALID_SIGNATURE){//the signature is not valid
        b_found = 1;
        break;
      }
    }
    else
      break;
    pflash ++;
  }
  if(b_found){
    LOG_D("parameter found at 0x%08x", (uint32_t)pflash);
    return pflash;
  }
  return 0;
}

void parameter_load(struct _parameter *p){
  //find the latest parameter in parameter page.
  const struct _parameter *pflash;
  if(p == 0)
    return;
  pflash = parameter_find_latest(); //get the latest one
  char *psrc = (char*)pflash, *pdst = (char*)p;
  if(pflash == 0){
    psrc = (char*)&default_parameter;
  }
  for(int i=0; i<sizeof(struct _parameter); i++){
    *pdst++ = *psrc++;
  }
}

void parameter_save(const struct _parameter *p){
  const struct _parameter *pflash;
  if(p == 0)
    return;
  pflash = parameter_find_latest(); //get the latest one
	if(pflash == 0){//no valid parameter found
		pflash = (const struct _parameter *)PARAMETER_PAGE_ADDR;//we assume the flash is empty if it's not valid.
	}
  else{
		//check if parameter is changed
    uint32_t *psrc = (uint32_t*)p, *pdst = (uint32_t*)pflash;
    uint32_t b_equal = 1;
    for(int i=0; i<sizeof(struct _parameter)/4; i++){
      if(*pdst++ != *psrc++){
        b_equal = 0;
      }
    }
    if(b_equal){
      LOG_D("parameter is not changed, return now");
      return;
    }
		pflash ++;
	}
  if((uint32_t)(pflash+1) >= (PARAMETER_PAGE_ADDR + PARAMETER_PAGE_SIZE)){
    //there is no next one
    pflash = (const struct _parameter *)PARAMETER_PAGE_ADDR;
    //need to erase flash
    FLASH_Unlock();
    FLASH_ErasePage(PARAMETER_PAGE_ADDR);
    LOG_D("flash parameter page is erased");
  }
  //write latest parameter
  FLASH_Unlock();
  uint32_t *psrc = (uint32_t*)p, dst = (uint32_t)pflash;
  for(int i=0; i<sizeof(struct _parameter)/4; i++){
    FLASH_ProgramWord(dst, *psrc++);
    dst += 4;
  }
  FLASH_Lock();
  LOG_D("parameter is saved");
}
