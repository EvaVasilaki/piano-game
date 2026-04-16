#include <assert.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

#include "ADTList.h"
#include "ADTVector.h"
#include "k08midi.h"
#include "state.h"


// Οι ολοκληρωμένες πληροφορίες της κατάστασης του παιχνιδιού.
// Ο τύπος State είναι pointer σε αυτό το struct, αλλά το ίδιο το struct
// δεν είναι ορατό στον χρήστη.

struct state {
	struct state_info info;  // Γενικές πληροφορίες για την κατάσταση του παιχνιδιού
	MidiFile midi_file;      // Το .mid αρχείο που διαβάσαμε με τη k08midi_file_read
	List midi_events;        // MIDI events προς αναπαραγωγή
	Vector clips;            // Clip προς ηχογράφηση
	uint recording_index;    // index του τρέχοντος (ή επόμενου) clip
	Vector midi_events_vec; // Ταξινομημένο vector με events κατα τον χρόνο
	Vector song_notes_vec; // Ταξινομημένο vector με τις νότες του τραγουδιού
};

// Τμήμα του τραγουδιού στο οποίο υπάρχει marker "rec" και ο χρήστης πρέπει να
// ηχογραφήσει, παίζοντας τις νότες από το αντίστοιχο channel του τραγουδιού. Το
// ηχογραφημένο τμήμα αναπαράγεται σε loop στα σημεία που υπάρχουν οι
// αντίστοιχοι "play" markers.

typedef struct clip {
	String name;		// όνομα
	int channel;		// MIDI κανάλι
	double start;		// χρονικό σημείο έναρξης
	double duration;	// διάρκεια
	List recorded;  	// Ηχογραφημένα MIDI_NOTE events
	List plays;     	// Λίστα με τα σημεία αναπαραγωγής του clip (τύπος ClipPlay)
}* Clip;

// Τμήμα αναπαραγωγής ενός clip, που προκύπτει από τα "play" markers.

typedef struct clip_play {
	double start;		// χρονικό σημείο έναρξης
	double duration;	// διάρκεια
}* ClipPlay;


// Bοηθητικές συναρτήσεις /////////////////////////////////////////////////////////////////////////////////
//
// Δημιουργεί και επιστρέφει ένα σημείο αναπαραγωγής clip
static ClipPlay create_clip_play(double start, double duration) {
	ClipPlay play = malloc(sizeof(*play));
	play->start = start;
	play->duration = duration;
	return play;
}

// Δημιουργεί και επιστρέφει ένα clip
static Clip create_clip(State state, String name, int channel, double start, double duration) {
	Clip clip = malloc(sizeof(*clip));
	clip->name = strdup(name);
	clip->channel = channel;
	clip->start = start;
	clip->duration = duration;
	clip->recorded = list_create(free);
	clip->plays = list_create(free);
	return clip;
}

// Καταστρέφει ένα clip
static void destroy_clip(Pointer value) {
	Clip clip = value;
	free(clip->name);
	list_destroy(clip->recorded);
	list_destroy(clip->plays);
	free(clip);
}

// Βρίσκει και επιστρέφει ένα clip με βάση το όνομά του, NULL αν δεν υπάρχει.
static Clip find_clip_by_name(State state, String clip_name) {
	for (int ci = 0; ci < vector_size(state->clips); ci++) {
		Clip clip = vector_get_at(state->clips, ci);
		if (strcmp(clip->name, clip_name) == 0)
			return clip;
	}
	return NULL;
}

//Helper συνάρτηση δυαδικής αναζήτησης με vector με σκοπό την καλύτερη απόδοση των συναρτήσεων
static int vectorized_binary_search(Vector vec, double target_time){
    //Πάνω κ κάτω όριο της δυαδικής αναζήτησης
	int lo = 0;
    int hi = vector_size(vec);

	//Κλασσικό binary search για έγκυρο διάστημα [lo, hi]
    while(lo < hi){
		//Υπολογισμός της θέσης στο μέσο
        int mid = lo + (hi-lo)/2;
		//Midi event στη θέση mid
        MidiEvent ev = vector_get_at(vec, mid);
		//Αν ο χρόνος είναι μικρότερος απο το target_time
		//τοτε η θέση βρίσκεται δεξιά απο το mid
        if(ev->time < target_time){
            lo = mid + 1;
        }
		//Αλλιώς είτε ειναι το mid η θέση που αναζητείται είτε πιο αριστερά από αυτό
        else{
			hi = mid;
		}
    }
	//Επιστροφή θέσης που ανήκει το event που αναζητείται
    return lo;

}

