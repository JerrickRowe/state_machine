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
#include "state_machine.h"
#include "state_machine_port.h"
#include <stddef.h>
#include <string.h>
#include <stdbool.h>

#include <assert.h>
#define USE_ASSERT 1
#if USE_ASSERT
#define ASSERT(e) assert(e)
#else
#define ASSERT(e)
#endif

#ifdef __cplusplus
extern "C" {
#endif

/*--- Public variable definitions -----------------------------------------------------*/

/*--- Private macros ------------------------------------------------------------------*/
#define DEBUG_SHOW_FSM_STATE_TRANSITION	 1
#define DEBUG_SHOW_FSM_EVENT_PROPAGATION 0
#define CLEAR_ALL_EVENT_AFTER_EXIT_STATE 0
#define PASS_EVENT_TO_CHILD_FSM			 1
#define EVENT_QUEUE_LENGTH				 10
#define DEFAULT_POLLING_INTERVAL		 100

/*--- Private type definitions --------------------------------------------------------*/
struct state {
	uint32_t		magic_number;
	uint32_t		id;
	const char	   *name;
	state_handler_t handler;
	uint32_t		ts_poll;
	uint32_t		poll_interval;
	uint32_t		poll_interval_next;
	void		   *lock;
	struct fsm	   *parent_fsm;
	struct fsm	   *child_fsm;
	struct state   *next;
};

struct fsm {
	uint32_t	magic_number;
	void	   *lock;
	const char *name;
	uint32_t	poll_interval;
	void	   *event_queue;
	state_t		parent_state;
	state_t		state_list;
	state_t		sta_prev;
	state_t		sta_curr;
	state_t		sta_next;
	struct fsm *next;

	os_handle_t os;
};

/*--- Private function declarations ---------------------------------------------------*/

/*--- Private variable definitions ----------------------------------------------------*/
static struct state root_state = { .magic_number	   = STATE_MAGIC_NUMBER,
								   .id				   = STATE_ID_ROOT,
								   .name			   = STATE_NAME_ROOT,
								   .handler			   = NULL,
								   .ts_poll			   = 0,
								   .poll_interval	   = 100,
								   .poll_interval_next = 100,
								   .lock			   = NULL,
								   .parent_fsm		   = NULL,
								   .child_fsm		   = NULL,
								   .next			   = NULL };

/*--- Private function definitions ----------------------------------------------------*/
static int fsm_state_register(fsm_t fsm, state_t state) {
	int ret = 0;
	ASSERT(fsm);
	ASSERT(fsm->magic_number == FSM_MAGIC_NUMBER);
	ASSERT(state);
	ASSERT(state->magic_number == STATE_MAGIC_NUMBER);
	ASSERT(fsm->os);
	ASSERT(fsm->lock);
	// !Make sure this state has not been registered to other fsm
	ASSERT(state->parent_fsm == NULL);
	ASSERT(state->lock == NULL);

	os_handle_t os = fsm->os;
	os->mutex_lock(fsm->lock, BLOCKTIME_MAX);
	bool is_first_state = fsm->state_list == NULL ? true : false;
	// Slide to tail of the state list, and check for ID collision
	state_t *node = &(fsm->state_list);
	while(*node) {
		if((*node)->id == state->id) {
			OS_PRINT_ERR(os, "ID collision");
			ret = -1;
			goto ERROR;
		}
		node = &((*node)->next);
	}
	// Set state parameter
	state->lock = os->mutex_create();
	ASSERT(state->lock);
	os->mutex_lock(state->lock, BLOCKTIME_MAX);
	state->parent_fsm		  = fsm;
	state->poll_interval	  = fsm->poll_interval;
	state->poll_interval_next = fsm->poll_interval;
	state->next				  = NULL;
	os->mutex_unlock(state->lock);
	// Append to tail of the state list
	*node = state;
	if(is_first_state) {
		fsm->sta_next = state;
	}
ERROR:
	os->mutex_unlock(fsm->lock);
	return ret;
}

static int fsm_state_unregister(fsm_t fsm, state_t state) {
	ASSERT(fsm);
	ASSERT(fsm->magic_number == FSM_MAGIC_NUMBER);
	ASSERT(fsm->os);
	ASSERT(fsm->lock);
	os_handle_t os = fsm->os;
	os->mutex_lock(fsm->lock, BLOCKTIME_MAX);
	// Find the state in state list
	state_t *node = &(fsm->state_list);
	while(*node) {
		if(*node == state) {
			ASSERT(state);
			ASSERT(state->magic_number == STATE_MAGIC_NUMBER);
			ASSERT(state->lock);
			os->mutex_lock(state->lock, BLOCKTIME_MAX);
			// Reset state parameter
			state->parent_fsm	  = NULL;
			void *lock_to_destroy = state->lock;
			state->lock			  = NULL;
			// Remove the state from state list
			*node		= (*node)->next;
			state->next = NULL;
			if(fsm->sta_next == state) {
				fsm->sta_next = NULL;
			}
			// Release mutex resources
			os->mutex_destroy(lock_to_destroy);
			break;
		}
		node = &((*node)->next);
	}
	os->mutex_unlock(fsm->lock);
	return 0;
}

static state_t state_new(const char *name, uint32_t id, state_handler_t handler) {
	if(name == NULL) {
		name = "No name";
	}
	state_t		ret = NULL;
	os_handle_t os	= (os_handle_t)&fsm_port_os_handle;
	ret				= os->malloc(sizeof(struct state));
	memset(ret, 0, sizeof(struct state));
	ASSERT(ret);
	ret->magic_number = STATE_MAGIC_NUMBER;
	ret->name		  = name;
	ret->id			  = id;
	ret->handler	  = handler;
	return ret;
}

static int state_del(state_t *pstate) {
	int			ret = 0;
	os_handle_t os	= (os_handle_t)&fsm_port_os_handle;
	ASSERT(pstate);
	state_t state = *pstate;
	ASSERT(state);
	if(state->parent_fsm) {
		ret = fsm_state_unregister(state->parent_fsm, state);
	}
	os->free(state);
	*pstate = NULL;
	return ret;
}

/*--- Public function definitions -----------------------------------------------------*/
state_t fsm_get_state(fsm_t fsm, uint32_t id) {
	ASSERT(fsm);
	ASSERT(fsm->magic_number == FSM_MAGIC_NUMBER);
	os_handle_t os = fsm->os;
	os->mutex_lock(fsm->lock, BLOCKTIME_MAX);
	state_t node = (fsm->state_list);
	while(node && (node->id != id)) {
		node = node->next;
	}
	os->mutex_unlock(fsm->lock);
	return node;
}

state_t fsm_get_state_by_name(fsm_t fsm, const char *name) {
	ASSERT(name);
	ASSERT(fsm);
	ASSERT(fsm->magic_number == FSM_MAGIC_NUMBER);
	os_handle_t os = fsm->os;
	os->mutex_lock(fsm->lock, BLOCKTIME_MAX);
	state_t node = (fsm->state_list);
	while(node && (strcmp(name, node->name) != 0)) {
		node = node->next;
	}
	os->mutex_unlock(fsm->lock);
	return node;
}

int fsm_state_add(fsm_t fsm, const char *name, uint32_t id, state_handler_t handler) {
	int		ret	  = 0;
	state_t state = state_new(name, id, handler);
	ASSERT(state);
	ret = fsm_state_register(fsm, state);
	if(ret != 0) {
		state_del(&state);
	}
	return ret;
}

int fsm_state_del_by_name(fsm_t fsm, const char *name) {
	state_t state = fsm_get_state_by_name(fsm, name);
	if(state == NULL) {
		return -1;	// No such state
	}
	fsm_state_unregister(fsm, state);
	return state_del(&state);
}

int fsm_state_del(fsm_t fsm, uint32_t id) {
	state_t state = fsm_get_state(fsm, id);
	if(state == NULL) {
		return -1;	// No such state
	}
	fsm_state_unregister(fsm, state);
	return state_del(&state);
}

int fsm_state_child_fsm_add(state_t state, fsm_t fsm) {
	ASSERT(state);
	ASSERT(state->magic_number == STATE_MAGIC_NUMBER);
	ASSERT(fsm);
	ASSERT(fsm->magic_number == FSM_MAGIC_NUMBER);
	ASSERT(fsm->os);
	ASSERT(fsm->lock);
	ASSERT(state->lock);
	os_handle_t os = fsm->os;
	if(fsm == state->parent_fsm) {
		OS_PRINT_ERR(os, "FSM self reference is not allowed");
		return -1;
	}
	os->mutex_lock(state->lock, BLOCKTIME_MAX);
	// Slide to tail of the child-FSM list
	fsm_t *node = &(state->child_fsm);
	while(*node) {
		node = &((*node)->next);
	}
	// Set FSM parameters
	os->mutex_lock(fsm->lock, BLOCKTIME_MAX);
	fsm->parent_state = state;
	fsm->next		  = NULL;
	os->mutex_unlock(fsm->lock);
	// Append to tail of the child-FSM list
	*node = fsm;
	os->mutex_unlock(state->lock);
	return 0;
}

int fsm_state_child_fsm_del(state_t state, fsm_t fsm) {
	ASSERT(state);
	ASSERT(state->magic_number == STATE_MAGIC_NUMBER);
	ASSERT(fsm);
	ASSERT(fsm->magic_number == FSM_MAGIC_NUMBER);
	ASSERT(fsm->os);
	ASSERT(fsm->lock);
	ASSERT(state->lock);
	os_handle_t os = fsm->os;
	os->mutex_lock(state->lock, BLOCKTIME_MAX);
	// Find the FSM in child-FSM list
	fsm_t *node = &(state->child_fsm);
	while(*node) {
		if(*node == fsm) {
			// Reset FSM parameters
			os->mutex_lock(fsm->lock, BLOCKTIME_MAX);
			fsm->parent_state = NULL;
			// Remove the FSM from child-FSM list
			*node	  = (*node)->next;
			fsm->next = NULL;
			os->mutex_unlock(fsm->lock);
			break;
		}
		node = &((*node)->next);
	}
	os->mutex_unlock(state->lock);
	return 0;
}

int fsm_switch(fsm_t fsm, uint32_t id) {
	int ret = 0;
	ASSERT(fsm);
	ASSERT(fsm->magic_number == FSM_MAGIC_NUMBER);
	ASSERT(fsm->os);
	ASSERT(fsm->lock);
	os_handle_t os = fsm->os;
	os->mutex_lock(fsm->lock, BLOCKTIME_MAX);
	// Check if the state is registered to the FSM
	state_t *node = &(fsm->state_list);
	while(*node && ((*node)->id != id)) {
		node = &((*node)->next);
	}
	// Set next state to trigger state transition
	if(*node) {
		if(fsm->sta_next == NULL) {
			fsm->sta_next = *node;
		} else {
			OS_PRINT(os,
					 "FSM %s: Request \"%s\"->\"%s\" is ignored" NL,
					 fsm->name,
					 fsm->sta_curr->name,
					 (*node)->name);
		}
	} else {
		OS_PRINT_ERR(os, "No #%d state in \"%s\" fsm:", id, fsm->name);
		ret = -1;
	}
	os->mutex_unlock(fsm->lock);
	return ret;
}

int fsm_switch_by_state_handle(fsm_t fsm, state_t state) {
	int ret = 0;
	ASSERT(fsm);
	ASSERT(fsm->magic_number == FSM_MAGIC_NUMBER);
	ASSERT(state);
	ASSERT(state->magic_number == STATE_MAGIC_NUMBER);
	ASSERT(fsm->os);
	ASSERT(fsm->lock);
	os_handle_t os = fsm->os;
	os->mutex_lock(fsm->lock, BLOCKTIME_MAX);
	// Check if the state is registered to the FSM
	state_t *node = &(fsm->state_list);
	while(*node && (*node != state)) {
		node = &((*node)->next);
	}
	// Set next state to trigger state transition
	if(*node) {
		if(fsm->sta_next == NULL) {
			fsm->sta_next = *node;
		} else {
			OS_PRINT(os,
					 "FSM %s: Request \"%s\"->\"%s\" is ignored" NL,
					 fsm->name,
					 fsm->sta_curr->name,
					 (*node)->name);
		}
	} else {
		OS_PRINT_ERR(os, "No #%d:%s state in \"%s\" fsm:", state->id, state->name, fsm->name);
		ret = -1;
	}
	os->mutex_unlock(fsm->lock);
	return ret;
}

int fsm_switch_by_name(fsm_t fsm, const char *name) {
	int ret = 0;
	ASSERT(fsm);
	ASSERT(fsm->magic_number == FSM_MAGIC_NUMBER);
	ASSERT(fsm->os);
	ASSERT(fsm->lock);
	ASSERT(name);
	os_handle_t os = fsm->os;
	os->mutex_lock(fsm->lock, BLOCKTIME_MAX);
	// Check if the state is registered to the FSM
	state_t *node = &(fsm->state_list);
	while(*node && (strcmp((*node)->name, name) != 0)) {
		node = &((*node)->next);
	}
	// Set next state to trigger state transition
	if(*node) {
		if(fsm->sta_next == NULL) {
			fsm->sta_next = *node;
		} else {
			OS_PRINT(os,
					 "FSM %s: Request \"%s\"->\"%s\" is ignored" NL,
					 fsm->name,
					 fsm->sta_curr->name,
					 (*node)->name);
		}
	} else {
		OS_PRINT_ERR(os, "No %s state in \"%s\" fsm:", name, fsm->name);
		ret = -1;
	}
	os->mutex_unlock(fsm->lock);
	return ret;
}

int fsm_poll(fsm_t fsm) {
	ASSERT(fsm);
	ASSERT(fsm->magic_number == FSM_MAGIC_NUMBER);
	ASSERT(fsm->os);
	ASSERT(fsm->lock);
	os_handle_t os = fsm->os;

	uint32_t		ts				 = os->uptime_ms();
	state_handler_t exit			 = NULL;
	state_handler_t enter			 = NULL;
	state_handler_t handler			 = NULL;
	fsm_t		   *child_fsm_buf	 = NULL;
	uint32_t		child_fsm_number = 0;
	struct event	poll_event;

	// Process state transition
	os->mutex_lock(fsm->lock, BLOCKTIME_MAX);
	ASSERT(fsm->event_queue);
	void	*event_queue = fsm->event_queue;
	state_t *sta_prev	 = &(fsm->sta_prev);
	state_t *sta_curr	 = &(fsm->sta_curr);
	state_t *sta_next	 = &(fsm->sta_next);
	if(*sta_next) {
		*sta_prev = *sta_curr;
		*sta_curr = *sta_next;
		*sta_next = NULL;
		exit	  = (*sta_prev)->handler;
		enter	  = (*sta_curr)->handler;
#if DEBUG_SHOW_FSM_STATE_TRANSITION
		OS_PRINT(os,
				 "FSM %s: {%lu,%s}==>{%lu,%s}" NL,
				 fsm->name,
				 (*sta_prev)->id,
				 (*sta_prev)->name,
				 (*sta_curr)->id,
				 (*sta_curr)->name);
#endif
	}
	handler = (*sta_curr)->handler;
	// Count child fsm number
	fsm_t *node = &((*sta_curr)->child_fsm);
	while(*node) {
		child_fsm_number++;
		node = &((*node)->next);
	}
	// Copy child fsm to buffer
	if(child_fsm_number) {
		child_fsm_buf = os->malloc(sizeof(struct fsm) * child_fsm_number);
		ASSERT(child_fsm_buf);
		node  = &((*sta_curr)->child_fsm);
		int i = 0;
		while(*node) {
			child_fsm_buf[i++] = *node;
			node			   = &((*node)->next);
		}
	}
	// Generate polling event
	if((*sta_curr)->poll_interval != FSM_NO_POLL) {	 // Do not poll if poll_interval == FSM_NO_POLL
		if(ts - (*sta_curr)->ts_poll >= (*sta_curr)->poll_interval) {
			(*sta_curr)->ts_poll = ts;
			poll_event.timestamp = ts;
			poll_event.type		 = FSM_EVT_POLL;
			poll_event.data		 = NULL;
			poll_event.datalen	 = 0;
			// Send polling event
			if(os->queue_send(fsm->event_queue, &poll_event, 0) == false) {
				OS_PRINT_ERR(os, "Failed to send poll event to fsm %s", fsm->name);
			}
			// Update polling interval
			(*sta_curr)->poll_interval = (*sta_curr)->poll_interval_next;
		}
	} else {
		(*sta_curr)->poll_interval = (*sta_curr)->poll_interval_next;
	}
	os->mutex_unlock(fsm->lock);

	struct event event;
	// Exit previous state
	if(exit) {
		struct state_info info = {
			// Get target state info, which is current state
			.id	  = (*sta_curr)->id,
			.name = (*sta_curr)->name,
		};
		event.type		= FSM_EVT_EXIT;
		event.timestamp = ts;
		event.data		= &info;
		event.datalen	= sizeof(struct state_info);
		exit(&event);  // Exit handler of previous state
	}
#if CLEAR_ALL_EVENT_AFTER_EXIT_STATE
	os->queue_clear(event_queue);
#endif
	// Enter current state
	if(enter) {
		struct state_info info = {
			// Get previous state info, which is current state
			.id	  = (*sta_prev)->id,
			.name = (*sta_prev)->name,
		};
		event.type		= FSM_EVT_ENTER;
		event.timestamp = ts;
		event.data		= &info;
		event.datalen	= sizeof(struct state_info);
		enter(&event);	// Exit handler of current state
	}
	// Execute state handler when event occur
	bool event_occured = false;
	if(os->queue_receive(event_queue, &event, 0)) {
		// OS_PRINT(os, R_B "FSM %s, state %s received event %u" R_F ,
		// 		  fsm->name,
		// 		  (*sta_curr)->name,
		// 		  event.type);
		event_occured = true;
		if(handler) {
			handler(&event);
		}
	}
	// Process child-fsm
	for(int i = 0; i < child_fsm_number; i++) {
		fsm_t child_fsm = child_fsm_buf[i];
		ASSERT(child_fsm->magic_number == FSM_MAGIC_NUMBER);
#if PASS_EVENT_TO_CHILD_FSM
		ASSERT(child_fsm->event_queue);
		ASSERT(child_fsm->lock);
		if(event_occured && event.type != FSM_EVT_POLL) {
			// Event passing is not needed for poll event because it's sent from inside each FSM
			os->mutex_lock(child_fsm->lock, BLOCKTIME_MAX);
#if DEBUG_SHOW_FSM_EVENT_PROPAGATION
			OS_PRINT(os, "Pass event %lu(0x%X) to %s" NL, event.type, event.type, child_fsm->name);
#endif
			if(os->queue_send(child_fsm->event_queue, &event, 200) == false) {
				OS_PRINT_ERR(os,
							 "Timeout while passing event %u to child %s",
							 event.type,
							 child_fsm->name);
			}
			os->mutex_unlock(child_fsm->lock);
		}
#endif
		fsm_poll(child_fsm);
	}
	os->free(child_fsm_buf);

	return 0;
}

int fsm_init(fsm_t fsm, const char *name) {
	ASSERT(fsm);
	ASSERT(name);
	fsm->os		   = (os_handle_t)&fsm_port_os_handle;
	os_handle_t os = fsm->os;
	// Init root state if not
	if(root_state.lock == NULL) {
		root_state.lock = os->mutex_create();
	}
	fsm->lock = os->mutex_create();
	ASSERT(fsm->lock);
	os->mutex_lock(fsm->lock, BLOCKTIME_MAX);
	ASSERT(fsm->event_queue == NULL);
	fsm->event_queue   = os->queue_create(EVENT_QUEUE_LENGTH, sizeof(struct event));
	fsm->poll_interval = DEFAULT_POLLING_INTERVAL;
	fsm->magic_number  = FSM_MAGIC_NUMBER;
	fsm->name		   = name;
	fsm->parent_state  = NULL;
	fsm->state_list	   = NULL;
	fsm->sta_prev	   = &root_state;
	fsm->sta_curr	   = &root_state;
	fsm->sta_next	   = NULL;
	fsm->next		   = NULL;
	os->mutex_unlock(fsm->lock);
	return 0;
}

// !Caution: fsm must not be deinit if there are any procedure using it.
int fsm_deinit(fsm_t fsm) {
	ASSERT(fsm);
	ASSERT(fsm->magic_number == FSM_MAGIC_NUMBER);
	ASSERT(fsm->os);
	ASSERT(fsm->lock);
	os_handle_t os = fsm->os;

	// Delete this fsm from parent state
	os->mutex_lock(fsm->lock, BLOCKTIME_MAX);
	state_t parent_state = fsm->parent_state;
	state_t node		 = fsm->state_list;
	os->mutex_unlock(fsm->lock);
	if(parent_state) {
		fsm_state_child_fsm_del(parent_state, fsm);
	}

	state_t next;
	while(node) {
		next = node->next;
		fsm_state_unregister(fsm, node);
		os->free(node);
		node = next;
	}

	os->mutex_lock(fsm->lock, BLOCKTIME_MAX);
	fsm->magic_number  = 0;
	fsm->poll_interval = 0;
	fsm->name		   = NULL;
	fsm->os			   = NULL;
	fsm->state_list	   = NULL;
	fsm->sta_prev	   = NULL;
	fsm->sta_curr	   = NULL;
	fsm->sta_next	   = NULL;
	os->queue_clear(fsm->event_queue);
	os->queue_destroy(fsm->event_queue);
	fsm->event_queue	  = NULL;
	void *lock_to_destroy = fsm->lock;
	fsm->lock			  = NULL;
	os->mutex_destroy(lock_to_destroy);
	return 0;
}

void fsm_print_info(fsm_t fsm) {
	ASSERT(fsm);
	ASSERT(fsm->magic_number == FSM_MAGIC_NUMBER);
	ASSERT(fsm->os);
	ASSERT(fsm->lock);
	os_handle_t os = fsm->os;
	os->mutex_lock(fsm->lock, BLOCKTIME_MAX);

	state_t sta_prev = (fsm->sta_prev);
	state_t sta_curr = (fsm->sta_curr);
	state_t sta_next = (fsm->sta_next);
	OS_PRINT(os, "FSM %s info:" NL, fsm->name);
	state_t *node = &(fsm->state_list);

	OS_PRINT(os, " Default polling interval: ");
	if(fsm->poll_interval < BLOCKTIME_MAX) {
		OS_PRINT(os, "%lums" NL, fsm->poll_interval);
	} else {
		OS_PRINT(os, "NOPOLL" NL);
	}
	OS_PRINT(os, " Previous state: ");
	if(sta_prev) {
		OS_PRINT(os, "{%lu(0x%X),%s}" NL, sta_prev->id, sta_prev->id, sta_prev->name);
	} else {
		OS_PRINT(os, "NULL" NL);
	}
	OS_PRINT(os, "  Current state: ");
	if(sta_curr) {
		OS_PRINT(os, "{%lu(0x%X),%s}" NL, sta_curr->id, sta_curr->id, sta_curr->name);
	} else {
		OS_PRINT(os, "NULL" NL);
	}
	OS_PRINT(os, "     Next state: ");
	if(sta_next) {
		OS_PRINT(os, "{%lu(0x%X),%s}" NL, sta_next->id, sta_next->id, sta_next->name);
	} else {
		OS_PRINT(os, "NULL" NL);
	}

	while(*node) {
		fsm_t parent = (*node)->parent_fsm;
		fsm_t child	 = (*node)->child_fsm;
		OS_PRINT(os,
				 " {%s}->{S%u,%s}->{%s}" NL,
				 parent ? parent->name : "Detached",
				 (*node)->id,
				 (*node)->name,
				 child ? child->name : "No child");
		node = &((*node)->next);
	}

	os->mutex_unlock(fsm->lock);
}

void fsm_get_state_list_csv(fsm_t fsm, char *csv_buf) {
	ASSERT(fsm);
	ASSERT(fsm->magic_number == FSM_MAGIC_NUMBER);
	ASSERT(fsm->os);
	ASSERT(fsm->lock);
	os_handle_t os = fsm->os;
	os->mutex_lock(fsm->lock, BLOCKTIME_MAX);
	state_t *node = &(fsm->state_list);
	csv_buf[0]	  = '\0';
	while(*node) {
		strcat(csv_buf, (*node)->name);
		node = &((*node)->next);
		if(*node) {
			strcat(csv_buf, ",");
		}
	}
	os->mutex_unlock(fsm->lock);
}

int fsm_change_default_poll_interval(fsm_t fsm, uint32_t interval_ms) {
	ASSERT(fsm);
	ASSERT(fsm->magic_number == FSM_MAGIC_NUMBER);
	ASSERT(fsm->os);
	ASSERT(fsm->lock);
	os_handle_t os = fsm->os;
	os->mutex_lock(fsm->lock, BLOCKTIME_MAX);
	fsm->poll_interval = interval_ms;
	os->mutex_unlock(fsm->lock);
	return 0;
}

int fsm_change_state_poll_interval(state_t state, uint32_t interval_ms) {
	ASSERT(state);
	ASSERT(state->magic_number == STATE_MAGIC_NUMBER);
	state->poll_interval_next = interval_ms;
	return 0;
}

int fsm_event_send(fsm_t fsm, uint32_t type, void *data, uint32_t datalen) {
	int			 ret = 0;
	struct event event;
	ASSERT(fsm);
	ASSERT(fsm->magic_number == FSM_MAGIC_NUMBER);
	ASSERT(fsm->os);
	ASSERT(fsm->lock);
	ASSERT(fsm->event_queue);
	os_handle_t os	= fsm->os;
	event.timestamp = os->uptime_ms();
	event.type		= type;
	event.data		= data;
	event.datalen	= datalen;
	os->mutex_lock(fsm->lock, BLOCKTIME_MAX);
#if DEBUG_SHOW_FSM_EVENT_PROPAGATION
	OS_PRINT(os, "Send event %lu(0x%X) to %s" NL, type, type, fsm->name);
#endif
	if(os->queue_send(fsm->event_queue, &event, 200) == false) {
		OS_PRINT_ERR(os, "Timeout while sending event %lu", type);
		ret = -1;
	}
	os->mutex_unlock(fsm->lock);
	return ret;
}

int fsm_event_clear(fsm_t fsm) {
	ASSERT(fsm);
	ASSERT(fsm->magic_number == FSM_MAGIC_NUMBER);
	ASSERT(fsm->os);
	ASSERT(fsm->event_queue);
	os_handle_t os = fsm->os;
	os->queue_clear(fsm->event_queue);
	return 0;
}

void fsm_get_current_state(fsm_t fsm, state_info_t info) {
	ASSERT(fsm);
	ASSERT(fsm->magic_number == FSM_MAGIC_NUMBER);
	ASSERT(fsm->os);
	ASSERT(fsm->lock);
	ASSERT(info);
	os_handle_t os = fsm->os;
	os->mutex_lock(fsm->lock, BLOCKTIME_MAX);
	state_t sta_curr = fsm->sta_curr;
	ASSERT(sta_curr);
	ASSERT(sta_curr->magic_number == STATE_MAGIC_NUMBER);
	os->mutex_lock(sta_curr->lock, BLOCKTIME_MAX);
	info->id   = sta_curr->id;
	info->name = sta_curr->name;
	os->mutex_unlock(sta_curr->lock);
	os->mutex_unlock(fsm->lock);
}

fsm_t fsm_new(const char *name) {
	if(name == NULL) {
		name = "No name";
	}
	fsm_t		ret = NULL;
	os_handle_t os	= (os_handle_t)&fsm_port_os_handle;
	ret				= os->malloc(sizeof(struct fsm));
	ASSERT(ret);
	fsm_init(ret, name);
	return ret;
}

int fsm_del(fsm_t *fsm) {
	int ret = 0;
	if(fsm == NULL) {
		return -1;
	}
	if(*fsm == NULL) {
		return -2;
	}
	os_handle_t os = (os_handle_t)&fsm_port_os_handle;
	ret			   = fsm_deinit(*fsm);
	os->free(*fsm);
	*fsm = NULL;
	return ret;
}

#ifdef __cplusplus
}
#endif
