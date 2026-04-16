#include "raylib.h"
#include <stdio.h>
#include "interface.h"
#include "state.h"
#include "k08midi.h"
#include <assert.h>

//Μεταβλητή που ορίζεται στο game.c και δείχνει 
//απο ποιό λευκό πλήκτρο ξεκινά το ενεργό παράθυρο του keyboard.
extern int current_start_white;
//Επιστροφή του τρέχοντος start_white περιορισμένο στο έγκυρο διάστημα [0, 23]
static int get_start_white(void){
    int start_white = current_start_white;
    if(start_white < 0){
        start_white = 0;
    }
    if(start_white>23){
        start_white = 23;
    }
    return start_white;
}
//Έλεγχος pitch class για αντιστοιχία σε μαύρο πλήκτρο
//Τα μαύρα πλήκτρα σε μία οκτάβα είναι: C#, D#, F#, G#, A#
static bool is_black_pitch_class(int pc){
    return pc==1||pc==3||pc==6||pc==8||pc==10;
}

//Σχεδιασμός falling note ως rounded rectangle
//Θεση x, το πάνω και κάτω y όριο, πλάτος και χρώμα
static void draw_note(float x, float y_top, float y_bot, float width, Color color){
    //ορθός σχεδιασμός 
    if(y_bot<y_top){
        float temp = y_top;
        y_top = y_bot;
        y_bot = temp;
    }

    Rectangle noteRec = {x-width/2.0, y_top, width, y_bot-y_top};

    DrawRectangleRounded(noteRec, 0.2, 6, color);
}

//Έλεγχος αν μετά από ένα συγκεκριμένο λευκό πλήκτρο υπάρχει μαύρο πλήκτρο.
// Στα positions 0,1,3,4,5 της 7άδας υπάρχει αντίστοιχο μαύρο πλήκτρο.
static bool black_index_exists(int white_index){
    int pos = white_index%7;
    return pos == 0|| pos ==1 || pos == 3 || pos ==4 || pos == 5;
}

// Ελεγχος αν κάποιο από τα 12 λευκά πλήκτρα του ενεργού παραθύρου
// είναι πατημένο στο πληκτρολόγιο του υπολογιστή.
static bool white_key_pressed(int idx) {
    switch (idx) {
        case 0:  return IsKeyDown(KEY_A);
        case 1:  return IsKeyDown(KEY_S);
        case 2:  return IsKeyDown(KEY_D);
        case 3:  return IsKeyDown(KEY_F);
        case 4:  return IsKeyDown(KEY_G);
        case 5:  return IsKeyDown(KEY_H);
        case 6:  return IsKeyDown(KEY_J);
        case 7:  return IsKeyDown(KEY_K);
        case 8:  return IsKeyDown(KEY_L);
        case 9:  return IsKeyDown(KEY_SEMICOLON);
        case 10: return IsKeyDown(KEY_APOSTROPHE);
        case 11: return IsKeyDown(KEY_BACKSLASH);
    }
    return false;
}

// Ελεγχος αν κάποιο από τα 8 μαύρα πλήκτρα του ενεργού παραθύρου
// είναι πατημένο στο πληκτρολόγιο του υπολογιστή.
static bool black_key_pressed(int idx){
    switch(idx){
        case 0: return IsKeyDown(KEY_W);
        case 1: return IsKeyDown(KEY_E);
        case 2: return IsKeyDown(KEY_T);
        case 3: return IsKeyDown(KEY_Y);
        case 4: return IsKeyDown(KEY_U);
        case 5: return IsKeyDown(KEY_O);
        case 6: return IsKeyDown(KEY_P);
        case 7: return IsKeyDown(KEY_RIGHT_BRACKET);
    }
    return false;
}

// Μετατροπή ενός MIDI pitch σε x συντεταγμένη πάνω στο πιάνο.
// Το πιάνο ξεκινά από MIDI pitch 24 και καλύπτει 5 οκτάβες (60 ημιτόνια).
static float pitch_to_x(int pitch, float pianoX, float whiteWidth) {
    int basePitch = 24;
    int rel = pitch-basePitch;
    // Αν το pitch είναι εκτός του εύρους του σχεδιαζόμενου πιάνου,
    // επιστρέφει αρνητική τιμή ώστε να αγνοηθεί.
    if(rel<0 || rel > 59) {
        return -1.0;
    }
    int pc = rel%12;   
    int octave = rel/12;
    // Για κάθε pitch class δίνεται η σχετική θέση του μέσα στην οκτάβα,
    // με μονάδα μέτρησης το πλάτος λευκού πλήκτρου.
    static const float x_in_octave[12] = {0.0, 0.5, 1.0, 1.5, 2.0, 3.0 ,3.5, 4.0, 4.5, 5.0, 5.5, 6.0};
    return pianoX + (octave *7 + x_in_octave[pc] + 0.5)*whiteWidth;
}

