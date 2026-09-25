#version 330 core

void main()
{
    const vec2 positions[3] = vec2[](
        vec2(-1.0f, -1.0f),
        vec2(3.0f, -1.0f),
        vec2(-1.0f, 3.0f)
    );

    gl_Position = vec4(positions[gl_VertexID], 0.0f, 1.0f);
}
