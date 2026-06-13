// GameState — descent progress, room memory, event flags (game_state.gd).
#include "game.h"
#include <string.h>
#include <stdio.h>

GameState gs;
Player player;
UIState ui;
Floor *F;
bool g_world_paused;

void gs_descend(void)
{
    gs.descentDepth += 1;
    char buf[48];
    snprintf(buf, sizeof buf, "\"depth\":%d", gs.descentDepth);
    telemetry_event("descend", buf);
    // depth_changed signal: drive the audio bed and the VHS intensity.
    audio_on_depth_changed(gs.descentDepth);
    render_set_vhs_intensity(Clamp(0.18f + 0.08f*gs.descentDepth, 0.0f, 1.0f));
}

void gs_set_flag(const char *flag, bool value)
{
    for (int i = 0; i < gs.flagCount; i++) {
        if (strcmp(gs.flags[i].name, flag) == 0) { gs.flags[i].val = value; return; }
    }
    if (gs.flagCount < 32) {
        snprintf(gs.flags[gs.flagCount].name, 32, "%s", flag);
        gs.flags[gs.flagCount].val = value;
        gs.flagCount++;
    }
    char buf[80];
    snprintf(buf, sizeof buf, "\"flag\":\"%s\",\"value\":%s", flag, value ? "true" : "false");
    telemetry_event("flag", buf);
}

bool gs_get_flag(const char *flag)
{
    for (int i = 0; i < gs.flagCount; i++)
        if (strcmp(gs.flags[i].name, flag) == 0) return gs.flags[i].val;
    return false;
}

void gs_show_caption(const char *text)
{
    snprintf(ui.caption, sizeof ui.caption, "%s", text);
    ui.captionTimer = 2.4f;
    ui.captionSeq++;
}

void gs_open_note(const char *text, bool is_final)
{
    gs.lastNoteFinal = is_final;
    snprintf(ui.noteText, sizeof ui.noteText, "%s", text);
    ui.noteOpen = true;
    g_world_paused = true;
}

void gs_mark_room_seen(const char *room)
{
    int times = 0;
    for (int i = 0; i < gs.seenCount; i++) {
        if (strcmp(gs.seen[i].name, room) == 0) { times = ++gs.seen[i].count; break; }
    }
    if (times == 0 && gs.seenCount < 32) {
        snprintf(gs.seen[gs.seenCount].name, 32, "%s", room);
        gs.seen[gs.seenCount].count = 1;
        times = 1;
        gs.seenCount++;
    }
    char buf[80];
    snprintf(buf, sizeof buf, "\"room\":\"%s\",\"times\":%d", room, times);
    telemetry_event("room_seen", buf);
}

int gs_room_visits(const char *room)
{
    for (int i = 0; i < gs.seenCount; i++)
        if (strcmp(gs.seen[i].name, room) == 0) return gs.seen[i].count;
    return 0;
}

void gs_emit_anomaly(const char *id)
{
    audio_on_anomaly(id);
}