// Αρχικοποίηση interface:
// - δημιουργία παραθύρου
// - ορισμός FPS
// - αρχικοποίηση audio device
// - αρχικοποίηση software synthesizer με soundfont
void interface_init(){
    InitWindow(SCREEN_WIDTH, SCREEN_HEIGHT, "Game");
    SetTargetFPS(60);
    InitAudioDevice(); 
    bool synth_ok = k08midi_synth_init("assets/GeneralUserGS.sf3");
    assert(synth_ok);
}

// Καθαρισμός interface και synth όταν κλείνει το πρόγραμμα
void interface_close(void){
    k08midi_synth_note_off_all();
    k08midi_synth_close();
    CloseAudioDevice();
	CloseWindow();
}

//Σχεδιασμός ολόκληρου του frame του παιχνιδιού
void interface_draw_frame(State state){
    BeginDrawing();
    ClearBackground((Color){250, 244, 212, 255});

    //Διαστάσεις πλήκτρων
    double whiteHeight = 220.0;
    double blackHeight = 125.0;
    double whiteWidth = 32.0;
    double blackWidth = 22.0;
    
    //Θέση πιάνου
    float pianoWidth = 35*whiteWidth;
    float pianoX = (SCREEN_WIDTH - pianoWidth) / 2.0;
    float pianoY = SCREEN_HEIGHT - whiteHeight-20.0;
    

    //Labels πλήκτρων
    const char *whiteKeys[12] = {"A", "S", "D", "F", "G", "H", "J", "K", "L", ";", "'", "\\"};
    const char *blackKeys[8] = {"W", "E", "T", "Y", "U", "O", "P", "]"};

    //Τρέχων σημείο που ξεκινά το visible παράθυρο του keyboard
    int start_white = get_start_white();
    //Σημείο λιγο πιο πάνω απο το πιάνο
    float targetY = pianoY;
       
    //Πληροφορίες state
    StateInfo info = state_info(state);
    double now = info->time;
    double window = 5.0;

    //Σχεδίαση πληροφοριων παιχνιδιού
    DrawText(TextFormat("time: %.2f", now), 40, 80, 30, (Color){102, 0, 5, 255});
    DrawText(TextFormat("level: %d", info->level), 40, 400, 30, (Color){102, 0, 5, 255});
    DrawText(TextFormat("score: %d", (int)(info->score*100)), 40, 320, 30, (Color){102, 0, 5, 255});
    
    //Σχεδίαση των falling notes

    //notes που πρεπει να εμφανίζονται στην οθόνη 
    List notes = state_displayed_notes(state, window);
    for (ListNode node = list_first(notes); node != LIST_EOF; node = list_next(notes, node)) {
        MidiEvent ev = list_node_value(notes, node);

        //Έλεγχος μόνο note_on events
        if (ev->type != MIDI_NOTE) continue;
        if (ev->velocity <= 0) continue;

        //Υπολογισμός θέσης x της νότας πάνω στο πιάνο
        float x = pitch_to_x(ev->key, pianoX, whiteWidth);

        if(x<0.0){
            continue;
        }

        //Εύρεση αντίστοιχου note off για εύρεση διάρκειας
        MidiEvent note_off = NULL;
        for(ListNode node2 = list_next(notes, node); node2!=LIST_EOF; node2=list_next(notes,node2)){
            MidiEvent ev2 = list_node_value(notes, node2);

            if(ev2->type != MIDI_NOTE) {
                continue;
            }
            if(ev2->key != ev->key) {
                continue;
            }
            if(ev2->channel != ev->channel) {
                continue;
            }

            if(ev2->velocity > 0) {
                continue;
            }

            note_off = ev2;
            break;
        }
        //Αν βρεθεί note off χρησιμοποιείται ο χρόνος του αλλιώς δίνεται μικρή default διαρκεια
        double start_dt = ev->time - now;
        double end_time= 0.0;
        if(note_off!=NULL){
            end_time = note_off->time;
        }
        else{
            end_time=ev->time + 0.3;
        }
        double end_dt = end_time - now;

        //Μετατροπή χρόνου σε y συντεταγμένες
        float y_bot = targetY - (start_dt/window)*700.0;
        float y_top = targetY - (end_dt/window)*700.0;

        //Υπολογισμός πάνω/κάτω άκρου για clipping
        float top = y_bot; 
        if(y_top < y_bot){
            top = y_top;
        }
        float bot = y_top;
        if(y_bot > y_top){
            bot = y_bot;
        }
        //Αν η νότα είναι εκτός οθόνης αγνοείται
        if(bot < 0 || top > SCREEN_HEIGHT){
            continue;
        }
        
        //Τα μαύρα πλήκτρα έχουν στενότερες νότες απο τα λευκά
        int pc = ev->key%12;
        float noteWidth = 0.0;
        if(is_black_pitch_class(pc)){
            noteWidth = 16.0;
        }
        else{
            noteWidth = 24.0;
        }

        draw_note(x, y_top, y_bot, noteWidth, (Color){116, 7, 14, 255});
    }   
    list_destroy(notes);

    //Λευκά πλήκτρα

    for(int i = 0; i < 35; i++){
        Rectangle whites = {pianoX + whiteWidth*i, pianoY, whiteWidth, whiteHeight};
        
        //θέση λευκού πλήκτρου μέσα στο ενεργό παράθυρο 12 πλήκτρων
        int idx = i - start_white;
        
        Color whiteFill = (Color){223, 143, 156, 255};
        //Αν το πλήκτρο ανήκει στο ενεργό παράθυρο και είναι πατημένο, αλλάζει χρώμα
        if(idx >= 0 && idx < 12 && white_key_pressed(idx)){
            whiteFill = (Color){190, 110, 123, 255};
        }
        DrawRectangleRounded(whites, 0.05, 6, whiteFill);
        DrawRectangleRoundedLinesEx(whites, 0.05, 6, 2.0f, (Color){102, 0, 5, 255});   
    }
    //Labels στα 12 playable πλήκτρα
    for(int idx = 0; idx<12; idx++){
        int i = start_white + idx;
        if(i<0 || i>=35){
            continue;
        }
        Rectangle whites = {pianoX + whiteWidth * i, pianoY, whiteWidth, whiteHeight};
        int textWidth = MeasureText(whiteKeys[idx], 12);
        DrawText(whiteKeys[idx], (int)(whites.x + whites.width/2 - textWidth/2), (int)(whites.y + whites.height - 18), 12, (Color){102, 0, 5, 255});
    }

    //Μαύρα πλήκτρα
    for(int i = 0; i < 35; i++){
        if(!black_index_exists(i)){
            continue;
        }
        Rectangle blacks = {pianoX + (i+1)*whiteWidth-blackWidth/2.0, pianoY, blackWidth, blackHeight};
            
        int black_idx = -1;
        int count = 0;
        Color blackFill = (Color){102, 0, 5, 255};
        //Υπολογισμός του index του μαύρου πλήκτρου μέσα στο ενεργό παράθυρο
        for(int j = start_white; j<start_white +11; j++){
            if(j>=35){
                break;
            }
            if(!black_index_exists(j)){
                continue;
            }
            if(j == i){
                black_idx = count;
                break;
            }
            count++;
        }
        //Αν το μαύρο πλήκτρο είναι στο ενεργό παράθυρο και είναι πατημένο, αλλάζει χρώμα
        if(black_idx >= 0 && black_idx < 8 && black_key_pressed(black_idx)){
            blackFill = (Color){70, 0, 3, 255};
        }
        DrawRectangleRounded(blacks, 0.05, 6, blackFill);
        
    }

    //Labels στα 8 playable πληκτρα
    int black_label_idx = 0;
    for(int i = start_white; i<start_white + 11 && i<35; i++){
        if(!black_index_exists(i)){
            continue;
        }
        if(black_label_idx >=8){
            break;
        }
        Rectangle blacks = {pianoX + (i+1) * whiteWidth - blackWidth/2.0, pianoY, blackWidth, blackHeight};
        int textWidth = MeasureText(blackKeys[black_label_idx], 12);
        DrawText(blackKeys[black_label_idx], (int)(blacks.x + blacks.width/2 - textWidth/2), (int)(blacks.y + blacks.height - 18), 12, (Color){223, 143, 156, 255});
        black_label_idx++;
    }
    EndDrawing();
}
