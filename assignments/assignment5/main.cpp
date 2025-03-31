#include <stdio.h>
#include <math.h>

#include <ew/external/glad.h>
#include <ew/shader.h>
#include <ew/model.h>
#include <ew/camera.h>
#include <ew/transform.h>
#include <ew/cameraController.h>
#include <ew/texture.h>
#include <ew/procGen.h>

#include <GLFW/glfw3.h>
#include <imgui.h>
#include <imgui_impl_glfw.h>
#include <imgui_impl_opengl3.h>

void framebufferSizeCallback(GLFWwindow* window, int width, int height);
GLFWwindow* initWindow(const char* title, int width, int height);
void drawUI();

//Global state
int screenWidth = 1080;
int screenHeight = 720;
float prevFrameTime;
float deltaTime;

ew::Camera cam;
ew::CameraController camCon;

ew::Camera light;
glm::vec3 lightDir;
unsigned int depthMap;

float timeExposure = 0.0f;

struct Material {
	float Ka = 1.0;
	float Kd = 0.5;
	float Ks = 0.5;
	float Shiny = 128;
};

Material material;

bool shadowToggle = true;
float minBias = 0.005f;
float maxBias = 0.05f;
int pcfSampleInt = 2;

struct Point {
	float pos[3];
	float rot[3];
	float sca[3] = { 1.0f, 1.0f, 1.0f };
};

struct Spline {
	Point controls[4];
	float startSize = 1.0f;
	float endSize = 1.0f;
};

std::vector<Spline*> splines;

glm::vec3 VecFy(float right[]) {
	glm::vec3 ret;
	for (int i = 0; i < 3; i++) {
		ret[i] = right[i];
	}
	return ret;
}

glm::quat EulToQuat(float eul[]) {
	double cr = cos(eul[0] * 0.5);
	double sr = sin(eul[0] * 0.5);
	double cp = cos(eul[1] * 0.5);
	double sp = sin(eul[1] * 0.5);
	double cy = cos(eul[2] * 0.5);
	double sy = sin(eul[2] * 0.5);

	glm::quat q;
	q.w = cr * cp * cy + sr * sp * sy;
	q.x = sr * cp * cy - cr * sp * sy;
	q.y = cr * sp * cy + sr * cp * sy;
	q.z = cr * cp * sy - sr * sp * cy;

	return q;
}

float Lerp(float A, float B, float time) {
	return A + (B - A) * time;
}

glm::quat SLerp(glm::quat A, glm::quat B, float time) {

	glm::quat An = glm::normalize(A);
	glm::quat Bn = glm::normalize(B);

	float dot = glm::dot(An, Bn);

	if (dot > 1.0f) dot = 1.0f;
	if (dot < 1.0f) dot = 1.0f;

	float angle = glm::acos(dot);
	float sinAng = glm::sin(angle);

	bool lerp = false;
	if (sinAng < 0.0001f) {
		lerp = true;
	}

	if (lerp) {
		glm::quat ret = An + (Bn - An) * time;
		return glm::normalize(ret);
	}
	else {
		float tI = glm::sin((1.0f - time) * angle) / sinAng;
		float tN = glm::sin(time * angle) / sinAng;
		glm::quat ret = tI * An + tN * Bn;
		return glm::normalize(ret);
	}

}

glm::vec3 VecLerp(glm::vec3 left, glm::vec3 right, float time) {
	glm::vec3 ret;
	for (int i = 0; i < 3; i++) {
		ret[i] = Lerp(left[i], right[i], time);
	}
	return ret;
}

glm::vec3 SplinePosLerp(Spline input, float time) {
	glm::vec3 ret;
	glm::vec3 a;
	glm::vec3 b;
	glm::vec3 c;
	glm::vec3 d;
	glm::vec3 e;
	a = VecLerp(VecFy(input.controls[0].pos), VecFy(input.controls[1].pos), time);
	b = VecLerp(VecFy(input.controls[1].pos), VecFy(input.controls[2].pos), time);
	c = VecLerp(VecFy(input.controls[2].pos), VecFy(input.controls[3].pos), time);
	d = VecLerp(a, b, time);
	e = VecLerp(b, c, time);
	ret = VecLerp(d, e, time);
	return ret;
}