static void state_midi_events_vec_insertation(State state, MidiEvent ev){
	//Πρόσθεση του νέου event και στη λίστα midi events 
	//ώστε να υπάρχει και στη κλασσικ΄η δομή αναπαραγωγής
	list_insert_next(state->midi_events, list_last(state->midi_events), ev);
	//Εύρεση της σωστής θέσης στο vector ώστε να παραμείνει ταξινομημένο κατά χρόνο
	int pos = vectorized_binary_search(state->midi_events_vec, ev->time);
	//Αύξηση του vector
	vector_insert_last(state->midi_events_vec, NULL);
	//Μετακίνηση όλων των στοιχείων μίας θέσης δεξιά απο το τέλος μέχρι τη θέση pos
	for(int i = vector_size(state->midi_events_vec) - 1; i > pos; i--){
		vector_set_at(state->midi_events_vec, i, vector_get_at(state->midi_events_vec, i-1));
	}
	//Τοποθέτηση νέου event στη σωστή ταξινομημένη θέση
	vector_set_at(state->midi_events_vec, pos, ev);

}


//Δημιουργία vector που περιέχει μονο τα MIDI_NOTE events του τραγουδιού
static void state_song_notes_vec_insertation(State state){

	state->song_notes_vec = vector_create(0,NULL); 
	//Σάρωση όλων των events του midi file
	for(ListNode node = list_first(state->midi_file->events); node!=LIST_EOF; node=list_next(state->midi_file->events, node)){
		MidiEvent ev = list_node_value(state->midi_file->events, node);
		//Μόνο note events
		if(ev->type!= MIDI_NOTE){
			continue;
		}

		vector_insert_last(state->song_notes_vec, ev);
	}

}

// Αντιγράφει στο state->midi_events όλα τα PROGRAM_CHANGE (αλλαγή οργάνου) και
// CONTROL_CHANGE (έλεγχος volume, modulation, sustain, κλπ), ώστε να είναι
// έτοιμα για αναπαραγωγή μαζί με τις ηχογραφημένες νότες. 
static void append_song_control_events(State state) {
	for (ListNode node = list_first(state->midi_file->events); node != LIST_EOF; node = list_next(state->midi_file->events, node)) {
		MidiEvent event = list_node_value(state->midi_file->events, node);
		if (event->type == MIDI_CONTROL_CHANGE || event->type == MIDI_PROGRAM_CHANGE) {
			MidiEvent clone = malloc(sizeof(*clone));
			*clone = *event; // clone
			state_midi_events_vec_insertation(state, clone);
		}
	}
}

// Δημιουργεί clips και clip_plays με βάση τα markers του αρχείου MIDI
static void create_clips(State state) {
	state->clips = vector_create(0, destroy_clip);
	double measure_duration = state_measure_duration(state);

	for (ListNode node = list_first(state->midi_file->events); node != LIST_EOF; node = list_next(state->midi_file->events, node)) {
		MidiEvent event = list_node_value(state->midi_file->events, node);
		if (event->type != MIDI_MARKER)
			continue;
		int channel = 0;
		int measures = 0;
		char clip_name[51]; // στη sscanf διαβάζουμε το πολύ 50 χαρακτήρες για όνομα

		// rec και play markers
		if (sscanf(event->marker, "rec,%50[^,],%d,%d", clip_name, &channel, &measures) == 3) {
			// "rec,<name>,<channel>,<measures>" marker, ορίζει ένα clip προς ηχογράφηση
			Clip clip = create_clip(state, clip_name, channel, event->time, measures * measure_duration);
			vector_insert_last(state->clips, clip);

		} else if (sscanf(event->marker, "play,%50[^,],%d", clip_name, &measures) == 2) {
			// "play,<name>,<measures>" marker, ορίζει ένα σημείο αναπαραγωγής του clip
			Clip clip = find_clip_by_name(state, clip_name);
			assert(clip);	// το .mid αρχείο πρέπει να έχει φτιαχτεί σωστά, για κάθε play να προηγείται ένα rec
			list_insert_next(clip->plays, list_last(clip->plays), create_clip_play(event->time, measures * measure_duration));
		}
	}
}
/////////////////////////////////////////////////////////////////////////////////////////////////////

