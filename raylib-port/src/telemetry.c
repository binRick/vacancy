// Telemetry — JSON-lines gamestate log (telemetry.gd). Enabled with
// --telemetry-log=<path> or VACANCY_TELEMETRY_LOG. One "tick" heartbeat per
// second with a full snapshot, plus immediate discrete events. Zero cost when
// disabled.
#include "game.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

static FILE *s_file;
static float s_accum;

void telemetry_init(const char *path)
{
    if (!path || !path[0]) return;
    s_file = fopen(path, "w");
    if (!s_file) {
        TraceLog(LOG_WARNING, "Telemetry: cannot open '%s'", path);
        return;
    }
    telemetry_event("start", "\"engine\":\"raylib 6.0\"");
}

bool telemetry_enabled(void) { return s_file != NULL; }

void telemetry_event(const char *name, const char *json_fields)
{
    if (!s_file) return;
    double t = GetTime();
    if (json_fields && json_fields[0])
        fprintf(s_file, "{\"t\":%.2f,\"ev\":\"%s\",%s}\n", t, name, json_fields);
    else
        fprintf(s_file, "{\"t\":%.2f,\"ev\":\"%s\"}\n", t, name);
    fflush(s_file);
}

void telemetry_update(float dt, bool paused, const char *scene)
{
    if (!s_file) return;
    s_accum += dt;
    if (s_accum < 1.0f) return;
    s_accum = fmodf(s_accum, 1.0f);

    Vector3 p = player.pos;
    float yaw_deg = -player.yaw*RAD2DEG;
    float speed = Vector2Length((Vector2){player.velocity.x, player.velocity.z});
    char buf[640];
    snprintf(buf, sizeof buf,
        "\"fps\":%d,\"paused\":%s,\"scene\":\"%s\",\"depth\":%d,\"floor\":\"%s\","
        "\"rooms_seen\":%d,\"master_db\":%.1f,"
        "\"player\":{\"pos\":[%.2f,%.2f,%.2f],\"yaw_deg\":%.1f,\"crouch\":%s,"
        "\"speed\":%.2f,\"target\":\"%s\"}",
        GetFPS(), paused ? "true" : "false", scene, gs.descentDepth,
        gs.currentFloorName, gs.seenCount, audio_master_volume(),
        p.x, p.y, p.z, yaw_deg, player.crouching ? "true" : "false",
        speed, player.targetPrompt);
    telemetry_event("tick", buf);
}

void telemetry_shutdown(void)
{
    if (!s_file) return;
    telemetry_event("end", NULL);
    fclose(s_file);
    s_file = NULL;
}