glm::vec3 SplineScaLerp(Spline input, float time) {
	glm::vec3 ret;
	glm::vec3 a;
	glm::vec3 b;
	glm::vec3 c;
	glm::vec3 d;
	glm::vec3 e;
	a = VecLerp(VecFy(input.controls[0].sca), VecFy(input.controls[1].sca), time);
	b = VecLerp(VecFy(input.controls[1].sca), VecFy(input.controls[2].sca), time);
	c = VecLerp(VecFy(input.controls[2].sca), VecFy(input.controls[3].sca), time);
	d = VecLerp(a, b, time);
	e = VecLerp(b, c, time);
	ret = VecLerp(d, e, time);
	return ret;
}

glm::quat SplineSLerp(Spline input, float time) {
	glm::quat ret;
	glm::quat a;
	glm::quat b;
	glm::quat c;
	glm::quat d;
	glm::quat e;
	a = SLerp(EulToQuat(input.controls[0].rot), EulToQuat(input.controls[1].rot), time);
	b = SLerp(EulToQuat(input.controls[1].rot), EulToQuat(input.controls[2].rot), time);
	c = SLerp(EulToQuat(input.controls[2].rot), EulToQuat(input.controls[3].rot), time);
	d = SLerp(a, b, time);
	e = SLerp(b, c, time);
	ret = SLerp(d, e, time);
	return ret;
}

void drawSpline(Spline& input) {
	unsigned int quadVAO, quadVBO;
	glGenVertexArrays(1, &quadVAO);
	glGenBuffers(1, &quadVBO);
	glBindVertexArray(quadVAO);
	glBindBuffer(GL_ARRAY_BUFFER, quadVBO);
	for (int i = 0; i < 50; i++) {
		glm::vec3 one = SplinePosLerp(input, i / 50.0f);
		glm::vec3 two = SplinePosLerp(input, (i + 1) / 50.0f);
		float quadVertices[] = { // current spline display.
			// positions         // texCoords
			one.x, one.y, one.z, 1.0f, 0.0f, 0.0f,
			two.x, two.y, two.z, 1.0f, 0.0f, 0.0f
		};
		glBufferData(GL_ARRAY_BUFFER, sizeof(quadVertices), &quadVertices, GL_STATIC_DRAW);
		glEnableVertexAttribArray(0);
		glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(float), (void*)0);
		glEnableVertexAttribArray(1);
		glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(float), (void*)(3 * sizeof(float)));
		glBindVertexArray(quadVAO);
		glDrawArrays(GL_LINES, 0, 2);
	}
}

//This code is a slightly reformatted version of a function written at https://gamedev.stackexchange.com/questions/28395/rotating-vector3-by-a-quaternion
//Credit to stackoverflow.com user Laurent Couvidou, who has saved me a lot of time making this happen.
glm::vec3 RotateVec3(glm::vec3 input, glm::quat rotate) {
	glm::vec3 ret;

	// Extract the vector part of the quaternion
	glm::quat rotNorm = glm::normalize(rotate);
	glm::vec3 rotVec = glm::vec3(rotNorm.x, rotNorm.y, rotNorm.z);

	// Extract the scalar part of the quaternion
	float s = rotate.w;

	// Do the math
	ret = 2.0f * glm::dot(rotVec, input) * rotVec + (s * s - glm::dot(rotVec, rotVec)) * input + 2.0f * s * glm::cross(rotVec, input);

	return ret;
}

