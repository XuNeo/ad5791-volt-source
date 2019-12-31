/**
 * @author Neo Xu (neo.xu1990@gmail.com)
 * @license The MIT License (MIT)
 * 
 * Copyright (c) 2019 Neo Xu
 * 
*/
#ifndef _FIFO_CONF_H_
#define _FIFO_CONF_H_

#define FIFO_ENTER_CRITICAL() 	__disable_irq() //macro to enter critical area.
#define FIFO_EXIT_CRITICAL()		__enable_irq() 	//macro to exit critical area.

#endif
