/**********************************************************************************/
/* MIT License                                                                    */
/*                                                                                */
/* Copyright (c) [2023] [Jerrick.Rowe]                                            */
/*                                                                                */
/* Permission is hereby granted, free of charge, to any person obtaining a copy   */
/* of this software and associated documentation files (the "Software"), to deal  */
/* in the Software without restriction, including without limitation the rights   */
/* to use, copy, modify, merge, publish, distribute, sublicense, and/or sell      */
/* copies of the Software, and to permit persons to whom the Software is          */
/* furnished to do so, subject to the following conditions:                       */
/*                                                                                */
/* The above copyright notice and this permission notice shall be included in all */
/* copies or substantial portions of the Software.                                */
/*                                                                                */
/* THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR     */
/* IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,       */
/* FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE    */
/* AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER         */
/* LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,  */
/* OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE  */
/* SOFTWARE.                                                                      */
/**********************************************************************************/

/*--- Private dependencies ------------------------------------------------------------*/
#include "state_machine_port.h"
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>

#include "systime.h"
#include "logger.h"

#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/semphr.h"

#define DEBUG_PRINT 1
#include "debug_print.h"

#define DEBUG_MEMORY 0

#ifdef __cplusplus
extern "C" {
#endif

/*--- Public variable definitions -----------------------------------------------------*/

/*--- Private macros ------------------------------------------------------------------*/

/*--- Private type definitions --------------------------------------------------------*/

/*--- Private function declarations ---------------------------------------------------*/
uint32_t fsm_port_get_systime(void);
void*	 fsm_port_malloc(size_t size);
void	 fsm_port_free(void* buf);
void*	 fsm_port_mutex_create(void);
bool	 fsm_port_mutex_lock(void* mutex, uint32_t blocktime);
bool	 fsm_port_mutex_unlock(void* mutex);
bool	 fsm_port_mutex_destroy(void* mutex);
void*	 fsm_port_queue_create(uint32_t length, uint32_t item_size);
bool	 fsm_port_queue_send(void* queue, void* item, uint32_t blocktime);
bool	 fsm_port_queue_receive(void* queue, void* dst, uint32_t blocktime);
bool	 fsm_port_queue_clear(void* queue);
bool	 fsm_port_queue_destroy(void* queue);
void	 fsm_port_print(int level, int line, const char* filename, char* fmt, ...);

/*--- Private variable definitions ----------------------------------------------------*/
const struct os_handle fsm_port_os_handle = { .uptime_ms	 = fsm_port_get_systime,
											  .malloc		 = fsm_port_malloc,
											  .free			 = fsm_port_free,
											  .mutex_create	 = fsm_port_mutex_create,
											  .mutex_destroy = fsm_port_mutex_destroy,
											  .mutex_lock	 = fsm_port_mutex_lock,
											  .mutex_unlock	 = fsm_port_mutex_unlock,
											  .queue_create	 = fsm_port_queue_create,
											  .queue_destroy = fsm_port_queue_destroy,
											  .queue_send	 = fsm_port_queue_send,
											  .queue_receive = fsm_port_queue_receive,
											  .queue_clear	 = fsm_port_queue_clear,
											  .print		 = fsm_port_print };

/*--- Private function definitions ----------------------------------------------------*/

uint32_t fsm_port_get_systime(void) {
	return uptime_ms_get();
}

void* fsm_port_malloc(size_t size) {
	void* ret = malloc(size);
	memset(ret, 0, size);
#if DEBUG_MEMORY
	fsm_port_print(FSM_DBG_LVL_RAW, "[FSM malloc] %p: %lu" NL, ret, size);
#endif
	return ret;
}

void fsm_port_free(void* buf) {
#if DEBUG_MEMORY
	fsm_port_print(FSM_DBG_LVL_RAW, "[FSM free] %p" NL, buf);
#endif
	free(buf);
}

void* fsm_port_mutex_create(void) {
	void* ret = xSemaphoreCreateMutex();
#if DEBUG_MEMORY
	fsm_port_print(FSM_DBG_LVL_RAW, "[FSM mutex create] %p" NL, ret);
#endif
	return ret;
}

bool fsm_port_mutex_destroy(void* mutex) {
#if DEBUG_MEMORY
	fsm_port_print(FSM_DBG_LVL_RAW, "[FSM mutex destroy] %p" NL, mutex);
#endif
	vSemaphoreDelete(mutex);
	return true;
}

bool fsm_port_mutex_lock(void* mutex, uint32_t blocktime) {
	if(blocktime == BLOCKTIME_MAX) {
		blocktime = portMAX_DELAY;
	}
	BaseType_t res = xSemaphoreTake((SemaphoreHandle_t)mutex, blocktime);
	if(res == pdTRUE) {
		return true;
	}
	return false;
}

bool fsm_port_mutex_unlock(void* mutex) {
	BaseType_t res = xSemaphoreGive((SemaphoreHandle_t)mutex);
	if(res == pdTRUE) {
		return true;
	}
	return false;
}

void* fsm_port_queue_create(uint32_t length, uint32_t item_size) {
	void* ret = xQueueCreate(length, item_size);
#if DEBUG_MEMORY
	fsm_port_print(FSM_DBG_LVL_RAW, "[FSM queue create] %p" NL, ret);
#endif
	return ret;
}

bool fsm_port_queue_send(void* queue, void* item, uint32_t blocktime) {
	if(blocktime == BLOCKTIME_MAX) {
		blocktime = portMAX_DELAY;
	}
	BaseType_t res = xQueueSend((QueueHandle_t)queue, item, blocktime);
	if(res == pdTRUE) {
		return true;
	}
	return false;
}

bool fsm_port_queue_receive(void* queue, void* dst, uint32_t blocktime) {
	if(blocktime == BLOCKTIME_MAX) {
		blocktime = portMAX_DELAY;
	}
	BaseType_t res = xQueueReceive((QueueHandle_t)queue, dst, blocktime);
	if(res == pdTRUE) {
		return true;
	}
	return false;
}

bool fsm_port_queue_clear(void* queue) {
	BaseType_t res = xQueueReset(queue);
	if(res == pdTRUE) {
		return true;
	}
	return false;
}

bool fsm_port_queue_destroy(void* queue) {
#if DEBUG_MEMORY
	fsm_port_print(FSM_DBG_LVL_RAW, "[FSM queue destroy] %p" NL, queue);
#endif
	vQueueDelete(queue);
	return true;
}

void fsm_port_print(int level, int line, const char* filename, char* fmt, ...) {
	char	strbuf[201] = "unparsed";
	va_list args;
	va_start(args, fmt);
	vsnprintf(strbuf, 200, fmt, args);
	va_end(args);
	switch(level) {
	case FSM_DBG_LVL_ERR:
		PRINT_RAW(R_R "[E] %s:%d: %s" R_F NL, filename, line, strbuf);
		logger_print_err("[E][FSM]: %s", strbuf);
		break;
	case FSM_DBG_LVL_WRN:
		PRINT_RAW(R_Y "[W] %s:%d: %s" R_F NL, filename, line, strbuf);
		logger_print_wrn("[W][FSM]: %s", strbuf);
		break;
	case FSM_DBG_LVL_INF:
		PRINT_RAW(R_F "[I] %s:%d: %s" R_F NL, filename, line, strbuf);
		logger_print_inf("[I][FSM]: %s", strbuf);
		break;
	case FSM_DBG_LVL_RAW:
		PRINT_RAW("%s", strbuf);
		logger_print_inf("%s", strbuf);
		break;
	}
}

/*--- Public function definitions -----------------------------------------------------*/

#ifdef __cplusplus
}
#endif