void UpdateSpline(int index) {
	if (index > 0) {
		for (int i = 0; i < 3; i++) {
			splines[index]->controls[0].pos[i] = splines[index - 1]->controls[3].pos[i];
			splines[index]->controls[0].rot[i] = splines[index - 1]->controls[3].rot[i];
			splines[index]->controls[0].sca[i] = splines[index - 1]->controls[3].sca[i];
			splines[index]->startSize = splines[index - 1]->endSize;
		}
	}
	glm::vec3 c1fk1 = glm::vec3(1.0f, 0.0f, 0.0f);
	c1fk1 = c1fk1 * splines[index]->startSize;
	c1fk1 = RotateVec3(c1fk1, EulToQuat(splines[index]->controls[0].rot));
	glm::vec3 c2fk2 = glm::vec3(-1.0f, 0.0f, 0.0f);
	c2fk2 = c2fk2 * splines[index]->endSize;
	c2fk2 = RotateVec3(c2fk2, EulToQuat(splines[index]->controls[3].rot));
	for (int i = 0; i < 3; i++) {
		splines[index]->controls[1].pos[i] = splines[index]->controls[0].pos[i] + c1fk1[i];
		splines[index]->controls[2].pos[i] = splines[index]->controls[3].pos[i] + c2fk2[i];
	}
}

void CreateSpline() {
	splines.push_back(new Spline());
	int curr = splines.size() - 1;
	if (splines.size() > 1) {
		splines[curr]->controls[0].pos[0] = splines[curr - 1]->controls[3].pos[0];
		splines[curr]->controls[0].pos[1] = splines[curr - 1]->controls[3].pos[1];
		splines[curr]->controls[0].pos[2] = splines[curr - 1]->controls[3].pos[2];
	}
	splines[curr]->controls[1].pos[0] = splines[curr]->controls[0].pos[0] + 1.0f;
	splines[curr]->controls[1].pos[1] = splines[curr]->controls[0].pos[1];
	splines[curr]->controls[1].pos[2] = splines[curr]->controls[0].pos[2];
	splines[curr]->controls[3].pos[0] = splines[curr]->controls[0].pos[0] + 3.0f;
	splines[curr]->controls[3].pos[1] = splines[curr]->controls[0].pos[1] + 3.0f;
	splines[curr]->controls[3].pos[2] = splines[curr]->controls[0].pos[2] + 3.0f;
	splines[curr]->controls[2].pos[0] = splines[curr]->controls[3].pos[0] - 1.0f;
	splines[curr]->controls[2].pos[1] = splines[curr]->controls[3].pos[1];
	splines[curr]->controls[2].pos[2] = splines[curr]->controls[3].pos[2];
}

