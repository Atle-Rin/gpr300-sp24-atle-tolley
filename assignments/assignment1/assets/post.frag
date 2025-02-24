#version 450

out vec4 FragColor;
in vec2 UV;

uniform sampler2D _ColorBuffer;
uniform vec3 _offsetX;
uniform vec3 _offsetY;

uniform float _grayscaleR;
uniform float _grayscaleG;
uniform float _grayscaleB;
uniform float _customGamma;
uniform int _customSharpen;

void main(){
	vec2 offsets[9] = vec2[](
        vec2(_offsetX.x, _offsetY.x), // top-left
        vec2(_offsetX.y, _offsetY.x), // top-center
        vec2(_offsetX.z, _offsetY.x), // top-right
        vec2(_offsetX.x, _offsetY.y), // center-left
        vec2(_offsetX.y, _offsetY.y), // center-center
        vec2(_offsetX.z, _offsetY.y), // center-right
        vec2(_offsetX.x, _offsetY.z), // bottom-left
        vec2(_offsetX.y, _offsetY.z), // bottom-center
        vec2(_offsetX.z, _offsetY.z)  // bottom-right    
    );
	float kernel[9] = float[](
        -_customSharpen, -_customSharpen, -_customSharpen,
        -_customSharpen,  (1 - (-8 * _customSharpen)), -_customSharpen,
        -_customSharpen, -_customSharpen, -_customSharpen
    );
    vec3 cols[9];
	for (int i = 0; i < 9; i++)
	{
		cols[i] = texture(_ColorBuffer, UV + offsets[i]).rgb;
        float average = _grayscaleR * cols[i].x + _grayscaleG * cols[i].y + _grayscaleB * cols[i].z;
        cols[i] = vec3(average, average, average);
        cols[i] = pow(cols[i], vec3(_customGamma/2.2));
	}
	vec3 color = vec3(0.0);
    for (int i = 0; i < 9; i++)
    {
		color += cols[i] * kernel[i];
    }
	FragColor = vec4(color, 1.0);
}