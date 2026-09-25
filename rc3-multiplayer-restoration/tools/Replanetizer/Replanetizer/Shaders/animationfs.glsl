#version 330 core

// Interpolated values from the vertex shaders
in vec2 UV;
in vec3 lightColor;
in float fogBlend;

// Ouput data
layout(location = 0) out vec4 color;
layout(location = 1) out int id;

// Values that stay constant for the whole mesh.
uniform sampler2D mainTexture;
uniform sampler2D blueNoiseTexture;
uniform int levelObjectNumber;
uniform vec4 fogColor;
uniform float objectBlendDistance;
uniform int useTransparency;
uniform int useTexture;
uniform int useMetalShading;
uniform int ssaaLevelLog;
uniform float mobyAlpha;

#define ONE_OVER_GOLDEN_RATIO (2654435769u) /* 0.61803398875f */

bool computeDitheringDiscard(float alpha)
{
    if (alpha <= 0.0f)
        return true;

    if (alpha >= 1.0f)
        return false;

    ivec2 internalPixel = ivec2(gl_FragCoord.xy);
    ivec2 noiseSize = textureSize(blueNoiseTexture, 0);
    ivec2 pixel = internalPixel.xy >> ssaaLevelLog;
    ivec2 noisePixel = pixel % noiseSize;
    float alphaThreshold = texelFetch(blueNoiseTexture, noisePixel, 0).x;

    uint offsetIndex = uint(1 + levelObjectNumber);

    float objectDitherOffset = float(offsetIndex * ONE_OVER_GOLDEN_RATIO);
    objectDitherOffset = objectDitherOffset / 0xFFFFFFFFu;

    ivec2 subpixel = internalPixel.xy & ((1 << ssaaLevelLog) - 1);

    uint subpixelID = uint(subpixel.x + (1 << ssaaLevelLog) * subpixel.y);
    float subpixelOffset = subpixelID * (1.0f / ((1 << ssaaLevelLog) * (1 << ssaaLevelLog)));

    alphaThreshold = fract(alphaThreshold + objectDitherOffset + subpixelOffset);

    return (alphaThreshold > alpha);
}

void main() {
	//color of the texture at the specified UV
    vec4 textureColor;
    if (useTexture != 0 || useMetalShading != 0) {
        textureColor = texture(mainTexture, UV);
    }
    else {
        textureColor = vec4(1.0f, 1.0f, 1.0f, 0.5f);
    }

    float alpha = 1.0f;

    // If the object is further than renderDistance we blend out over distance using a dissolve pattern
    if (objectBlendDistance > 0.0f) {
        alpha *= 1.0f - objectBlendDistance;
    }

    alpha *= mobyAlpha;

    if (useTransparency != 0) {
        alpha *= textureColor.w;
    }

    if (useMetalShading == 0 && computeDitheringDiscard(alpha)) discard;

	color.xyz = 1.5f * textureColor.xyz * lightColor * 2.0f;
	color.w = (useMetalShading != 0) ? alpha : 1.0f;

	color.xyz = (fogColor.xyz - color.xyz) * fogBlend + color.xyz;

	id = (4 << 24) | levelObjectNumber;
}
