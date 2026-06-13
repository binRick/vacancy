#version 330
// VHS/CRT post (port of post_vhs_crt.gdshader): barrel distortion, tracking
// jitter, occasional vertical roll, chromatic aberration, scanlines, noise,
// desaturation, vignette. `intensity` is driven upward by descent depth.

in vec2 fragTexCoord;
in vec4 fragColor;

uniform sampler2D texture0;
uniform vec4 colDiffuse;
uniform float intensity;          // rises with descent depth
uniform float scanlineStrength;   // 0.3
uniform float barrel;             // 0.05
uniform float time;
uniform vec2 resolution;          // 320x240 (drives scanline count + noise)

out vec4 finalColor;

#define TAU 6.28318530718

float hash(vec2 p) {
    return fract(sin(dot(p, vec2(127.1, 311.7)))*43758.5453);
}

void main()
{
    vec2 uv = fragTexCoord;
    float t = time;

    // Barrel distortion, zero net zoom at mid-radius; corners curve outward.
    vec2 c = uv - 0.5;
    float r2 = dot(c, c);
    uv = 0.5 + c*(1.0 + barrel*(r2 - 0.25)*4.0);

    // Occasional vertical roll: the picture briefly loses tracking.
    float win = floor(t*0.4);
    float ph = fract(t*0.4);
    if (hash(vec2(win, 3.37)) > 0.96) {
        uv.y = fract(uv.y - exp(-ph*6.0)*0.2*(0.3 + intensity));
    }

    // Sparse horizontal tracking jitter on coarse bands.
    float frame = floor(t*24.0);
    float band = floor(uv.y*40.0);
    if (hash(vec2(frame, band)) > 1.0 - 0.2*intensity) {
        uv.x += (hash(vec2(band, frame)) - 0.5)*0.008;
    }

    // Chromatic aberration, worse toward the edges and with depth.
    float ab = (0.0006 + 0.0035*intensity)*(0.5 + r2*2.0);
    vec3 col = vec3(
            texture(texture0, uv + vec2(ab, 0.0)).r,
            texture(texture0, uv).g,
            texture(texture0, uv - vec2(ab, 0.0)).b);

    // Outside the curved tube there is nothing.
    if (uv.x < 0.0 || uv.x > 1.0 || uv.y < 0.0 || uv.y > 1.0) col = vec3(0.0);

    // Scanlines matched to the internal line count.
    float lines = resolution.y;
    float scan = 0.5 + 0.5*cos(uv.y*lines*TAU);
    col *= 1.0 - scanlineStrength*(0.4 + 0.6*intensity)*scan;

    // Tape noise.
    col += (hash(uv*resolution + vec2(fract(t)*61.7, 0.0)) - 0.5)*(0.03 + 0.1*intensity);

    // The deeper you go, the less color survives.
    float grey = dot(col, vec3(0.299, 0.587, 0.114));
    col = mix(col, vec3(grey), 0.25*intensity);

    // Vignette.
    col *= 1.0 - r2*(0.5 + 0.9*intensity);

    finalColor = vec4(col, 1.0);
}
