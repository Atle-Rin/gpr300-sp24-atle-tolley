#version 450

out vec4 FragColor;
in Surface{
	vec3 worldPos;
	vec3 worldNormal;
	vec2 texCoord;
	vec4 fragPosLightSpace;
}fs_in;
uniform sampler2D _MainTex;
uniform sampler2D _ShadowMap;
uniform vec3 _EyePos;
uniform vec3 _LightDirection = vec3(0.0, -1.0, 0.0);
uniform vec3 _LightColor = vec3(1.0);
uniform vec3 _AmbientColor = vec3(0.3,0.4,0.46);
uniform vec3 _ShadowMapDirection;

uniform float _MinBias;
uniform float _MaxBias;
uniform int _PCFSamplesSqrRt;

struct Material{
	float Ka;
	float Kd;
	float Ks;
	float Shiny;
};
uniform Material _Material;

float ShadowCalculation(vec4 fragPosLightSpace) {
    vec3 projCoords = fragPosLightSpace.xyz / fragPosLightSpace.w;
	projCoords = projCoords * 0.5 + 0.5;
	float currentDepth = projCoords.z;
	float bias = max(_MaxBias * (1.0 - dot(fs_in.worldNormal, _ShadowMapDirection)), _MinBias);
	float shadow = 0.0;
	vec2 texelSize = 1.0 / textureSize(_ShadowMap, 0);
	if (_PCFSamplesSqrRt != 0) {
		for (int x = -_PCFSamplesSqrRt; x <= _PCFSamplesSqrRt; ++x)
		{
		    for (int y = -_PCFSamplesSqrRt; y <= _PCFSamplesSqrRt; ++y)
		    {
				float pcfDepth = texture(_ShadowMap, projCoords.xy + vec2(x, y) * texelSize).r; 
				shadow += currentDepth - bias > pcfDepth ? 1.0 : 0.0;        
			}    
		}
		int pcfSamples = 1 + _PCFSamplesSqrRt * 2;
		pcfSamples *= pcfSamples;
		shadow /= pcfSamples;
	}
	else {
		float closestDepth = texture(_ShadowMap, projCoords.xy).r;
		shadow = currentDepth - bias > closestDepth ? 1.0 : 0.0;
	}
	return shadow;
}

void main()
{
	vec3 normal = normalize(fs_in.worldNormal);
	vec3 toLight = -_LightDirection;
	float diffFactor = max(dot(normal, toLight),0.0);
	vec3 toEye = normalize(_EyePos - fs_in.worldPos);
	vec3 h = normalize(toLight + toEye);
	float specFactor = pow(max(dot(normal, h), 0.0), _Material.Shiny);
	vec3 lightColor = (_Material.Kd * diffFactor + _Material.Ks * specFactor) * _LightColor;
	float shadow = ShadowCalculation(fs_in.fragPosLightSpace);
	lightColor *= (1.0 - shadow);
	lightColor += _AmbientColor * _Material.Ka;
	vec3 objColor = texture(_MainTex, fs_in.texCoord).rgb;
	FragColor = vec4(objColor * lightColor, 1.0);
}