int main() {
	GLFWwindow* window = initWindow("Assignment 0", screenWidth, screenHeight);
	glfwSetFramebufferSizeCallback(window, framebufferSizeCallback);
	ew::Shader shader = ew::Shader("assets/lit.vert", "assets/lit.frag");
	ew::Shader shadow = ew::Shader("assets/light.vert", "assets/light.frag");
	ew::Shader shaded = ew::Shader("assets/shade.vert", "assets/shade.frag");
	ew::Model monkey = ew::Model("assets/suzanne.obj");
	ew::MeshData planeData = ew::createPlane(50, 50, 1);
	ew::Mesh plane = ew::Mesh(planeData);
	ew::MeshData pointLightData = ew::createSphere(0.05f, 20);
	ew::Mesh pointLight = ew::Mesh(pointLightData);
	ew::MeshData splinePointData = ew::createSphere(0.1f, 20);
	ew::Mesh splinePoint = ew::Mesh(splinePointData);
	ew::Transform monkeyTrans;
	ew::Transform planeTrans;
	ew::Transform lightTrans;
	ew::Transform pointsTrans;
	ew::Transform linesTrans;
	GLuint ornamentTexture = ew::loadTexture("assets/ornament_color.png");

	cam.position = glm::vec3(0.0f, 0.0f, 5.0f);
	cam.target = glm::vec3(0.0f, 0.0f, 0.0f);
	cam.aspectRatio = (float)screenWidth / screenHeight;
	cam.fov = 60.0f;

	light.position = glm::vec3(-5.0f, 10.0f, -3.0f);
	lightDir = -light.position;
	light.target = glm::vec3(0.0f, 0.0f, 0.0f);
	light.aspectRatio = (float)screenWidth / screenHeight;
	light.fov = 60.0f;
	light.orthographic = true;
	light.orthoHeight = 10.0f;
	

	lightTrans.position = glm::vec3(-5.0f, 10.0f, -3.0f);

	planeTrans.position = glm::vec3(0.0f, -10.0f, 0.0f);

	glEnable(GL_CULL_FACE);
	glCullFace(GL_BACK); //Back face culling
	glEnable(GL_DEPTH_TEST); //Depth testing
	glDepthFunc(GL_LESS);

	unsigned int fbo;
	glGenFramebuffers(1, &fbo);
	glBindFramebuffer(GL_FRAMEBUFFER, fbo);

	glClearColor(0.6f, 0.8f, 0.92f, 1.0f);
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

	glGenTextures(1, &depthMap);
	glBindTexture(GL_TEXTURE_2D, depthMap);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH_COMPONENT, screenWidth, screenHeight, 0, GL_DEPTH_COMPONENT, GL_FLOAT, NULL);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_BORDER);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_BORDER);
	float borderColor[] = { 1.0f, 1.0f, 1.0f, 1.0f };
	glTexParameterfv(GL_TEXTURE_2D, GL_TEXTURE_BORDER_COLOR, borderColor);

	glBindFramebuffer(GL_FRAMEBUFFER, fbo);
	glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, depthMap, 0);
	glDrawBuffer(GL_NONE);
	glReadBuffer(GL_NONE);
	glBindFramebuffer(GL_FRAMEBUFFER, 0);

	glBindTextureUnit(0, ornamentTexture);

	CreateSpline();

	while (!glfwWindowShouldClose(window)) {
		glfwPollEvents();

		light.aspectRatio = (float)screenWidth / screenHeight;
		light.position = -lightDir;
		lightTrans.position = -lightDir;
		shadow.setVec3("_EyePos", light.position);
		cam.aspectRatio = (float)screenWidth / screenHeight;
		camCon.move(window, &cam, deltaTime);
		shader.setVec3("_EyePos", cam.position);

		for (int i = 0; i < splines.size(); i++) {
			if (timeExposure > (float)i && timeExposure < (i + 1.0f)) {
				monkeyTrans.position = SplinePosLerp(*splines[i], timeExposure - (float)i);
				monkeyTrans.rotation = SplineSLerp(*splines[i], timeExposure - (float)i);
				monkeyTrans.scale = SplineScaLerp(*splines[i], timeExposure - (float)i);
			}
		}

		float time = (float)glfwGetTime();
		deltaTime = time - prevFrameTime;
		prevFrameTime = time;

		//RENDER
		glClearColor(0.6f,0.8f,0.92f,1.0f);
		glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

		glBindFramebuffer(GL_FRAMEBUFFER, fbo);
		glClear(GL_DEPTH_BUFFER_BIT);

		glCullFace(GL_FRONT);
		shadow.use();
		shadow.setMat4("_Model", monkeyTrans.modelMatrix());
		shadow.setMat4("_ViewProjection", light.projectionMatrix() * light.viewMatrix());
		shadow.setFloat("_Material.Ka", material.Ka);
		shadow.setFloat("_Material.Kd", material.Kd);
		shadow.setFloat("_Material.Ks", material.Ks);
		shadow.setFloat("_Material.Shininess", material.Shiny);
		monkey.draw();

		glCullFace(GL_BACK);
		shadow.setMat4("_Model", planeTrans.modelMatrix());
		plane.draw();

		pointsTrans.position = VecFy(splines[0]->controls[0].pos);
		pointsTrans.rotation = EulToQuat(splines[0]->controls[0].rot);
		shadow.setMat4("_Model", pointsTrans.modelMatrix());
		splinePoint.draw();
		for (int i = 0; i < splines.size(); i++) {
			pointsTrans.position = VecFy(splines[i]->controls[1].pos);
			pointsTrans.rotation = EulToQuat(splines[i]->controls[1].rot);
			shadow.setMat4("_Model", pointsTrans.modelMatrix());
			pointLight.draw();
			pointsTrans.position = VecFy(splines[i]->controls[2].pos);
			pointsTrans.rotation = EulToQuat(splines[i]->controls[2].rot);
			shadow.setMat4("_Model", pointsTrans.modelMatrix());
			pointLight.draw();
			pointsTrans.position = VecFy(splines[i]->controls[3].pos);
			pointsTrans.rotation = EulToQuat(splines[i]->controls[3].rot);
			shadow.setMat4("_Model", pointsTrans.modelMatrix());
			splinePoint.draw();
		}

		glBindFramebuffer(GL_FRAMEBUFFER, 0);
		glCullFace(GL_BACK);

		if (shadowToggle)
		{
			glBindTextureUnit(0, depthMap);
			glBindTextureUnit(1, depthMap);
			shaded.use();
			shaded.setMat4("_Model", planeTrans.modelMatrix());
			shaded.setMat4("_ViewProjection", cam.projectionMatrix() * cam.viewMatrix());
			shaded.setFloat("_Material.Ka", material.Ka);
			shaded.setFloat("_Material.Kd", material.Kd);
			shaded.setFloat("_Material.Ks", material.Ks);
			shaded.setFloat("_Material.Shininess", material.Shiny);
			shaded.setMat4("_LightSpaceMatrix", light.projectionMatrix() * light.viewMatrix());
			shaded.setInt("_ShadowMap", 1);
			shaded.setInt("_MainTex", 0);
			shaded.setVec3("_ShadowMapDirection", light.position);
			shaded.setFloat("_MinBias", minBias);
			shaded.setFloat("_MaxBias", maxBias);
			shaded.setInt("_PCFSamplesSqrRt", pcfSampleInt);
			plane.draw();

			glBindTextureUnit(0, ornamentTexture);
			shaded.setMat4("_Model", monkeyTrans.modelMatrix());
			monkey.draw();

			shaded.setMat4("_Model", lightTrans.modelMatrix());
			pointLight.draw();

			pointsTrans.position = VecFy(splines[0]->controls[0].pos);
			pointsTrans.rotation = EulToQuat(splines[0]->controls[0].rot);
			shaded.setMat4("_Model", pointsTrans.modelMatrix());
			splinePoint.draw();
			for (int i = 0; i < splines.size(); i++) {
				pointsTrans.position = VecFy(splines[i]->controls[1].pos);
				pointsTrans.rotation = EulToQuat(splines[i]->controls[1].rot);
				shaded.setMat4("_Model", pointsTrans.modelMatrix());
				pointLight.draw();
				pointsTrans.position = VecFy(splines[i]->controls[2].pos);
				pointsTrans.rotation = EulToQuat(splines[i]->controls[2].rot);
				shaded.setMat4("_Model", pointsTrans.modelMatrix());
				pointLight.draw();
				pointsTrans.position = VecFy(splines[i]->controls[3].pos);
				pointsTrans.rotation = EulToQuat(splines[i]->controls[3].rot);
				shaded.setMat4("_Model", pointsTrans.modelMatrix());
				splinePoint.draw();
				shaded.setMat4("_Model", linesTrans.modelMatrix());
				drawSpline(*splines[i]);
			}
		}
		else
		{
			glBindTextureUnit(0, depthMap);
			shader.use();
			shader.setMat4("_Model", planeTrans.modelMatrix());
			shader.setMat4("_ViewProjection", cam.projectionMatrix() * cam.viewMatrix());
			shader.setFloat("_Material.Ka", material.Ka);
			shader.setFloat("_Material.Kd", material.Kd);
			shader.setFloat("_Material.Ks", material.Ks);
			shader.setFloat("_Material.Shininess", material.Shiny);
			plane.draw();

			glBindTextureUnit(0, ornamentTexture);
			shader.setMat4("_Model", monkeyTrans.modelMatrix());
			monkey.draw();

			shader.setMat4("_Model", lightTrans.modelMatrix());
			pointLight.draw();


			pointsTrans.position = VecFy(splines[0]->controls[0].pos);
			pointsTrans.rotation = EulToQuat(splines[0]->controls[0].rot);
			shaded.setMat4("_Model", pointsTrans.modelMatrix());
			splinePoint.draw();
			for (int i = 0; i < splines.size(); i++) {
				pointsTrans.position = VecFy(splines[i]->controls[1].pos);
				pointsTrans.rotation = EulToQuat(splines[i]->controls[1].rot);
				shaded.setMat4("_Model", pointsTrans.modelMatrix());
				pointLight.draw();
				pointsTrans.position = VecFy(splines[i]->controls[2].pos);
				pointsTrans.rotation = EulToQuat(splines[i]->controls[2].rot);
				shaded.setMat4("_Model", pointsTrans.modelMatrix());
				pointLight.draw();
				pointsTrans.position = VecFy(splines[i]->controls[3].pos);
				pointsTrans.rotation = EulToQuat(splines[i]->controls[3].rot);
				shaded.setMat4("_Model", pointsTrans.modelMatrix());
				splinePoint.draw();
				shaded.setMat4("_Model", linesTrans.modelMatrix());
				drawSpline(*splines[i]);
			}
		}

		drawUI();

		timeExposure += deltaTime * splines.size();

		if (timeExposure > splines.size() * 1.0f) {
			timeExposure = 0.0f;
		}

		for (int i = 0; i < splines.size(); i++) {
			UpdateSpline(i);
		}

		glfwSwapBuffers(window);
	}
	printf("Shutting down...");
}

