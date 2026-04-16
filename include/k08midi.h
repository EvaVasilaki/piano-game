///////////////////////////////////////////////////////////
//
// k08midi
//
// Βοηθητική βιβλιοθήκη για MIDI αρχεία, MIDI input και synth.
//
///////////////////////////////////////////////////////////

#pragma once // #include το πολύ μία φορά

#include <stdbool.h>
#include <stdint.h>

#include "ADTList.h"
#include "ADTVector.h"

typedef char* String;

// Τύποι MIDI events που υποστηρίζονται από τη βιβλιοθήκη.
typedef enum {
    MIDI_NOTE,
    MIDI_CONTROL_CHANGE,
    MIDI_PROGRAM_CHANGE,
    MIDI_MARKER,
    MIDI_LYRIC,
} MidiEventType;

// Ένα MIDI event (χρόνος σε seconds).
typedef struct midi_event {
	double time;			// seconds
	uint8_t channel;
	MidiEventType type;
	union {
		// MIDI_NOTE:
		// Χρησιμοποιείται για note on / note off σε ένα κανάλι.
		// key: αριθμός νότας (0..127), velocity: ένταση (0 => note off).
		struct {
			uint8_t key;
			uint8_t velocity;
		};
		// MIDI_CONTROL_CHANGE:
		// Χρησιμοποιείται για αλλαγή controller παραμέτρων (π.χ. volume, modulation).
		// control: αριθμός controller, control_value: τιμή controller (0..127).
		struct {
			uint8_t control;
			uint8_t control_value;
		};
		// MIDI_PROGRAM_CHANGE:
		// Χρησιμοποιείται για αλλαγή ήχου/οργάνου (preset/program) στο κανάλι.
		// program: αριθμός προγράμματος (0..127).
		struct {
			uint8_t program;
		};
		// MIDI_MARKER:
		// Meta event με λεκτική σήμανση θέσης/ενότητας στο κομμάτι.
		// marker: κείμενο του marker.
		struct {
			String marker;
		};
		// MIDI_LYRIC:
		// Meta event με στίχους/λεκτικό περιεχόμενο συγχρονισμένο με τον χρόνο.
		// lyric: κείμενο των στίχων.
		struct {
			String lyric;
		};
	};
}* MidiEvent;

// Ένα MIDI αρχείο όπως επιστρέφεται από το parser.
typedef struct midi_file {
	List events;
	double duration;			// seconds
	double tempo;				// BPM (assumed constant)
	uint8_t time_signature[2];	// numerator, denominator (assumed constant)
}* MidiFile;


// Ανάγνωση MIDI αρχείων ////////////////////////////////////////////////////////////////////////
//
// Διαβάζει MIDI αρχείο από path και επιστρέφει MidiFile, ή NULL αν αποτύχει.

MidiFile k08midi_file_read(String midi_path);

// Ελευθερώνει όλη τη μνήμη που δεσμεύει το MidiFile.

void k08midi_file_destroy(MidiFile midi_file);


// Αναπαραγωγή MIDI (synth) ////////////////////////////////////////////////////////////////////
//
// Αρχικοποιεί το synth backend. Επιστρέφει true σε επιτυχία.

bool k08midi_synth_init(String soundfont_path);

// Σταματά το audio stream και ελευθερώνει πόρους.

void k08midi_synth_close(void);

// Επιλέγει program για MIDI channel.

void k08midi_synth_set_program(uint8_t channel, uint8_t program, bool is_drums);

// Στέλνει MIDI control change (controller, value 0..127).

void k08midi_synth_set_control(uint8_t channel, uint8_t control, uint8_t value);

// Στέλνει MIDI note event (0 => note_off, 1..127 => note_on velocity).

void k08midi_synth_note(uint8_t channel, uint8_t key, uint8_t velocity);

// Σβήνει όλες τις νότες σε όλα τα κανάλια.

void k08midi_synth_note_off_all(void);


// Είσοδος MIDI (πχ από keyboard) /////////////////////////////////////////////////////////////
//
// Επιστρέφει τα διαθέσιμα ονόματα MIDI input συσκευών.  Το ίδιο Vector
// επαναχρησιμοποιείται σε κάθε κλήση και ανήκει στη βιβλιοθήκη.  Ο χρήστης δεν
// πρέπει να το καταστρέφει.  Στο active επιστρέφεται το index της ενεργής
// συσκευής.

Vector k08midi_input_devices(uint* active);

// Αλλάζει την ενεργή MIDI input συσκευή με ακριβές ταίριασμα ονόματος.

void k08midi_input_set_device(String device);

// Επιστρέφει τα MIDI note events που συνέβησαν από την προηγούμενη κλήση μέχρι
// τώρα.  Η λίστα περιέχει MidiEvent με type == MIDI_NOTE.  Η ίδια List
// επαναχρησιμοποιείται σε κάθε κλήση και ανήκει στη βιβλιοθήκη.  Ο χρήστης δεν
// πρέπει να την καταστρέφει.

List k08midi_input_get_events(void);
