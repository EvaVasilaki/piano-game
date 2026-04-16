//////////////////////////////////////////////////////////////////
//
// Test για το state.h module
//
//////////////////////////////////////////////////////////////////

#include <stdlib.h>
#include <math.h>
#include <memory.h>
#include "acutest.h"			// Απλή βιβλιοθήκη για unit testing

#include "state.h"


///// Βοηθητικές συναρτήσεις ////////////////////////////////////////
//
// σύγκριση double
static bool double_equal(double a, double b) {
	return fabs(a-b) < 1e-6;
}

// αρχικοποίηση πλήκτρων (κανένα πατημένο)
static struct key_state empty_key_state() {
	struct key_state ks = { .space=false, .n=false };
	memset(ks.active_keys, 0, sizeof(ks.active_keys));
	memset(ks.changed_keys, -1, sizeof(ks.changed_keys));
	return ks;
}
/////////////////////////////////////////////////////////////////////


void test_state_create() {
	State state = state_create("test.mid");
	TEST_ASSERT(state != NULL);

	StateInfo info = state_info(state);
	TEST_ASSERT(info != NULL);
	TEST_ASSERT(info->paused);
	TEST_ASSERT(!info->game_over);
	TEST_ASSERT(double_equal(info->time, 0.0));
	TEST_ASSERT(info->level == 1);

	state_destroy(state);
}

void test_state_basic_functions() {
	State state = state_create("test.mid");
	TEST_ASSERT(state != NULL);

	TEST_ASSERT(state_playing_channel(state) == 9);
	TEST_ASSERT(double_equal(state_measure_duration(state), 1.93548));
	TEST_ASSERT(double_equal(state_total_duration(state), 18.987));

	state_destroy(state);
}

void test_state_update() {
	State state = state_create("test.mid");
	TEST_ASSERT(state != NULL);

	// Παράδειγμα:
	struct key_state ks = empty_key_state();
	ks.space = true;

	state_update(state, &ks, 0.0);
	TEST_ASSERT(!state_info(state)->paused);

	ks = empty_key_state();
	ks.space = true;
	state_update(state, &ks, 0.0);
	TEST_ASSERT(state_info(state)->paused);
	
	state_destroy(state);
}

//////////////////////////////////////////////////////////////////////
///////////////////Test για την state_update/////////////////////////
void test_state_update2(){
	State state = state_create("test.mid");
	TEST_ASSERT(state!=NULL);

	TEST_ASSERT(state_info(state)->paused);
	TEST_ASSERT(double_equal(state_info(state)->time, 0.0));

	struct key_state ks = empty_key_state();
	ks.n = true;

	state_update(state, &ks, 2.0);
	TEST_ASSERT(state_info(state)->paused);
	TEST_ASSERT(double_equal(state_info(state)->time, 2.0));
	state_destroy(state);
}

//////////////////////////////////////////////////////////////////////
///////////////////Test για την state_update/////////////////////////
void test_state_update_recorded_events(){
	State state = state_create("test.mid");
	TEST_ASSERT(state!=NULL);

	struct key_state ks = empty_key_state();
	ks.space = true;
	state_update(state, &ks, 0.0);

	ks = empty_key_state();
	state_update(state, &ks, 4.0);

	ks = empty_key_state();
	ks.changed_keys[70] = 100;
	ks.active_keys[70] = 100;
	state_update(state, &ks, 0.2);

	ks = empty_key_state();
	state_update(state, &ks, 5.0);

	bool found = false;
	List events = state_playback_events(state, 0.0);
	TEST_ASSERT(events!=NULL);

	for(ListNode node = list_first(events); node!=LIST_EOF; node=list_next(events, node)){
		MidiEvent currentEvent = list_node_value(events, node);
		if(currentEvent->type == MIDI_NOTE && currentEvent->key == 70 && currentEvent->velocity == 100){
			found = true;
			break;
		}
	}

	TEST_ASSERT(found);
	list_destroy(events);
	state_destroy(state);
}
//////////////////////////////////////////////////////////////////
////// test για την state_update/////////////////////////////////
void test_state_update_score(){
	State state = state_create("test.mid");
	TEST_ASSERT(state!=NULL);

	struct key_state ks = empty_key_state();
	ks.space = true;
	state_update(state, &ks, 0.0);
	TEST_ASSERT(!state_info(state)->paused);

	ks = empty_key_state();
	state_update(state, &ks, 4.0);

	ks = empty_key_state();
	ks.changed_keys[70] = 100;
	ks.active_keys[70] = 100;
	state_update(state, &ks, 0.2);

	ks = empty_key_state();
	state_update(state, &ks, 5.0);

	TEST_ASSERT(state_info(state)->score >= 0.0);
	TEST_ASSERT(state_info(state)->accuracy >= 0.0);

	state_destroy(state);

}
/////////////////////////////////////////////////////////////////
//// TEST ΓΙΑ ΤΗΝ state_displayed_notes///
void test_state_displayed_notes(){
	State state = state_create("test.mid");
	TEST_ASSERT(state!=NULL);
	double window = 2.0;
	double start = state_info(state)->time;

	List notes = state_displayed_notes(state, window);
	TEST_ASSERT(notes!= NULL);
	double prev_time = -1.0;
	for(ListNode node = list_first(notes); node!=LIST_EOF; node = list_next(notes, node)){
		MidiEvent ev = list_node_value(notes, node);

		TEST_ASSERT(ev !=NULL);
		TEST_ASSERT(ev->type == MIDI_NOTE);
		TEST_ASSERT(ev->time >= start);
		TEST_ASSERT(ev->time <= start + window);

		if(prev_time >= 0.0){
			TEST_ASSERT(prev_time <= ev->time);
		}
		prev_time = ev->time;

	}

	list_destroy(notes);
	state_destroy(state);

}

/////////////////////////////////////////////////////////////////////////////
/////////Test gia thn state_playback_event/////////////////////////

void test_state_playback_events() {
	State state = state_create("test.mid");
	TEST_ASSERT(state!=NULL);

	struct key_state ks = empty_key_state();
	state_update(state, &ks, 5.0);

	double now = state_info(state)->time;
	double since = 2.0;

	List events = state_playback_events(state, since);
	TEST_ASSERT(events != NULL);

	double prev_time = -1.0;
	for(ListNode node = list_first(events); node!=LIST_EOF; node=list_next(events, node)){
		MidiEvent ev = list_node_value(events, node);

		TEST_ASSERT(ev!=NULL);
		TEST_ASSERT(ev->time>=now-since);
		TEST_ASSERT(ev->time<=now);

		if(prev_time>=0.0) {
			TEST_ASSERT(prev_time<=ev->time);
		}
		prev_time = ev->time;
	}

	list_destroy(events);
	state_destroy(state);

}





// Λίστα με όλα τα tests προς εκτέλεση
TEST_LIST = {
	{ "test_state_create", test_state_create },
	{ "test_state_basic_functions", test_state_basic_functions },
	{ "test_state_update", test_state_update },
	{ "test_state_displayed_notes", test_state_displayed_notes},
	{ "test_state_playback_events", test_state_playback_events},
	{ "test_state_update2", test_state_update2},
	{ "test_state_update_recorded_events", test_state_update_recorded_events},
	{ "test_state_update_score", test_state_update_score},
	{ NULL, NULL } // τερματίζουμε τη λίστα με NULL
};
