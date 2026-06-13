#version 330
// Fragment half of the PSX world material: affine texture lookup + low-budget
// per-fragment Lambert lighting from the room's omni lights, plus a flat
// ambient term (the Godot WorldEnvironment ambient).

in vec2 correctUV;
in vec2 affineUV;
in float affineW;
in vec3 worldPos;
in vec3 worldNormal;
in vec4 vColor;

uniform sampler2D texture0;
uniform vec4 colDiffuse;

// 1.0 = full PSX affine swim, 0.0 = modern perspective-correct.
uniform float affine;
uniform vec3 ambientColor;

#define MAX_LIGHTS 8
uniform int lightCount;
uniform vec3 lightPos[MAX_LIGHTS];
uniform vec3 lightColor[MAX_LIGHTS];
uniform float lightEnergy[MAX_LIGHTS];
uniform float lightRange[MAX_LIGHTS];

out vec4 finalColor;

void main()
{
    vec2 uv = mix(correctUV, affineUV/max(affineW, 1e-5), affine);
    vec3 tex = texture(texture0, uv).rgb;
    vec3 albedo = colDiffuse.rgb*tex*vColor.rgb;

    vec3 N = normalize(worldNormal);
    vec3 lit = ambientColor;
    for (int i = 0; i < lightCount; i++) {
        vec3 d = lightPos[i] - worldPos;
        float dist = length(d);
        float atten = clamp(1.0 - dist/lightRange[i], 0.0, 1.0);
        atten *= atten;
        float ndl = max(dot(N, d/max(dist, 1e-4)), 0.0);
        lit += lightColor[i]*lightEnergy[i]*atten*ndl;
    }

    finalColor = vec4(albedo*lit, 1.0);
}
