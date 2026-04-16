#include "k08midi.h"

#include "raylib.h"

#include <stdio.h>
#include <assert.h>

// Αναπαραγωγή του event, χρησιμοποιώντας τις k08midi_synth_* συναρτήσεις.
static void dispatch_event(MidiEvent event) {
    switch (event->type) {
        case MIDI_NOTE:
            k08midi_synth_note(event->channel, event->key, event->velocity);
            printf("[MIDI_NOTE] t=%.3f ch=%u key=%u velocity=%u\n",
                   event->time, event->channel, event->key, event->velocity);
            break;

        case MIDI_CONTROL_CHANGE:
            k08midi_synth_set_control(event->channel, event->control, event->control_value);
            printf("[MIDI_CONTROL_CHANGE] t=%.3f ch=%u control=%u value=%u\n",
                   event->time, event->channel, event->control, event->control_value);
            break;

        case MIDI_PROGRAM_CHANGE:
            // Τα drums δεν ρυθμίζονται από το program number, όπως τα περισσότερα όργανα,
            // αλλά βρίσκονται από σύμβαση στο channel 10 (id 9).
            k08midi_synth_set_program(event->channel, event->program, event->channel == 9);
            printf("[MIDI_PROGRAM_CHANGE] t=%.3f ch=%u program=%u\n",
                   event->time, event->channel, event->program);
            break;

        case MIDI_MARKER:
            printf("[MIDI_MARKER] t=%.3f text=\"%s\"\n",
                   event->time, event->marker != NULL ? event->marker : "");
            break;

        default:
            break;
    }
}

int main(void) {
    InitWindow(800, 450, "Raylib + k08midi MIDI Player");
    SetTargetFPS(60);

	// Αρχικοποίηση του software synthesizer, χρησιμοποιώντας ένα soundfont
	// (.sf3) που περιέχει samples από όλα τα βασικά MIDI όργανα.
    bool synth_ok = k08midi_synth_init("./assets/GeneralUserGS.sf3");
    assert(synth_ok);

    // Διαβάζουμε το MIDI file.
    MidiFile midi_file = k08midi_file_read("./assets/back-to-black.mid");
    assert(midi_file != NULL);

    ListNode next_event = list_first(midi_file->events);
    double elapsed_seconds = 0.0;
    bool playing = true;

    while (!WindowShouldClose()) {
        if (IsKeyPressed(KEY_SPACE)) {
            playing = !playing;
        }

        if (playing) {
            // Προχωράμε τον χρόνο μπροστά
            elapsed_seconds += GetFrameTime();

            // Αναπαράγουμε events από το midi_file->events από το σημείο που είχαμε μείνει, μέχρι και το τωρινό
            while (next_event != LIST_EOF) {
                MidiEvent event = list_node_value(midi_file->events, next_event);
                if (event->time > elapsed_seconds) {
                    break;
                }
                dispatch_event(event);
                next_event = list_next(midi_file->events, next_event);
            }
        }

        //  Reset αν πατηθεί R
        if (IsKeyPressed(KEY_R) || next_event == LIST_EOF) {
            elapsed_seconds = 0.0;
            next_event = list_first(midi_file->events);
            k08midi_synth_note_off_all();
        }

        // Σχεδιάζουμε την οθόνη
        BeginDrawing();
        ClearBackground(RAYWHITE);
        DrawText("Playing MIDI File...", 50, 50, 20, DARKGRAY);
        DrawText(TextFormat("Time: %.1f s", elapsed_seconds), 50, 80, 20, BLUE);
        DrawText("Press SPACE to Pause/Resume", 50, 120, 18, DARKGREEN);
        DrawText("Press R to Restart", 50, 145, 18, MAROON);
        EndDrawing();
    }

    k08midi_synth_note_off_all();
    k08midi_file_destroy(midi_file);
    k08midi_synth_close();
    CloseWindow();

    return 0;
}
