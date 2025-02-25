#version 450

layout(location = 0) in vec3 vPos;
layout(location = 1) in vec3 vNormal;
layout(location = 2) in vec2 vTexCoord;

uniform mat4 _Model;
uniform mat4 _ViewProjection;
uniform mat4 _LightSpaceMatrix;

out Surface{
	vec3 worldPos;
	vec3 worldNormal;
	vec2 texCoord;
	vec4 fragPosLightSpace;
}vs_out;

void main()
{
	vs_out.worldPos = vec3(_Model * vec4(vPos, 1.0));
	vs_out.worldNormal = transpose(inverse(mat3(_Model))) * vNormal;
	vs_out.texCoord = vTexCoord;
    vs_out.fragPosLightSpace = _LightSpaceMatrix * vec4(vs_out.worldPos, 1.0);
	gl_Position = _ViewProjection * _Model * vec4(vPos, 1.0);
}