State state_create(String midi_file) {
	State state = malloc(sizeof(*state));
	assert(state != NULL);

	state->info.paused = true;
	state->info.game_over = false;
	state->info.accuracy = 0.0;
	state->info.score = 0.0;
	state->info.time = 0.0;
	state->info.level = 1;

	state->midi_file = NULL;
	state->midi_events = list_create(free);

	state->midi_file = k08midi_file_read(midi_file);
	assert(state->midi_file != NULL);
	state_song_notes_vec_insertation(state);

	state->midi_events_vec = vector_create(0, NULL);

	create_clips(state);
	assert(vector_size(state->clips) > 0);
	state->recording_index = 0;

	list_destroy(state->midi_events);
	state->midi_events = list_create(free);
	append_song_control_events(state);

	return state;
}

StateInfo state_info(State state) {
	return &state->info;
}

int state_playing_channel(State state) {
	int idx = state->recording_index;
	Clip current_clip = (Clip)vector_get_at(state->clips, idx);
	return current_clip->channel;
}

double state_measure_duration(State state) {
	MidiFile midi = state->midi_file;
	return midi->time_signature[0] * (60.0 / midi->tempo) * (4.0 / midi->time_signature[1]);
}

double state_total_duration(State state) {
	double max_midi_event = 0.0;
	for(ListNode node = list_first(state->midi_file->events); node!=LIST_EOF; node = list_next(state->midi_file->events, node)){
		MidiEvent event = (MidiEvent)list_node_value(state->midi_file->events, node);
		if(event->time > max_midi_event){
			max_midi_event = event->time;
		}
	}
	return max_midi_event;	
}

//Δημιουργία λίστας που επιστρέφει τις νότες που πρέπει να εμφανιστούν
List state_displayed_notes(State state, double time_window) {
	List result = list_create(NULL);
	//Αν δεν υπάρχουν άλλα clip, σταματάει
	if(state->recording_index >= vector_size(state->clips)){
		return result;
	}
	//Τρέχων κλιπ
	Clip current_clip = vector_get_at(state->clips, state->recording_index);

	
	double start = state->info.time; //τρέχων χρόνος
	double end = state->info.time + time_window;//τρέχων χρόνος + time_window δευτερόλεπτα
	//Χρονικά όρια του τρέχοντος clip
	double clip_start = current_clip->start;
	double clip_end = current_clip->start + current_clip->duration;

	//Περιορισμός visible window για notes εκτός clip
	if(end>clip_end){
		end = clip_end;
	}
	//Με τη βοηθητική vectorized binary search, βρίσκεται η πρώτη πιθανή θέση στο vector song_notes_vec
	int pos = vectorized_binary_search(state->song_notes_vec, clip_start);

	//iteration των note events απο τη θέση pos και μετά
	for(int i = pos; i < vector_size(state->song_notes_vec); i++){
		MidiEvent note_on = vector_get_at(state->song_notes_vec, i);
		//Έλεγχος για MIDI_NOTES
		if(note_on->type != MIDI_NOTE){
			continue;
		}
		//Εφόσον τα events είναι χρονικά ταξινομημένα, δεν χρειάζεται αν περάσει το end η συνέχεια
		if(note_on->time>end){
			break;
		}
		//Έλεγχος για νότες μόνο του channel του τρέχοντος clip
		if(note_on->channel != current_clip->channel){
			continue;
		}
		//Έλεγχος μόνο για νότες που ανήκουν χρονικά στο clip
		if(note_on->time < clip_start || note_on->time>clip_end){
			continue;
		}
		//Αντίστοιχη note off της note_on που μόλις βρέθηκε, ω΄στε να φαίνεται στην οθόνη για όσο διαρκεί
		MidiEvent note_off = NULL;
		for(int j = i+1; j<vector_size(state->song_notes_vec); j++){
			MidiEvent ev = vector_get_at(state->song_notes_vec, j);
			if(ev->type !=MIDI_NOTE){
				continue;
			}
			if(ev->velocity > 0){
				continue;
			}
			if(ev->channel != note_on->channel){
				continue;
			}
			if(ev->key != note_on->key){
				continue;
			}
			note_off = ev;
			break;
		}
		//Για μη εύρεση του note off θεωρείται οτι η νότα διαρκεί μέχρι το τέλος του clip
		double note_end = clip_end;
		if(note_off != NULL){
			note_end = note_off->time;
		}
		//Mία νότα εμφανίζεται εαν έχει αρχίσει πριν ή μέσα στο end και δεν έχει τελειώσει πριν το start
		if(note_on->time <= end && note_end >= start){
			list_insert_next(result, list_last(result), note_on);
			//Αν υπάρχει note off , εισάγεται κ αυτό
			if(note_off!=NULL){
				list_insert_next(result, list_last(result), note_off);
			}
		}
	}
	//Επιστροφή αποτελέσματος
	return result;
}


