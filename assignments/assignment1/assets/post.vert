#version 450

out vec2 UV;

void main(){
	float u = (((uint(gl_VertexID) + 2) / 3) % 2);
	float v = (((uint(gl_VertexID) + 1) / 3) % 2);
	UV = vec2(u,v);
	gl_Position = vec4(-1.0 + u * 2.0, -1.0 + v * 2.0, 0.0, 1.0);
}