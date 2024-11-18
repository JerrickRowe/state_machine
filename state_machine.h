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

#ifndef __STATEMACHINE_H__
#define __STATEMACHINE_H__

/*--- Public dependencies -------------------------------------------------------------*/
#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include <limits.h>

#ifdef __cplusplus
extern "C" {
#endif

/*--- Public macros -------------------------------------------------------------------*/

#define STATE_MAGIC_NUMBER (0x1A1EA1CBu)
#define FSM_MAGIC_NUMBER   (0xF51EE15Fu)

#ifdef __cplusplus
#define STATE(_id, _name, _enter, _handler, _exit) \
	{ (STATE_MAGIC_NUMBER), (_id), (_name), (_enter), (_handler), (_exit) }
#else
#define STATE(_id, _name, _enter, _handler, _exit) \
	{ (STATE_MAGIC_NUMBER), (_id), (_name), (_enter), (_handler), (_exit) }
#endif

#define BLOCKTIME_MAX (UINT_MAX)
#define FSM_NO_POLL	  (UINT_MAX)

#define STATE_ID_ROOT	(UINT_MAX)
#define STATE_NAME_ROOT ("ROOT")

#define FSM_EVT_POLL  (UINT_MAX)
#define FSM_EVT_ENTER ((FSM_EVT_POLL)-1)
#define FSM_EVT_EXIT  ((FSM_EVT_ENTER)-1)

/**
 * @brief
 *
 */
#define IS_ENTER_FROM(_evt, _id) (((state_info_t)((event_t)_evt->data))->id == ((uint32_t)_id))

#define IS_ENTER_FROM_NAME(_evt, _name) \
	(strcmp(((state_info_t)((event_t)_evt->data))->name, (char *)(_name)) == 0)

#define IS_EXIT_TO(_evt, _id) (((state_info_t)((event_t)_evt->data))->id == ((uint32_t)_id))

#define IS_EXIT_TO_NAME(_evt, _name) \
	(strcmp(((state_info_t)((event_t)_evt->data))->name, (char *)(_name)) == 0)

/*--- Public type definitions ---------------------------------------------------------*/
struct event {
	uint32_t type;
	uint32_t timestamp;
	void	*data;
	uint32_t datalen;
};
typedef struct event *event_t;

struct state_info {
	uint32_t	id;
	const char *name;
};
typedef struct state_info *state_info_t;

typedef void (*state_handler_t)(event_t event);
typedef struct state *state_t;
typedef struct fsm	 *fsm_t;

/*--- Public variable declarations ----------------------------------------------------*/

/*--- Public function declarations ----------------------------------------------------*/

/**
 * @brief Create a new instance of a state machine.
 *
 * @param name The name of the state machine instance
 * @return fsm_t The newly created state machine instance
 */
extern fsm_t fsm_new(const char *name);

/**
 * @brief Delete an instance of a state machine.
 *
 * @param fsm Pointer to the state machine instance to delete
 * @return int 0 if the state machine instance was successfully deleted, or an error code if an
 * error occurred
 */
extern int fsm_del(fsm_t *fsm);

/**
 * @brief Retrieve a state handle from a state machine based on the state's ID.
 *
 * @param fsm The state machine from which to retrieve the state handle
 * @param id The ID of the state
 * @return state_t The state handle if found, or NULL if the state was not found
 */
extern state_t fsm_get_state(fsm_t fsm, uint32_t id);

/**
 * @brief Retrieve a state handle from a state machine based on the state's name.
 *
 * @param fsm The state machine from which to retrieve the state handle
 * @param name The name of the state
 * @return state_t The state handle if found, or NULL if the state was not found
 */
extern state_t fsm_get_state_by_name(fsm_t fsm, const char *name);

/**
 * @brief Add a state to a state machine.
 *
 * @param fsm The state machine to which the state will be added
 * @param name The name of the state to add
 * @param id The ID of the state to add
 * @param handler The state handler function for the added state
 * @return int 0 if the state was added successfully, or an error code if an error occurred
 */
extern int fsm_state_add(fsm_t fsm, const char *name, uint32_t id, state_handler_t handler);

/**
 * @brief Delete a state from a state machine based on the state's name.
 *
 * @param fsm The state machine from which to delete the state
 * @param name The name of the state to delete
 * @return int 0 if the state was deleted successfully, or an error code if an error occurred
 */
extern int fsm_state_del_by_name(fsm_t fsm, const char *name);

/**
 * @brief Delete a state from a state machine based on the state's ID.
 *
 * @param fsm The state machine from which to delete the state
 * @param id The ID of the state to delete
 * @return int 0 if the state was deleted successfully, or an error code if an error occurred
 */
extern int fsm_state_del(fsm_t fsm, uint32_t id);

/**
 * @brief Add a child FSM to the specified state.
 *
 * @param state The state to associate with the child FSM
 * @param fsm The child FSM object to add
 */
extern int fsm_state_child_fsm_add(state_t state, fsm_t fsm);

/**
 * @brief Remove the specified child FSM from the state.
 *
 * @param state The state from which to remove the child FSM
 * @param fsm The child FSM object to remove
 * @return int The result or error code (if applicable)
 */
extern int fsm_state_child_fsm_del(state_t state, fsm_t fsm);

/**
 * @brief Run the state machine by periodically polling this function. The polling interval
 *        should be smaller than or equal to the smallest interval setting among all of the
 *        states in the state machine to ensure correct polling behavior.
 *
 * @param fsm The state machine object
 * @return int Always 0
 */
extern int fsm_poll(fsm_t fsm);

/**
 * @brief Switch the state machine to a state specified by ID.
 *
 * @param fsm The state machine to switch
 * @param id The ID of the desired state to switch to
 * @return int 0 if the state was successfully switched, -1 if the state is not in the provided
 * state machine
 */
extern int fsm_switch(fsm_t fsm, uint32_t id);

/**
 * @brief Switch the state machine to a state specified by the state handle.
 *
 * @param fsm The state machine to switch
 * @param state The handle of the desired state to switch to
 * @return int 0 if the state was successfully switched, -1 if the state is not in the provided
 * state machine
 */
extern int fsm_switch_by_state_handle(fsm_t fsm, state_t state);

/**
 * @brief Switch the state machine to a state specified by the name.
 *
 * @param fsm The state machine to switch
 * @param name The name of the desired state to switch to
 * @return int 0 if the state was successfully switched, -1 if the state is not in the provided
 * state machine
 */
extern int fsm_switch_by_name(fsm_t fsm, const char *name);

/**
 * @brief Get information about the current running state from a state machine.
 *
 * @param fsm The state machine to retrieve the information from
 * @param info The structure to store the information about the current state
 */
extern void fsm_get_current_state(fsm_t fsm, state_info_t info);

/**
 * @brief Change the default polling interval of a state machine, which affects newly added states.
 *
 * @param fsm The state machine to change the default polling interval of
 * @param interval_ms The new default polling interval in milliseconds
 * @return int
 */
extern int fsm_change_default_poll_interval(fsm_t fsm, uint32_t interval_ms);

/**
 * @brief Change the polling interval of a state.
 *
 * @param state The state to change the polling interval of
 * @param interval_ms The new polling interval in milliseconds
 * @return int
 */
extern int fsm_change_state_poll_interval(state_t state, uint32_t interval_ms);

/**
 * @brief Send an event to a state machine.
 *
 * @param fsm Pointer to the state machine
 * @param type The event type
 * @param data Pointer to the event data
 * @param datalen The length of the event data
 * @return int 0 if the event was successfully sent
 */
extern int fsm_event_send(fsm_t fsm, uint32_t type, void *data, uint32_t datalen);

/**
 * @brief Clear all event queueing in a state machine.
 *
 * @param fsm The state machine to clear the event queue of
 * @return int Always 0
 */
extern int fsm_event_clear(fsm_t fsm);

/**
 * @brief Print information about a state machine to stdout
 *
 * @param fsm Pointer to a state machine
 */
extern void fsm_print_info(fsm_t fsm);

/**
 * @brief Get name of all states in a state machine in csv formated string
 *
 * @param fsm Pointer to a state machine
 * @param csv_buf Buffer to store csv string. You are expected to provide enough space in csv_buf
 * for entire string, or unexpected behaviour might happen
 */
extern void fsm_get_state_list_csv(fsm_t fsm, char *csv_buf);

#ifdef __cplusplus
}
#endif

#endif	// __STATEMACHINE_H__