void resetCamera(ew::Camera* camera, ew::CameraController* controller)
{
	camera->position = glm::vec3(0, 0, 5.0f);
	camera->target = glm::vec3(0);
	controller->yaw = controller->pitch = 0;
}

void drawUI() {
	ImGui_ImplGlfw_NewFrame();
	ImGui_ImplOpenGL3_NewFrame();
	ImGui::NewFrame();

	ImGui::Begin("Settings");
	if (ImGui::Button("Reset Camera")) {
		resetCamera(&cam, &camCon);
	}
	if (ImGui::CollapsingHeader("Material")) {
		ImGui::SliderFloat("AmbientK", &material.Ka, 0.0f, 1.0f);
		ImGui::SliderFloat("DiffuseK", &material.Kd, 0.0f, 1.0f);
		ImGui::SliderFloat("SpecularK", &material.Ks, 0.0f, 1.0f);
		ImGui::SliderFloat("Shininess", &material.Shiny, 2.0f, 1024.0f);
	}
	if (ImGui::CollapsingHeader("Light Direction")) {
		ImGui::SliderFloat("X", &lightDir.x, -10.0f, 10.0f);
		ImGui::SliderFloat("Y", &lightDir.y, -10.0f, 10.0f);
		ImGui::SliderFloat("Z", &lightDir.z, -10.0f, 10.0f);
	}
	if (ImGui::CollapsingHeader("Shadow Render Controls")) {
		ImGui::SliderFloat("Min. Bias", &minBias, 0.001f, maxBias);
		ImGui::SliderFloat("Max. Bias", &maxBias, minBias, 0.1f);
		ImGui::SliderInt("PCF Axis Samples", &pcfSampleInt, 0, 9);
	}
	if (ImGui::Button("Toggle Shadows")) {
		if (shadowToggle) shadowToggle = false;
		else shadowToggle = true;
	}
	if (ImGui::CollapsingHeader("Splines")) {
		for (int i = 0; i < splines.size(); i++) {
			std::string header = std::string("Spline " + std::to_string(i));
			if (ImGui::CollapsingHeader(header.c_str())) {
				if (i == 0) {
					if (ImGui::CollapsingHeader("Start Point")) {
						ImGui::DragFloat3("Position", splines[0]->controls[0].pos, 0.1f, -10.0f, 10.0f);
						ImGui::DragFloat3("Rotation", splines[0]->controls[0].rot, 0.1f, -10.0f, 10.0f);
						ImGui::DragFloat3("Scale", splines[0]->controls[0].sca, 0.1f, -10.0f, 10.0f);
						ImGui::DragFloat("Knot Size", &splines[0]->startSize, 0.05f, 0.1f, 5.0f);
					}
				}
				std::string one = std::string(std::to_string(i) + ": Sub 1");
				if (ImGui::CollapsingHeader(one.c_str())) {
					ImGui::DragFloat3("End: Rotation", splines[i]->controls[1].rot, 0.1f, -10.0f, 10.0f);
					ImGui::DragFloat3("End: Scale", splines[i]->controls[1].sca, 0.1f, -10.0f, 10.0f);
				}
				std::string two = std::string(std::to_string(i) + ": Sub 2");
				if (ImGui::CollapsingHeader(two.c_str())) {
					ImGui::DragFloat3("End: Rotation", splines[i]->controls[2].rot, 0.1f, -10.0f, 10.0f);
					ImGui::DragFloat3("End: Scale", splines[i]->controls[2].sca, 0.1f, -10.0f, 10.0f);
				}
				std::string three = std::string(std::to_string(i) + ": End Point");
				if (ImGui::CollapsingHeader(three.c_str())) {
					ImGui::DragFloat3("End: Position", splines[i]->controls[3].pos, 0.1f, -10.0f, 10.0f);
					ImGui::DragFloat3("End: Rotation", splines[i]->controls[3].rot, 0.1f, -10.0f, 10.0f);
					ImGui::DragFloat3("End: Scale", splines[i]->controls[3].sca, 0.1f, -10.0f, 10.0f);
					ImGui::DragFloat("End: Knot Size", &splines[i]->endSize, 0.05f, 0.1f, 5.0f);
				}
			}
		}
		if (ImGui::Button("Add Spline")) {
			CreateSpline();
		}
		if (ImGui::Button("Remove Spline")) {
			if (splines.size() > 1) {
				delete splines.back();
				splines.back() = nullptr;
				splines.pop_back();
			}
		}
	}

	ImGui::End();

	ImGui::Begin("Shadow Map");
	//Using a Child allow to fill all the space of the window.
	ImGui::BeginChild("Shadow Map");
	//Stretch image to be window size
	ImVec2 windowSize = ImGui::GetWindowSize();
	//Invert 0-1 V to flip vertically for ImGui display
	ImGui::Image((ImTextureID)depthMap, windowSize, ImVec2(0, 1), ImVec2(1, 0));
	ImGui::EndChild();
	ImGui::End();

	ImGui::Render();
	ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
}

void framebufferSizeCallback(GLFWwindow* window, int width, int height)
{
	glViewport(0, 0, width, height);
	screenWidth = width;
	screenHeight = height;
}

/// <summary>
/// Initializes GLFW, GLAD, and IMGUI
/// </summary>
/// <param name="title">Window title</param>
/// <param name="width">Window width</param>
/// <param name="height">Window height</param>
/// <returns>Returns window handle on success or null on fail</returns>
GLFWwindow* initWindow(const char* title, int width, int height) {
	printf("Initializing...");
	if (!glfwInit()) {
		printf("GLFW failed to init!");
		return nullptr;
	}

	GLFWwindow* window = glfwCreateWindow(width, height, title, NULL, NULL);
	if (window == NULL) {
		printf("GLFW failed to create window");
		return nullptr;
	}
	glfwMakeContextCurrent(window);

	if (!gladLoadGL(glfwGetProcAddress)) {
		printf("GLAD Failed to load GL headers");
		return nullptr;
	}

	//Initialize ImGUI
	IMGUI_CHECKVERSION();
	ImGui::CreateContext();
	ImGui_ImplGlfw_InitForOpenGL(window, true);
	ImGui_ImplOpenGL3_Init();

	return window;
}