List state_playback_events(State state, double since) {//Δημιουργία λίστας που θα περιέχει τα midi events που πρέπει να αναπαραχθούν απο το προηγούμενο frame μεχρι τώρα
	List result = list_create(NULL);
	//Με binary search εύρεση της πρώτης θέσης στο midi_events_vec όπου υπάρχει event με time>= since
	int start = vectorized_binary_search(state->midi_events_vec, since);
	//iteration απο εκεί και μετα για τα απαιτούμενα events
	for(int i = start; i<vector_size(state->midi_events_vec); i++){
		MidiEvent current = vector_get_at(state->midi_events_vec, i);
		//Αν το event έγινε πριν απο το χρονικό σημείο since έχει ήδη αναπαραχθεί αρα αγνοείται
		if(current-> time < since)
		continue;
		//Αν το event είναι μετά απο το current time του παιχνιδιού τότε δεν πρέπει να παιχτεί ακόμα
		//και επειδή η λίστα είναι ταξινομημένη, σταματάει κ η συνάρτηση
		if(current->time > state->info.time)
		break;
		//εισαγωγή των event που ανήκουν στο διάστημα [since, current_time]
		list_insert_next(result, list_last(result), current);
	}
	//επιστροφή αποτελέσματος
	return result;
}

//helper συνάρτηση που βοηθάει στον υπολογισμό του accuracy ανάλογα το level
static double required_acc(int level){
	double req = 0.5 + 0.05*(level-1);
	if(req> 0.9){
		req = 0.9;
	}
	return req;
}

//helper για το reinitialization του state μετά την state update
static void state_reinitialize(State state){
	state->recording_index = 0;
	state->info.time = 0.0;
	state->info.score = 0.0;
	state->info.accuracy = 0.0;
	state->info.game_over = false;
	state->info.paused = true;

	//Διαγραφή προηγούμενων ηχογραφήσεων και δημιουργία καινούριας κενής λιστας για recorded
	for(int i = 0; i < vector_size(state->clips); i++){
		Clip clip = vector_get_at(state->clips, i);
		list_destroy(clip->recorded);
		clip->recorded = list_create(free);
	}

	//Διαγραφή όλων των playback event που είχαν δημιουργηθεί απο προηγούμενες ηχογραφήσεις
	list_destroy(state->midi_events);
	state->midi_events = list_create(free);
	append_song_control_events(state);
}


