#pragma once

#include "raylib.h"
#include "k08midi.h"
#include "ADTList.h"

#define SCREEN_WIDTH 1200	// Πλάτος της οθόνης
#define SCREEN_HEIGHT 800	// Ύψος της οθόνης

// Όλοι οι χρόνοι και οι διάρκειες εκφράζονται σε δευτερόλεπτα.

// Γενικές πληροφορίες για την κατάσταση του παιχνιδιού
typedef struct state_info {
	bool paused;					// true αν το παιχνίδι είναι paused
	bool game_over;					// true αν το παιχνίδι είναι σε game over
	double time;					// Ο τρέχων χρόνος στο τραγούδι
	double accuracy;				// Ακρίβεια παιξίματος
	double score;					// Σκορ
	int level;						// 1-10
}* StateInfo;

// Πληροφορίες για την κατάσταση των πλήκτρων
typedef struct key_state {
	bool space;                     // true αν πατείται space
	bool n;							// true αν πατείται n

	// Πίνακες με το πλήρες MIDI εύρος, 0: C-1 (πολύ χαμηλή Ντο), 127: G9 (πολύ υψηλή Σολ)
	// Ο πίνακας active_keys περιλαμβάνει την τρέχουσα κατάσταση κάθε πλήκτρου,
	// ενώ ο changed_keys περιλαμβάνει μόνο τα πλήκτρα που άλλαξαν τώρα.
	char active_keys[128];			// Τρέχων velocity (0: μη πατημένο, >0: πατημένο)
	char changed_keys[128];			// <0 αν δεν άλλαξε, >=0 αν άλλαξε μόλις
}* KeyState;

// Η κατάσταση του παιχνιδιού (handle)
typedef struct state* State;


// Δημιουργεί και επιστρέφει την αρχική κατάσταση του παιχνιδιού με βάση το τραγούδι midi_file

State state_create(char* midi_file);

// Επιστρέφει τις βασικές πληροφορίες του παιχνιδιού στην κατάσταση state

StateInfo state_info(State state);

// Επιστρέφει το channel που πρέπει να χρησιμοποιείται για live playing.

int state_playing_channel(State state);

// Επιστρέφει τη διάρκεια ενός μέτρου του τραγουδιού (σε δευτερόλεπτα).
// Θεωρούμε ότι η διάρκεια είναι σταθερή σε όλο το τραγούδι.

double state_measure_duration(State state);

// Επιστρέφει τη συνολική διάρκεια (σε δευτερόλεπτα) του τραγουδιού.

double state_total_duration(State state);

// Επιστρέφει μια λίστα με νότες (MIDI_NOTE events) που εμφανίζονται για να
// παίξει ο χρήστης, από την τρέχουσα χρονική στιγμή μέχρι και time_window στο
// μέλλον.  Η λίστα είναι ταξινομημένη σε αύξουσα σειρά ως προς event->time.

List state_displayed_notes(State state, double time_window);

// Επιστρέφει μια λίστα με MIDI events προς αναπαραγωγή (ηχογραφημένες νότες και
// control/program changes από το αρχικό τραγούδι), από "since" δευτερόλεπτα στο
// παρελθόν μέχρι και την τρέχουσα χρονική στιγμή.  Η λίστα επιστρέφεται
// ταξινομημένη σε μη-φθίνουσα σειρά ως προς event->time.

List state_playback_events(State state, double since);

// Ενημερώνει την κατάσταση state του παιχνιδιού μετά την πάροδο elapsed_time
// χρόνου, και με βάση την κατάσταση πλήκτρων ks.

void state_update(State state, KeyState ks, double elapsed_time);

// Καταστρέφει την κατάσταση state ελευθερώνοντας τη δεσμευμένη μνήμη.

void state_destroy(State state);
