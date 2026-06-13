shader_type spatial;
render_mode skip_vertex_transform, diffuse_lambert, specular_disabled;

// PSX-style world material (SPEC §7): vertices snap to a coarse screen grid
// (the classic jitter) and UVs interpolate affinely (texture swim).

uniform vec4 albedo_color : source_color = vec4(1.0);
uniform sampler2D albedo_texture : source_color, filter_nearest, repeat_enable, hint_default_white;
uniform vec2 uv_scale = vec2(1.0);
// Screen-space grid the vertices snap to; (320, 240) = internal render pixels.
uniform vec2 snap_resolution = vec2(320.0, 240.0);
// 1.0 = full PSX affine swim, 0.0 = modern perspective-correct.
uniform float affine : hint_range(0.0, 1.0) = 1.0;

// UV pre-multiplied by clip-space w: dividing it back by the interpolated w
// in the fragment shader cancels the GPU's perspective correction, leaving
// screen-linear (affine) interpolation, like PS1 hardware.
varying vec2 affine_uv;
varying float affine_w;
varying vec2 correct_uv;

void vertex() {
	VERTEX = (MODELVIEW_MATRIX * vec4(VERTEX, 1.0)).xyz;
	NORMAL = normalize((MODELVIEW_MATRIX * vec4(NORMAL, 0.0)).xyz);
	vec4 clip_pos = PROJECTION_MATRIX * vec4(VERTEX, 1.0);
	if (clip_pos.w > 0.0) {
		vec2 half_grid = snap_resolution * 0.5;
		clip_pos.xy = round(clip_pos.xy / clip_pos.w * half_grid) / half_grid * clip_pos.w;
	}
	POSITION = clip_pos;
	correct_uv = UV * uv_scale;
	affine_uv = correct_uv * clip_pos.w;
	affine_w = clip_pos.w;
}

void fragment() {
	vec2 uv = mix(correct_uv, affine_uv / max(affine_w, 1e-5), affine);
	ALBEDO = albedo_color.rgb * texture(albedo_texture, uv).rgb;
	ROUGHNESS = 1.0;
}
