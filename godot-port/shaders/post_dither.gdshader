shader_type canvas_item;
// Ordered (Bayer 4x4) dithering + bit-depth crunch (SPEC §7).
// Runs at the internal 320x240 so the dither pattern is screen-pixel sized.

uniform int color_levels : hint_range(2, 64) = 32; // 32 = PSX 15-bit color
uniform float dither_strength : hint_range(0.0, 2.0) = 1.0;

const mat4 BAYER = mat4(
		vec4(0.0, 8.0, 2.0, 10.0),
		vec4(12.0, 4.0, 14.0, 6.0),
		vec4(3.0, 11.0, 1.0, 9.0),
		vec4(15.0, 7.0, 13.0, 5.0)) / 16.0;

void fragment() {
	vec3 col = texture(TEXTURE, UV).rgb;
	ivec2 px = ivec2(mod(UV / TEXTURE_PIXEL_SIZE, 4.0));
	float threshold = BAYER[px.x][px.y] - 0.5;
	float levels = float(color_levels - 1);
	col += threshold * dither_strength / levels;
	col = round(col * levels) / levels;
	COLOR = vec4(col, 1.0);
}
