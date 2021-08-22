
#version 400 core

layout( location = 0 ) in vec3 v_position;
layout( location = 1 ) in vec4 v_color;
layout( location = 2 ) in vec3 v_normal_vertex;

out vec4 iColor;

void main()
{
    gl_Position = vec4(v_position.x, v_position.y, v_position.z, 1.0);
    iColor = v_color;
}
