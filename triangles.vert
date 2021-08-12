
#version 400 core

layout( location = 0 ) in vec4 vPosition;
layout( location = 1 ) in vec4 fPosition;

out vec4 iColor;

void
main()
{
    gl_Position = vPosition;
    iColor = fPosition;
}
