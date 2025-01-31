#version 450

out vec4 FragColor;
in vec3 Normal;

void main()
{
	FragColor = vec4(Normal * 0.5 + 0.5, 1.0);
}