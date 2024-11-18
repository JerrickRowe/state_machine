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

#ifndef __STATEMACHINE_PORT_H__
#define __STATEMACHINE_PORT_H__

/*--- Public dependencies -------------------------------------------------------------*/
#include <stdint.h>
#include "state_machine.h"

#ifdef __cplusplus
extern "C" {
#endif

/*--- Public macros -------------------------------------------------------------------*/

/*--- Public type definitions ---------------------------------------------------------*/

struct os_handle {
	uint32_t (*uptime_ms)(void);
	void *(*malloc)(size_t size);
	void (*free)(void *buf);
	void *(*mutex_create)(void);
	bool (*mutex_destroy)(void *mutex);
	bool (*mutex_lock)(void *mutex, uint32_t blocktime);
	bool (*mutex_unlock)(void *mutex);
	void *(*queue_create)(uint32_t length, uint32_t item_size);
	bool (*queue_destroy)(void *queue);
	bool (*queue_send)(void *queue, void *item, uint32_t blocktime);
	bool (*queue_receive)(void *queue, void *dst, uint32_t blocktime);
	bool (*queue_clear)(void *queue);
	void (*print)(int level, int line, const char *filename, char *fmt, ...);
};
typedef struct os_handle *os_handle_t;

typedef enum fsm_dbg_lvl {
	FSM_DBG_LVL_OFF,
	FSM_DBG_LVL_ERR,
	FSM_DBG_LVL_WRN,
	FSM_DBG_LVL_INF,
	FSM_DBG_LVL_RAW,
} fsm_dbg_lvl_t;

#define NL "\r\n"

#define OS_PRINT(_os, fmt, ...)                                                             \
	do {                                                                                    \
		((os_handle_t)_os)->print(FSM_DBG_LVL_RAW, __LINE__, __FILE__, fmt, ##__VA_ARGS__); \
	} while(0)

#define OS_PRINT_ERR(_os, fmt, ...)                                                         \
	do {                                                                                    \
		((os_handle_t)_os)->print(FSM_DBG_LVL_ERR, __LINE__, __FILE__, fmt, ##__VA_ARGS__); \
	} while(0)

/*--- Public variable declarations ----------------------------------------------------*/
extern const struct os_handle fsm_port_os_handle;

/*--- Public function declarations ----------------------------------------------------*/

#ifdef __cplusplus
}
#endif

#endif	// __STATEMACHINE_PORT_H__