void state_update(State state, KeyState ks, double elapsed_time) {
	//Βοηθητική μεταβλητή που καθορίζει εαν θα γίνει update το frame
	bool frame_update = false;
	//Έλεγχος για game_over κατάσταση
	if(state->info.game_over){
		if(ks->space){
			//Εαν είναι game over και πατηθεί space, βγαίνει απο game over και ξαναρχίζει το παιχνίδι 
			state->info.game_over = false;
			state_reinitialize(state);
		}
		else{
			return;
		}
	}
	//paused
	//Εαν είναι paused
	if(!state->info.paused && ks->space){
		state->info.paused = true;//με space συνεχίζει
		return;
	}
	if(state->info.paused){
		if(ks->space){
			state->info.paused = false;
			return;
		}
		else if(ks->n){//με n προχωράει μόνο ένα frame
			frame_update = true;
		}
		else {//αλλιως δεν προχωράει ο χρόνος
			return;
		}
	}
	//Αν δεν είναι game over ουτε paused, τοτε προχωράει ο χρόνος κανονικά
	if(!state->info.paused && !state->info.game_over){
		frame_update = true;
	}
	//frame_update
	if(frame_update){
		//Διατήρηση του παλιού χρόνου και συνέχεια του χρόνου του παιχνιδιού
		double old_time = state->info.time;
		state->info.time += elapsed_time;
		//Τρέχον clip
		uint idx = state->recording_index;
		Clip current = (Clip)vector_get_at(state->clips, idx);
		double current_end = current->start + current->duration;
		if(current->start <= state->info.time && state->info.time <= current->start + current->duration){//ελεγχος ενεργου κλιπ 
			//Για κάθε MIDI note (0...127) αν η κατάσταση άλλαξε σε αυτό το frame
			//αποθηκεύεται το αντίστοιχο MIDI_NOTE event στο recorded του clip
			for(int i = 0; i<128; i++){
				if(ks->changed_keys[i] >= 0){
					MidiEvent event = malloc(sizeof(*event));
					event->type = MIDI_NOTE;
					event->channel = current->channel;
					event->key = i;
					event->time = state->info.time;
					event->velocity = ks->active_keys[i];
					list_insert_next(current->recorded, list_last(current->recorded), event);
				}
			}
		}
		
		//Αν το clip μόλις τελείωσε ανάμεσα στο προηγούμενο και το τωρινό frame 
		else if(old_time <= current_end && current_end < state->info.time){
			//Για όσα πλήκτρα μένουν πατημένα μέχρι το τέλος του clip
			//κλείνει η νότα με velocity = 0 στο current_end
			for(int i = 0; i<128; i++){
				if(ks->active_keys[i] > 0){
					MidiEvent event = malloc(sizeof(*event));
					event->type = MIDI_NOTE;
					event->channel = current->channel;
					event->key = i;
					event->time = current_end;
					event->velocity = 0;
					list_insert_next(current->recorded, list_last(current->recorded), event);
				}
			}
			//Επόμενο clip εαν υπάρχει
			if(state->recording_index < vector_size(state->clips)-1){
				state->recording_index++;
			}
			//Δημιουργία playback events απο το clip που μόλις ηχογραφήθηκε
			for(ListNode node = list_first(current->plays); node!=LIST_EOF; node = list_next(current->plays, node)){
				ClipPlay currentPlay = list_node_value(current->plays, node);
				double play_end = currentPlay->start + currentPlay->duration;
				//Κάθε event που γράφτηκε στο clip
				for(ListNode node2 = list_first(current->recorded); node2!=LIST_EOF; node2 = list_next(current->recorded, node2)){
					MidiEvent current_event = list_node_value(current->recorded, node2);
					//Υπολογισμος offset του event μεσα στο clip
					double offset = current_event->time - current->start;
					int k = 0;
					//Τοποθέτηση αντιγράφων του event σε loop μέσα στο play section
					while(currentPlay->start + k* current->duration + offset < play_end){
						MidiEvent copy = malloc(sizeof(*copy));
						copy->channel = current_event->channel;
						copy->type = current_event->type;
						copy->velocity = current_event->velocity;
						copy->key = current_event->key;
						copy->time = currentPlay->start + k* current->duration + offset;
						//εισαγωγη του νέου event ταξινομημένα στο midi_events_vec/list
						state_midi_events_vec_insertation(state, copy);
						k++;
					}
				}
			}
			//Υπολογισμ΄ός του score και του accuracy για τα ολοκληρωμένα clip
			double total_score = 0.0;
			double total_accuracy = 0.0;
			int completed_clips = 0;
				
			for(int i = 0; i<vector_size(state->clips); i++){
				Clip clip = vector_get_at(state->clips, i);
				double clip_end = clip->start + clip->duration;
				//Αν το clip δεν έχει ολοκληρωθεί δεν βαθμολογείται
				if(state->info.time < clip->start+clip->duration){
					continue;
				}
				double clip_accuracy = 0.0;
				
				//Δημιουργία λίστας με τις σωστές νότες του τραγουδιού για αυτό το clip
				List clip_song = list_create(NULL);
				for(ListNode node = list_first(state->midi_file->events); node!=LIST_EOF; node=list_next(state->midi_file->events, node)){
					MidiEvent event = list_node_value(state->midi_file->events, node);
					if(event->type != MIDI_NOTE){
						continue;
					}
					if(event->time < clip->start){
						continue;
					}
					if(event->channel != clip->channel){
						continue;
					}
					if(event->time > clip_end){
						break;
					}
					list_insert_next(clip_song, list_last(clip_song), event);
				}
				//Μετρηση note_on που υπάρχουν στην ηχογράφηση του χρήστη
				int count1 = 0;
				for(ListNode node = list_first(clip->recorded); node != LIST_EOF; node= list_next(clip->recorded, node)){
					MidiEvent ev = list_node_value(clip->recorded, node);
					if(ev->velocity<=0){
						continue;
					}
					count1++;
				}
				//Μέτρηση των note_on στο σωστό clip του τραγουδιού
				int count2 = 0;
				for(ListNode node = list_first(clip_song); node != LIST_EOF; node= list_next(clip_song, node)){
					MidiEvent ev = list_node_value(clip_song, node);
					if(ev->velocity<=0){
						continue;
					}
					count2++;
				}
				double clip_score_total = 0.0;
				double note_accuracy= 0.0;
				
				int played_note_on_count = count1;
				int song_note_on_count = count2;
				//Το max χρησιμοποιείται για normalization της ακρίβειας
				int max = 0;
				if(played_note_on_count > song_note_on_count){
					max = played_note_on_count;
				}
				else{
					max =song_note_on_count;
				}
				//Σύγκριση κάθε ηχογραφημένης νότας με τις νότες του σωστού clip
				for(ListNode node1 = list_first(clip->recorded); node1!=LIST_EOF; node1=list_next(clip->recorded, node1)){
					MidiEvent recNoteOn = list_node_value(clip->recorded, node1);
					MidiEvent recNoteOff = NULL;
					double recNoteDuration = 0.0;
					double time_diff = 0.0;
					double duration_diff = 0.0;
					double diff = 0.0;
					double score = 0.0;
					
					//Μόνο note on 
					if(recNoteOn->velocity<= 0){
						continue;
					}
					//Εύρεση του αντίσοτιχου note off της ηχογραφημένης νότας
					for(ListNode node3 = list_next(clip->recorded,node1); node3!=LIST_EOF; node3=list_next(clip->recorded, node3)){
						MidiEvent ev = list_node_value(clip->recorded, node3);
						if(ev->velocity > 0){
							continue;
						}
						if(ev->key != recNoteOn->key){
							continue;
						}
						recNoteOff = ev;
						break;
					}
					if(recNoteOff == NULL){
						continue;
					}
					recNoteDuration = recNoteOff->time - recNoteOn->time;\
					//Αναζήτηση matching νότας στο σωστό clip_song
					for(ListNode node2 = list_first(clip_song); node2!=LIST_EOF; node2=list_next(clip_song, node2)){
						MidiEvent clipSongNoteOn = list_node_value(clip_song, node2);
						MidiEvent clipSongNoteOff = NULL;
						double clipSongNoteDuration = 0.0;
						double timeDiff = clipSongNoteOn->time - recNoteOn->time;

						if(clipSongNoteOn->velocity<=0){
							continue;
						}
						if(clipSongNoteOn->key != recNoteOn->key){
							continue;
						}
						//Επιστροφή μικρού timing error μέχρι 0.1 sec
						if(timeDiff < 0){
							if(-1*timeDiff > 0.1){
								continue;
							}
						}
						else if(timeDiff > 0.1){
							continue;
						}
						//Εύρεση του note off της σωστής νότας
						ListNode node_off = LIST_EOF;
						for(ListNode node4 = list_next(clip_song,node2); node4!=LIST_EOF; node4=list_next(clip_song, node4)){
							MidiEvent ev = list_node_value(clip_song, node4);
							if(ev->velocity > 0){
								continue;
							}
							if(ev->key != clipSongNoteOn->key){
								continue;
							}
							clipSongNoteOff = ev;
							node_off = node4;
							break;
						}
						if(clipSongNoteOff == NULL){
							continue;
						}
						clipSongNoteDuration = clipSongNoteOff->time - clipSongNoteOn->time;
						//Υπολογισμός διαφοράς timing 
						if(timeDiff<0){
							timeDiff = -1*timeDiff;
						}
						time_diff = timeDiff;
						//Στα drums δεν μετριέται duration difference
						if(clipSongNoteOn->channel == 9){
							duration_diff = 0.0;
						}
						else{
						duration_diff = clipSongNoteDuration - recNoteDuration;
						}
						if(duration_diff < 0){
							duration_diff = -1*duration_diff;
						}
						//Τελική διαφορά
						diff = time_diff + duration_diff;
						//Υπολογισμός score
						score = exp((-1)*(diff/0.1)*(diff/0.1));
						clip_score_total += score;
						if(max == 0){
							note_accuracy = 0;
						}
						else{
						note_accuracy = score/max;
						}
						clip_accuracy += note_accuracy;
						//Ευθυγράμμιση των ηχογραφημένων notes στις σωστές χρονικές στιγμές
						recNoteOn->time = clipSongNoteOn->time;
						recNoteOff->time = clipSongNoteOff->time;
						//Αφαίρεση της matched note απο το clip_song ώστε να μη χρησιμοποιηθεί ξανά
						if(node2 == list_first(clip_song)){
						list_remove_next(clip_song, LIST_BOF);
						}
						else{
							for(ListNode prev = list_first(clip_song); prev!=LIST_EOF; prev = list_next(clip_song, prev)){
								if(list_next(clip_song, prev) != node2){
									continue;
								}
								list_remove_next(clip_song, prev);
								break;
							}
					
						}
						//Αφαίρεση αντίστοιχου note off
						if(node_off == list_first(clip_song)){
						list_remove_next(clip_song, LIST_BOF);
						}
						else{
							for(ListNode prev = list_first(clip_song); prev!=LIST_EOF; prev = list_next(clip_song, prev)){
								if(list_next(clip_song, prev) != node_off){
									continue;
								}
								list_remove_next(clip_song, prev);
								break;
							}
					
						}
						//Διακοπή μετά την εύρεση της matched note
						break;
					}
				}
				//Πρόσθεση του score/accuracy αυτού του clip στα συνολικά
				total_score += clip_score_total;
				total_accuracy += clip_accuracy;
				completed_clips++;
				list_destroy(clip_song);
			}
			//Ενημέρωση των συνολικών στοιχείων του παιχνιδιού
			state->info.score = total_score;
			if(completed_clips == 0){
				state->info.accuracy = 0.0;
			}
			else{
			state->info.accuracy = total_accuracy/completed_clips;
			}
			//Έλεγχος τέλους τραγουδιού 
			if(state->info.time >= state_total_duration(state)){
				//Αν ο παικτης έπιασε το απαιτούμενο accuracy προχωρά στο επόμενο level και το state ξανα αρχικοποιείται
				if(state->info.accuracy >= required_acc(state->info.level)){
					state->info.level++;
					state_reinitialize(state);
					
				}
				//Αλλιώς το παιχνίδι τελειώνει
				else{
					state->info.game_over = true;
				}
			}
		}
	}
}




//Διγραφή και αποδέσμευση μνήμης
void state_destroy(State state) {
	vector_destroy(state->song_notes_vec);
	vector_destroy(state->midi_events_vec);
	k08midi_file_destroy(state->midi_file);
	list_destroy(state->midi_events);
	vector_destroy(state->clips);
	free(state);
}