#version 450

out vec4 FragColor;
in vec2 UV;

uniform sampler2D _ColorBuffer;

void main(){
	vec3 color = 1.0 - texture(_ColorBuffer, UV).rgb;
	color /= 4;
	color += (1.0 - texture(_ColorBuffer, vec2(UV.x - 1, UV.y)).rgb) / 8;
	color += (1.0 - texture(_ColorBuffer, vec2(UV.x - 1, UV.y - 1)).rgb) / 16;
	color += (1.0 - texture(_ColorBuffer, vec2(UV.x, UV.y - 1)).rgb) / 8;
	color += (1.0 - texture(_ColorBuffer, vec2(UV.x + 1, UV.y - 1)).rgb) / 16;
	color += (1.0 - texture(_ColorBuffer, vec2(UV.x + 1, UV.y)).rgb) / 8;
	color += (1.0 - texture(_ColorBuffer, vec2(UV.x + 1, UV.y + 1)).rgb) / 16;
	color += (1.0 - texture(_ColorBuffer, vec2(UV.x, UV.y + 1)).rgb) / 8;
	color += (1.0 - texture(_ColorBuffer, vec2(UV.x - 1, UV.y + 1)).rgb) / 16;
	FragColor = vec4(color, 1.0);
}