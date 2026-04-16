#include <string.h>
#include <stdio.h>
#include "raylib.h"
#include "state.h"
#include "interface.h"
#include <stdlib.h>
#include "k08midi.h"

//Δηλωση state_update που υλοποιείται στο state.c
void state_update(State state, KeyState ks, double elapsed_time);

//Δείχνει το πρώτο λευκό πλήκτρο του "παραθύρου" των 12 λευκών πλήκτρων
//που εμφανίζονται ως ενεργά στο interface.
int current_start_white = 7;

//Καθολική κατάσταση του παιχνιδιού
static State state;
//Δεδομένα κατάστασης πληκτρολογίου
static struct key_state key_state_data;
static KeyState ks = &key_state_data;

//Περιορισμός της τιμής του start_white στο επιτρεπτό διάστημα [0, 23]
static int clamp_start_white(int start_white){
    if(start_white<0){
        return 0;
    }
    if(start_white > 23){
        return 23;  
    }
    return start_white;
}

//Μετατροπής index λευκού πλήκτρου σε MIDI pitch.
//Το πιάνο ξεκινά από pitch 24 και κάθε οκτάβα έχει λευκά στις θέσεις:
// C D E F G A B -> 0,2,4,5,7,9,11
static int white_index_to_pitch(int white_index){
    static const int white_pc[7] = {0, 2, 4, 5, 7, 9, 11};
    int octave = white_index /7;
    int pos = white_index%7;
    return 24+octave*12+white_pc[pos];
}

//Ανάλογα με τη θέση ενός λευκού πλήκτρου, βρίσκει αν υπάρχει μαύρο πλήκτρο δεξιά του
// και επιστρέφει μέσω pointer το αντίσοιχο MIDI pitch
//Επιστρέφει true αν υπάρχει μαύρο πλήκτρο, αλλιώς false.
static bool black_index_to_pitch(int white_index, int *pitch){
    int pos = white_index%7;
    int octave = white_index/7;

    switch(pos){
        case 0: *pitch = 24 + octave *12+1; return true;
        case 1: *pitch = 24 + octave *12+3; return true;
        case 3: *pitch = 24 + octave *12+6; return true;
        case 4: *pitch = 24 + octave *12+8; return true;
        case 5: *pitch = 24 + octave *12+10; return true;
    }
    return false;
}

//Μηδενισμός της κατάστασης πληκτρολογίου στην αρχή του παιχνιδιού
static void reset_keyboard_state(KeyState ks){
    ks->space = false;
    ks->n = false;
    for(int i =0; i<128; i++){
        ks->active_keys[i]= 0;
        ks->changed_keys[i] = -1;
    }
}

//Συλλογή όλων των pitches που αντιστοιχούν στο ενεργό "παράθυρο" του πληκτρολογίου:
//12 λευκά + 8 μαύρα = 20 pitches
//Στο τέλος ταξινομούνται σε αύξουσα σειρά pitch
static int collect_window_pitches(int start_white, int pitches[20]){
    int count = 0;
    //Προσθήκη των 12 λευκών πλήκτρων
    for(int i = 0; i <12; i++){
        pitches[count++] = white_index_to_pitch(start_white+i);
    }
    //Προσθ΄ηκη των μάυρων πλήκτρων ανάμεσα στα 12 λευκά 
    for(int i =0; i<11; i++){
        int pitch;
        if(black_index_to_pitch(start_white + i, &pitch)){
            pitches[count++] = pitch;
        }
    }
    //Ταξινόμηση των pitches σε αύξυοσα σειρά
    for(int i =0; i<count; i++){
        for(int j = i+1; j<count; j++){
            if(pitches[j] < pitches[i]){
                int tmp = pitches[i];
                pitches[i] = pitches[j];
                pitches[j] = tmp;
            }
        }
    }
    return count;
}
//Έλεγχος εαν ένα pitch ανήκει στο τρέχων παράθυρο πλήκτρων
static bool pitch_to_window(int pitch, int start_white){
    int pitches[20];
    int count = collect_window_pitches(start_white, pitches);

    for(int i = 0; i<count; i++){
        if(pitches[i] == pitch){
            return true;
        }
    }
    return false;
}

