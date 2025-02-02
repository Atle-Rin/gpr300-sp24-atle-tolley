#version 450

out vec4 FragColor;
in Surface{
	vec3 worldPos;
	vec3 worldNormal;
	vec2 texCoord;
}fs_in;
uniform sampler2D _MainTex;
uniform vec3 _EyePos;
uniform vec3 _LightDirection = vec3(0.0, -1.0, 0.0);
uniform vec3 _LightColor = vec3(1.0);

void main()
{
	vec3 normal = normalize(fs_in.worldNormal);
	vec3 toLight = -_LightDirection;
	float diffFactor = max(dot(normal, toLight),0.0);
	vec3 toEye = normalize(_EyePos - fs_in.worldPos);
	vec3 h = normalize(toLight + toEye);
	float specFactor = pow(max(dot(normal, h),0.0),128);
	vec3 lightColor = (diffFactor + specFactor) * _LightColor;
	vec3 objColor = texture(_MainTex, fs_in.texCoord).rgb;
	FragColor = vec4(objColor * lightColor, 1.0);
}