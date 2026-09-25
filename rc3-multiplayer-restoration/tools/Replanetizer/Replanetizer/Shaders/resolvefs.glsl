#version 330 core

uniform sampler2D resolveTexture;
uniform int resolveSsaaLevel;

layout(location = 0) out vec4 color;

void main()
{
    ivec2 outputPixel = ivec2(gl_FragCoord.xy);
    ivec2 sourceBase = outputPixel * resolveSsaaLevel;
    vec3 colorSum = vec3(0.0f);

    for (int y = 0; y < resolveSsaaLevel; y++)
    {
        for (int x = 0; x < resolveSsaaLevel; x++)
        {
            colorSum += texelFetch(resolveTexture, sourceBase + ivec2(x, y), 0).rgb;
        }
    }

    float sampleCount = float(resolveSsaaLevel * resolveSsaaLevel);
    color = vec4(colorSum / sampleCount, 1.0f);
}