//Αυτόματη μετακίνηση του παράθυρου των πλήκτρων όταν η επόμενη εμφανιζόμενη νότα
//δεν ανήκει στο visible range.
//Η μετακίνηση γίνεται σταδιακά ανλα 1 λευκό πλήκτρο κάθε φορά
static void update_auto_octave_change(State state){
    //νότες που εμφανίζονται στο επόμενο χρονικό παράθυρο
    List notes = state_displayed_notes(state, 5.0);

    int found_pitch = -1;
    //Εύρεση πρώτης note_on νότας που δεν χωράει στο τρέχων παράθυρο
    for(ListNode node = list_first(notes); node!=LIST_EOF; node= list_next(notes, node)){
        MidiEvent ev = list_node_value(notes, node);

        if(ev->type!=MIDI_NOTE){
            continue;
        }
        if(ev->velocity <=0){
            continue;
        }
        if(!pitch_to_window(ev->key, current_start_white)){
            found_pitch = ev->key;
            break;
        }
    }

    list_destroy(notes);
    //Αν δεν βρέθηκε τέτοια νότα, δεν αλλάζει τίποτα
    if(found_pitch <0){
        return;
    }
    
    
    int best_start_white = current_start_white;
    int best_cost = 1000000;
    bool found = false;

    //Αναζήτηση start_white που περιέχει το found_pitch
    //και βρίσκεται πιο κοντά στο τρέχων current_start_white.
    for(int start_white = 0; start_white<=23; start_white++){
        if(!pitch_to_window(found_pitch, start_white)){
            continue;
        }
        int cost = abs(start_white - current_start_white);
          
        if(!found || cost < best_cost){
            best_cost = cost;
            best_start_white = start_white;
            found = true;
        }
    }
    if(!found){
        return;
    }
    //Σταδιακή μετακίνηση προς την καλύτερη δυνατή θέση
    if(best_start_white>current_start_white){
        current_start_white++;
    }
    else if(best_start_white<current_start_white){
        current_start_white--;
    }

    current_start_white = clamp_start_white(current_start_white);
     
}
//Συνάρτηση που ενημερώνει την κατάσταση του πληκτρολογίου για το τρέχων frame
//Γίνεται mapping απο τα πλήκτρα του υπολογιστή στα MIDI patches
//του ενεργού παραθύρου(12 λευκά + 8 μαύρα)
static void update_keyboard_state(KeyState ks) {
    ks->space = IsKeyPressed(KEY_SPACE);
    ks->n = IsKeyPressed(KEY_N);

    //Καθαρισμός changed_keys για αυτο το frame
    for (int i = 0; i < 128; i++) {
        ks->changed_keys[i] = -1;
    }

    // Mapping πληκτρολογίου -> MIDI νότες
    int white_ray_keys[12] = {KEY_A, KEY_S, KEY_D, KEY_F,KEY_G, KEY_H, KEY_J, KEY_K,KEY_L, KEY_SEMICOLON, KEY_APOSTROPHE, KEY_BACKSLASH};
    int black_ray_keys[8] = {KEY_W, KEY_E, KEY_T, KEY_Y, KEY_U, KEY_O, KEY_P, KEY_RIGHT_BRACKET};


    //Ενημέρωση κατάστασης για τα λευκά πλήκτρα
    for(int i =0; i<12; i++){
        int midi = white_index_to_pitch(current_start_white + i);
        char old_value = ks->active_keys[midi];
        char new_value = 0;
        if(IsKeyDown(white_ray_keys[i])){
            new_value = 100;
        }
        ks->active_keys[midi] = new_value;
        if(old_value != new_value){
            ks->changed_keys[midi] = new_value;
        }

    }
    //Ενημέρωση κατάστασης για τα μαύρα πλήκτρα
    int black_idx = 0;
    for(int i =0; i<11 && black_idx < 8; i++){
        int pitch;
        if(!black_index_to_pitch(current_start_white + i, &pitch)){
            continue;
        }
        char old_value = ks->active_keys[pitch];
        char new_value = 0;
        if(IsKeyDown(black_ray_keys[black_idx])){
            new_value = 100;
        }
        ks->active_keys[pitch] = new_value;
        if(old_value != new_value){
            ks->changed_keys[pitch] = new_value;
        }
        black_idx++;
    }
}


//Βασικό loop για update and draw 
//Ενημέρωση αυτόματου shift, ανάγνωση input, ενημέρωση state
//παίξημο playback midi events και σχεδίαση frame
static void update_and_draw(void) {
    update_auto_octave_change(state);
    update_keyboard_state(ks);
    //Διατήρηση χρόνου προηγούμενου frame για να βρεθο΄υν τα events που πρέπει να παιχτούν
    double old_time = state_info(state)->time;
    state_update(state, ks, GetFrameTime());

    //Απαιτούμενα events για αναπαραγωγή ανάμεσα σε old και current time
    List events = state_playback_events(state, old_time);
    for(ListNode node = list_first(events); node!=LIST_EOF; node=list_next(events,node)){
        MidiEvent ev = list_node_value(events, node);
        if(ev->type==MIDI_NOTE){
            k08midi_synth_note(ev->channel, ev->key, ev->velocity);
        }
        else if(ev->type == MIDI_PROGRAM_CHANGE){
            k08midi_synth_set_program(ev->channel, ev->program, ev->channel==9);
        }
        else if(ev->type==MIDI_CONTROL_CHANGE){
            k08midi_synth_set_control(ev->channel, ev->control, ev->control_value);
        }
    }
    list_destroy(events);
    interface_draw_frame(state);
}

//Αρχικοποίηση προγράμματος και εκκίνηση του main loop
int main(void) {
    //Μηδενισμός δεδομένων του KeyState
    memset(&key_state_data, 0, sizeof(key_state_data));
    reset_keyboard_state(ks);

    //Αρχική θέση παραθύρου πλήκτρων
    current_start_white = 7;
    //Αρχικοποίηση interface/audio
    interface_init();
    state = state_create("assets/back-to-black.mid");
    
    //Εκτέλεση βασικού loop
    start_main_loop(update_and_draw);

    //Αποδέσμευση 
    interface_close();
    state_destroy(state);

    return 0;
}

