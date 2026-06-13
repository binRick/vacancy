#version 330
// PSX-style world material (port of psx_vertex_snap.gdshader): vertices snap
// to a coarse screen grid (the classic jitter) and UVs interpolate affinely
// (the texture swim). Lighting is done per-fragment in world space against a
// small set of point lights bound per room (render.c).

in vec3 vertexPosition;
in vec2 vertexTexCoord;
in vec3 vertexNormal;
in vec4 vertexColor;

uniform mat4 mvp;
uniform mat4 matModel;
uniform mat4 matNormal;
// Screen-space grid the vertices snap to; (320, 240) = internal render pixels.
uniform vec2 snapResolution;

out vec2 correctUV;
// UV pre-multiplied by clip-space w: dividing it back by the interpolated w in
// the fragment cancels the GPU's perspective correction, leaving screen-linear
// (affine) interpolation, like PS1 hardware.
out vec2 affineUV;
out float affineW;
out vec3 worldPos;
out vec3 worldNormal;
out vec4 vColor;

void main()
{
    vec4 clip = mvp*vec4(vertexPosition, 1.0);
    if (clip.w > 0.0) {
        vec2 halfGrid = snapResolution*0.5;
        clip.xy = round(clip.xy/clip.w*halfGrid)/halfGrid*clip.w;
    }
    gl_Position = clip;

    correctUV = vertexTexCoord;
    affineUV = vertexTexCoord*clip.w;
    affineW = clip.w;
    worldPos = (matModel*vec4(vertexPosition, 1.0)).xyz;
    worldNormal = normalize((matNormal*vec4(vertexNormal, 0.0)).xyz);
    vColor = vertexColor;
}
