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

/*--- Private dependencies -------------------------------------------------------*/

#include "unity.h"
#include <stdio.h>
#include <assert.h>
#include <string.h>
#include "sysdelay.h"

#include "../state_machine.h"

#include "esp_heap_caps.h"
#define GET_HEAP_FREE_SIZE() heap_caps_get_free_size(MALLOC_CAP_8BIT)

#ifdef __cplusplus
extern "C" {
#endif

#define TEST_SEQ_BUF_LEN 200

#define STATE_1_ID	 5
#define STATE_1_NAME "Test state 1"

#define STATE_2_ID	 2
#define STATE_2_NAME "Test state 2"

#define STATE_3_ID	 80
#define STATE_3_NAME "Test child state"

#define TEST_EVENT 0xA555

static char correct_seq_str[TEST_SEQ_BUF_LEN + 1] =
	"1 2 2 2 2 2 2 3 4 5 5 5 5 5 6 7 2 8 9 10 9 10 9 10 9 10 9 10 9 10 9 10 9 10 9 10 9 10 11 11 "
	"11 11 11 ";

static char test_seq_buf[TEST_SEQ_BUF_LEN + 1] = "";
static int	sta1_run_cnt					   = 0;
static int	sta2_run_cnt					   = 0;
static int	sta3_run_cnt					   = 0;

static void append_seq(int seq) {
	char seq_str[] = "123 ";
	snprintf(seq_str, sizeof(seq_str), "%d ", seq);
	strncat(test_seq_buf, seq_str, TEST_SEQ_BUF_LEN - strlen(test_seq_buf));
}

/*--- Private function definitions -----------------------------------------------*/
static void state1_handler(event_t event) {
	sta1_run_cnt++;
	switch(event->type) {
	case FSM_EVT_ENTER:
		if(IS_ENTER_FROM(event, STATE_ID_ROOT)) {
			append_seq(1);
		} else if(IS_ENTER_FROM_NAME(event, STATE_2_NAME)) {
			append_seq(7);
		}
		break;
	case FSM_EVT_EXIT:
		if(IS_EXIT_TO_NAME(event, STATE_2_NAME)) {
			append_seq(3);
		}
		break;
	case FSM_EVT_POLL: append_seq(2); break;
	case TEST_EVENT:
		TEST_ASSERT_EQUAL_INT(event->datalen, sizeof(int));
		append_seq(9);
		(*(int *)(event->data))++;
		break;
	default: break;
	}
}

static void state2_handler(event_t event) {
	sta2_run_cnt++;
	switch(event->type) {
	case FSM_EVT_ENTER:
		if(IS_ENTER_FROM(event, STATE_1_ID)) {
			append_seq(4);
		}
		break;
	case FSM_EVT_EXIT:
		if(IS_EXIT_TO_NAME(event, STATE_1_NAME)) {
			append_seq(6);
		}
		break;
	case FSM_EVT_POLL: append_seq(5); break;
	default: break;
	}
}

static void state3_handler(event_t event) {
	sta3_run_cnt++;
	switch(event->type) {
	case FSM_EVT_ENTER:
		if(IS_ENTER_FROM(event, STATE_ID_ROOT)) {
			append_seq(8);
		}
		break;
	case TEST_EVENT: append_seq(10); break;
	case FSM_EVT_POLL: append_seq(11); break;
	default: break;
	}
}

/*--- Public function definitions ------------------------------------------------*/

TEST_CASE("Test State machine features", "[fsm]") {
	fsm_t			  fsm, child_fsm;
	state_t			  state1, state2, state3;
	struct state_info info;
	uint32_t		  heap_begin	  = GET_HEAP_FREE_SIZE();
	int				  test_event_data = 0;

	test_seq_buf[0] = 0;
	sta1_run_cnt	= 0;
	sta2_run_cnt	= 0;

	fsm = fsm_new("Test FSM");
	TEST_ASSERT_NOT_NULL(fsm);
	child_fsm = fsm_new("Test child FSM");
	TEST_ASSERT_NOT_NULL(child_fsm);

	// No state is expected for a blank state machine
	TEST_ASSERT_NULL(fsm_get_state(fsm, STATE_1_ID));
	TEST_ASSERT_NULL(fsm_get_state(fsm, STATE_2_ID));
	TEST_ASSERT_NULL(fsm_get_state(fsm, STATE_3_ID));

	// Check if state registration interface work
	TEST_ASSERT_EQUAL_INT(fsm_state_add(fsm, STATE_1_NAME, STATE_1_ID, 0), 0);
	TEST_ASSERT_EQUAL_INT(fsm_state_add(fsm, STATE_2_NAME, STATE_2_ID, 0), 0);
	TEST_ASSERT_EQUAL_INT(fsm_state_add(fsm, STATE_3_NAME, STATE_3_ID, 0), 0);

	// Check if state getting interface work
	TEST_ASSERT_NOT_NULL(fsm_get_state(fsm, STATE_1_ID));
	TEST_ASSERT_NOT_NULL(fsm_get_state(fsm, STATE_2_ID));
	TEST_ASSERT_NOT_NULL(fsm_get_state(fsm, STATE_3_ID));
	TEST_ASSERT_NOT_NULL(fsm_get_state_by_name(fsm, STATE_1_NAME));
	TEST_ASSERT_NOT_NULL(fsm_get_state_by_name(fsm, STATE_2_NAME));
	TEST_ASSERT_NOT_NULL(fsm_get_state_by_name(fsm, STATE_3_NAME));

	// State get by either ID and name should be equal.
	TEST_ASSERT_EQUAL(fsm_get_state(fsm, STATE_1_ID), fsm_get_state_by_name(fsm, STATE_1_NAME));
	TEST_ASSERT_EQUAL(fsm_get_state(fsm, STATE_2_ID), fsm_get_state_by_name(fsm, STATE_2_NAME));
	TEST_ASSERT_EQUAL(fsm_get_state(fsm, STATE_3_ID), fsm_get_state_by_name(fsm, STATE_3_NAME));

	// Get state with a random name or ID should be NULL
	TEST_ASSERT_NULL(fsm_get_state_by_name(fsm, "random name"));
	TEST_ASSERT_NULL(fsm_get_state(fsm, 15247204));

	fsm_print_info(fsm);

	// Check if state deletting interface work
	TEST_ASSERT_EQUAL_INT(fsm_state_del(fsm, STATE_1_ID), 0);
	TEST_ASSERT_EQUAL_INT(fsm_state_del(fsm, STATE_2_ID), 0);
	TEST_ASSERT_EQUAL_INT(fsm_state_del(fsm, STATE_3_ID), 0);

	fsm_print_info(fsm);

	TEST_ASSERT_EQUAL_INT(fsm_change_default_poll_interval(fsm, 10), 0);
	TEST_ASSERT_EQUAL_INT(fsm_change_default_poll_interval(child_fsm, FSM_NO_POLL), 0);

	TEST_ASSERT_EQUAL_INT(fsm_state_add(fsm, STATE_1_NAME, STATE_1_ID, state1_handler), 0);
	TEST_ASSERT_EQUAL_INT(fsm_state_add(fsm, STATE_2_NAME, STATE_2_ID, state2_handler), 0);
	TEST_ASSERT_EQUAL_INT(fsm_state_add(child_fsm, STATE_3_NAME, STATE_3_ID, state3_handler), 0);

	// Test ID collision detection
	TEST_ASSERT_NOT_EQUAL(fsm_state_add(fsm, STATE_2_NAME, STATE_2_ID, state2_handler), 0);
	TEST_ASSERT_NOT_EQUAL(fsm_state_add(child_fsm, STATE_3_NAME, STATE_3_ID, state3_handler), 0);

	state1 = fsm_get_state(fsm, STATE_1_ID);
	state2 = fsm_get_state_by_name(fsm, STATE_2_NAME);
	state3 = fsm_get_state(child_fsm, STATE_3_ID);
	fsm_change_state_poll_interval(state2, 100);

	for(int i = 0; i < 51; i++) {
		fsm_poll(fsm);
		sysdelay_ms(1);
		fsm_get_current_state(fsm, &info);
		TEST_ASSERT_TRUE(info.id == STATE_1_ID);
		TEST_ASSERT_TRUE(strcmp(STATE_1_NAME, info.name) == 0);
	}
	fsm_switch_by_state_handle(fsm, state2);
	fsm_switch(fsm, STATE_1_ID);
	TEST_ASSERT(fsm_event_clear(fsm) == 0);
	for(int i = 0; i < 50; i++) {
		fsm_poll(fsm);
		sysdelay_ms(10);
		fsm_get_current_state(fsm, &info);
		TEST_ASSERT_TRUE(info.id == STATE_2_ID);
		TEST_ASSERT_TRUE(strcmp(STATE_2_NAME, info.name) == 0);
	}

	fsm_switch(fsm, STATE_1_ID);
	fsm_switch(fsm, STATE_2_ID);
	fsm_change_state_poll_interval(state1, FSM_NO_POLL);
	TEST_ASSERT_EQUAL_INT(fsm_state_child_fsm_add(state1, child_fsm), 0);
	fsm_print_info(fsm);
	fsm_print_info(child_fsm);
	for(int i = 0; i < 500; i++) {
		fsm_poll(fsm);
		sysdelay_ms(1);
		if(i % 50 == 0) {
			fsm_event_send(fsm, TEST_EVENT, &test_event_data, sizeof(int));
		}
	}

	// Test nested polling event in child fsm when parent fsm is set to FSM_NO_POLL
	fsm_change_state_poll_interval(state3, 20);
	for(int i = 0; i < 100; i++) {
		fsm_poll(fsm);
		sysdelay_ms(1);
	}

	TEST_ASSERT_EQUAL_INT(fsm_state_child_fsm_del(state1, child_fsm), 0);

	TEST_ASSERT_EQUAL_INT(fsm_state_del_by_name(fsm, STATE_1_NAME), 0);
	TEST_ASSERT_EQUAL_INT(fsm_state_del_by_name(fsm, STATE_2_NAME), 0);
	TEST_ASSERT_EQUAL_INT(fsm_state_del_by_name(child_fsm, STATE_3_NAME), 0);

	// state_del() will handle states that has not been unregistered
	TEST_ASSERT_EQUAL_INT(fsm_state_add(fsm, STATE_3_NAME, STATE_3_ID, 0), 0);
	TEST_ASSERT_EQUAL_INT(fsm_state_add(fsm, STATE_2_NAME, STATE_2_ID, 0), 0);
	TEST_ASSERT_EQUAL_INT(fsm_state_add(fsm, STATE_1_NAME, STATE_1_ID, 0), 0);

	char statename_list[200];
	fsm_get_state_list_csv(fsm, statename_list);
	printf("statename_list: %s\r\n", statename_list);
	TEST_ASSERT_EQUAL_INT(strcmp(statename_list, STATE_3_NAME "," STATE_2_NAME "," STATE_1_NAME),
						  0);

	TEST_ASSERT_EQUAL_INT(fsm_del(&fsm), 0);
	TEST_ASSERT_NULL(fsm);
	TEST_ASSERT_EQUAL_INT(fsm_del(&child_fsm), 0);
	TEST_ASSERT_NULL(child_fsm);

	printf("Correct sequence: %s\r\n", correct_seq_str);
	printf("Actual sequence : %s\r\n", test_seq_buf);
	printf("sta1_run_cnt = %d, sta2_run_cnt = %d, sta3_run_cnt = %d,\r\n",
		   sta1_run_cnt,
		   sta2_run_cnt,
		   sta3_run_cnt);
	printf("test_event_data = %d\r\n", test_event_data);

	TEST_ASSERT_TRUE(strcmp(test_seq_buf, correct_seq_str) == 0);
	TEST_ASSERT_TRUE(sta1_run_cnt == 20);
	TEST_ASSERT_TRUE(sta2_run_cnt == 7);
	TEST_ASSERT_TRUE(sta3_run_cnt == 16);
	TEST_ASSERT_TRUE(test_event_data == 10);

	uint32_t heap_end = GET_HEAP_FREE_SIZE();
	printf("heap_begin = %lu, heap_end = %lu, delta = %d\r\n",
		   heap_begin,
		   heap_end,
		   (int)heap_begin - (int)heap_end);
	TEST_ASSERT_TRUE_MESSAGE(heap_begin - heap_end <= 336, "Memory leak test failed");
}

#ifdef __cplusplus
}
#endif
