#version 330
// Ordered (Bayer 4x4) dithering + bit-depth crunch (port of post_dither.gdshader).
// Runs at the internal 320x240 so the dither pattern is screen-pixel sized.

in vec2 fragTexCoord;
in vec4 fragColor;

uniform sampler2D texture0;
uniform vec4 colDiffuse;
uniform vec2 resolution;     // 320x240 internal render size

out vec4 finalColor;

// Column-major flatten of the Godot mat4 BAYER (so bayer[x*4+y] == BAYER[x][y]).
const float bayer[16] = float[16](
        0.0, 8.0, 2.0, 10.0,
        12.0, 4.0, 14.0, 6.0,
        3.0, 11.0, 1.0, 9.0,
        15.0, 7.0, 13.0, 5.0);
const int color_levels = 32;     // 32 = PSX 15-bit color
const float dither_strength = 1.0;

void main()
{
    vec3 col = texture(texture0, fragTexCoord).rgb;
    ivec2 px = ivec2(mod(fragTexCoord*resolution, 4.0));
    float threshold = bayer[px.x*4 + px.y]/16.0 - 0.5;
    float levels = float(color_levels - 1);
    col += threshold*dither_strength/levels;
    col = floor(col*levels + 0.5)/levels;
    finalColor = vec4(col, 1.0);
}
