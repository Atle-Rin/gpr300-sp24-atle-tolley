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

#include <cmath>

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

enum lerpType {
	BASIC = 0,
	ACCELERATE,
	DECELERATE,
	S_CURVE
};

char** lerpTypes;

float Lerp(float A, float B, float time, lerpType type) {
	switch (type)
	{
	case BASIC:
		return A + (B - A) * time;
	case ACCELERATE:
		return A + (B - A) * time * time * time;
	case DECELERATE:
		return A + (B - A) * (1 - std::pow(1 - time, 3));
	case S_CURVE:
		Lerp(Lerp(A, B, time, ACCELERATE), Lerp(A, B, time, DECELERATE), time, BASIC);
	default:
		break;
	}
}

struct Frame {
	float startTime = 0.0f;
	float length = 1.0f;
	float values[3] = { 0.0f, 0.0f, 0.0f };
	lerpType type = BASIC;
};

struct Animation {
	std::vector<Frame*> frames;
	float startTime = 0.0f;
	float duration;
	bool play = true;
};

struct Animator {
	Animation* animations[3];
	float timeLimit;
	float timeExposure = 0.0f;
	float timeSpeed = 1.0f;
	bool loop = true;
};

Animator player;

Frame* addFrame(glm::vec3 inValues, float inStart, float inLength, lerpType inType) {
	Frame* ret = new Frame();
	for (int i = 0; i < 3; i++) {
		ret->values[i] = inValues[i];
	}
	ret->startTime = inStart;
	ret->length = inLength;
	ret->type = inType;
	return ret;
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
	ew::Transform monkeyTrans;
	ew::Transform monkeyStartTrans;
	ew::Transform planeTrans;
	ew::Transform lightTrans;
	GLuint ornamentTexture = ew::loadTexture("assets/ornament_color.png");

	lerpTypes = new char* [4];
	for (int i = 0; i < 4; i++) {
		lerpTypes[i] = new char[11];
	}
	lerpTypes[0] = "Basic";
	lerpTypes[1] = "Accelerate";
	lerpTypes[2] = "Decelerate";
	lerpTypes[3] = "S-Curve";

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

	Animation* movements = new Animation();
	Animation* turns = new Animation();
	Animation* resizes = new Animation();
	player.animations[0] = movements;
	player.animations[1] = turns;
	player.animations[2] = resizes;

	while (!glfwWindowShouldClose(window)) {
		glfwPollEvents();

		light.aspectRatio = (float)screenWidth / screenHeight;
		light.position = -lightDir;
		lightTrans.position = -lightDir;
		shadow.setVec3("_EyePos", light.position);
		cam.aspectRatio = (float)screenWidth / screenHeight;
		camCon.move(window, &cam, deltaTime);
		shader.setVec3("_EyePos", cam.position);

		if (player.animations[0]->play)
		{
			for (int i = 0; i < player.animations[0]->frames.size(); i++)
			{
				if (player.animations[0]->frames[i]->startTime < player.timeExposure && player.timeExposure < (player.animations[0]->frames[i]->startTime + player.animations[0]->frames[i]->length))
				{
					if (i == 0) {
						monkeyTrans.position.x = Lerp(monkeyStartTrans.position.x, player.animations[0]->frames[i]->values[0], (player.timeExposure - player.animations[0]->frames[i]->startTime) / player.animations[0]->frames[i]->length, player.animations[0]->frames[i]->type);
						monkeyTrans.position.y = Lerp(monkeyStartTrans.position.y, player.animations[0]->frames[i]->values[1], (player.timeExposure - player.animations[0]->frames[i]->startTime) / player.animations[0]->frames[i]->length, player.animations[0]->frames[i]->type);
						monkeyTrans.position.z = Lerp(monkeyStartTrans.position.z, player.animations[0]->frames[i]->values[2], (player.timeExposure - player.animations[0]->frames[i]->startTime) / player.animations[0]->frames[i]->length, player.animations[0]->frames[i]->type);
					}
					else {
						monkeyTrans.position.x = Lerp(player.animations[0]->frames[i - 1]->values[0], player.animations[0]->frames[i]->values[0], (player.timeExposure - player.animations[0]->frames[i]->startTime) / player.animations[0]->frames[i]->length, player.animations[0]->frames[i]->type);
						monkeyTrans.position.y = Lerp(player.animations[0]->frames[i - 1]->values[1], player.animations[0]->frames[i]->values[1], (player.timeExposure - player.animations[0]->frames[i]->startTime) / player.animations[0]->frames[i]->length, player.animations[0]->frames[i]->type);
						monkeyTrans.position.z = Lerp(player.animations[0]->frames[i - 1]->values[2], player.animations[0]->frames[i]->values[2], (player.timeExposure - player.animations[0]->frames[i]->startTime) / player.animations[0]->frames[i]->length, player.animations[0]->frames[i]->type);
					}
				}
			}
		}
		if (player.animations[1]->play)
		{
			for (int i = 0; i < player.animations[1]->frames.size(); i++)
			{
				if (player.animations[1]->frames[i]->startTime < player.timeExposure && player.timeExposure < (player.animations[1]->frames[i]->startTime + player.animations[1]->frames[i]->length))
				{
					if (i == 0) {
						monkeyTrans.rotation.x = Lerp(monkeyStartTrans.rotation.x, player.animations[1]->frames[i]->values[0], (player.timeExposure - player.animations[1]->frames[i]->startTime) / player.animations[1]->frames[i]->length, player.animations[1]->frames[i]->type);
						monkeyTrans.rotation.y = Lerp(monkeyStartTrans.rotation.y, player.animations[1]->frames[i]->values[1], (player.timeExposure - player.animations[1]->frames[i]->startTime) / player.animations[1]->frames[i]->length, player.animations[1]->frames[i]->type);
						monkeyTrans.rotation.z = Lerp(monkeyStartTrans.rotation.z, player.animations[1]->frames[i]->values[2], (player.timeExposure - player.animations[1]->frames[i]->startTime) / player.animations[1]->frames[i]->length, player.animations[1]->frames[i]->type);
					}
					else {
						monkeyTrans.rotation.x = Lerp(player.animations[1]->frames[i - 1]->values[0], player.animations[1]->frames[i]->values[0], (player.timeExposure - player.animations[1]->frames[i]->startTime) / player.animations[1]->frames[i]->length, player.animations[1]->frames[i]->type);
						monkeyTrans.rotation.y = Lerp(player.animations[1]->frames[i - 1]->values[1], player.animations[1]->frames[i]->values[1], (player.timeExposure - player.animations[1]->frames[i]->startTime) / player.animations[1]->frames[i]->length, player.animations[1]->frames[i]->type);
						monkeyTrans.rotation.z = Lerp(player.animations[1]->frames[i - 1]->values[2], player.animations[1]->frames[i]->values[2], (player.timeExposure - player.animations[1]->frames[i]->startTime) / player.animations[1]->frames[i]->length, player.animations[1]->frames[i]->type);
					}
				}
			}
		}
		if (player.animations[2]->play)
		{
			for (int i = 0; i < player.animations[2]->frames.size(); i++)
			{
				if (player.animations[2]->frames[i]->startTime < player.timeExposure && player.timeExposure < (player.animations[2]->frames[i]->startTime + player.animations[2]->frames[i]->length))
				{
					if (i == 0) {
						monkeyTrans.scale.x = Lerp(monkeyStartTrans.scale.x, player.animations[2]->frames[i]->values[0], (player.timeExposure - player.animations[2]->frames[i]->startTime) / player.animations[2]->frames[i]->length, player.animations[2]->frames[i]->type);
						monkeyTrans.scale.y = Lerp(monkeyStartTrans.scale.y, player.animations[2]->frames[i]->values[1], (player.timeExposure - player.animations[2]->frames[i]->startTime) / player.animations[2]->frames[i]->length, player.animations[2]->frames[i]->type);
						monkeyTrans.scale.z = Lerp(monkeyStartTrans.scale.z, player.animations[2]->frames[i]->values[2], (player.timeExposure - player.animations[2]->frames[i]->startTime) / player.animations[2]->frames[i]->length, player.animations[2]->frames[i]->type);
					}
					else {
						monkeyTrans.scale.x = Lerp(player.animations[2]->frames[i - 1]->values[0], player.animations[2]->frames[i]->values[0], (player.timeExposure - player.animations[2]->frames[i]->startTime) / player.animations[2]->frames[i]->length, player.animations[2]->frames[i]->type);
						monkeyTrans.scale.y = Lerp(player.animations[2]->frames[i - 1]->values[1], player.animations[2]->frames[i]->values[1], (player.timeExposure - player.animations[2]->frames[i]->startTime) / player.animations[2]->frames[i]->length, player.animations[2]->frames[i]->type);
						monkeyTrans.scale.z = Lerp(player.animations[2]->frames[i - 1]->values[2], player.animations[2]->frames[i]->values[2], (player.timeExposure - player.animations[2]->frames[i]->startTime) / player.animations[2]->frames[i]->length, player.animations[2]->frames[i]->type);
					}
				}
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

		glBindFramebuffer(GL_FRAMEBUFFER, 0);
		glCullFace(GL_BACK);

		if (shadowToggle)
		{
			glBindTextureUnit(1, depthMap);
			shaded.use();
			shaded.setMat4("_Model", monkeyTrans.modelMatrix());
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
			monkey.draw();

			shaded.setMat4("_Model", planeTrans.modelMatrix());
			plane.draw();

			shaded.setMat4("_Model", lightTrans.modelMatrix());
			pointLight.draw();
		}
		else
		{
			shader.use();
			shader.setMat4("_Model", monkeyTrans.modelMatrix());
			shader.setMat4("_ViewProjection", cam.projectionMatrix() * cam.viewMatrix());
			shader.setFloat("_Material.Ka", material.Ka);
			shader.setFloat("_Material.Kd", material.Kd);
			shader.setFloat("_Material.Ks", material.Ks);
			shader.setFloat("_Material.Shininess", material.Shiny);
			monkey.draw();

			shader.setMat4("_Model", planeTrans.modelMatrix());
			plane.draw();

			shader.setMat4("_Model", lightTrans.modelMatrix());
			pointLight.draw();
		}

		drawUI();

		float bigTotal = 0.0f;
		for (int i = 0; i < 3; i++)
		{
			float total = 0.0f;
			for (int j = 0; j < player.animations[i]->frames.size(); j++)
			{
				total += player.animations[i]->frames[j]->length;
			}
			player.animations[i]->duration = total;
			bigTotal += total;
		}
		player.timeLimit = bigTotal;
		player.timeExposure += deltaTime * player.timeSpeed;
		if (player.timeExposure > player.timeLimit && player.loop)
		{
			player.timeExposure = 0.0f;
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
	if (ImGui::CollapsingHeader("Animation Controls")) {
		ImGui::DragFloat("Total Length (Visibility, Not Editable)", &player.timeLimit, 0.0f, 240.0f);
		ImGui::SliderFloat("Timer", &player.timeExposure, 0.0f, player.timeLimit);
		ImGui::DragFloat("Playback Speed", &player.timeSpeed, -5.0f, 5.0f);
		if (ImGui::Button("Toggle Looping"))
		{
			if (player.loop) player.loop = false;
			else player.loop = true;
		}
	}
	if (ImGui::CollapsingHeader("Transform")) {
		for (int i = 0; i < player.animations[0]->frames.size(); i++) {
			std::string headerName = "Frame " + std::to_string(i);
			if (ImGui::CollapsingHeader(headerName.c_str())) {
				ImGui::SliderFloat("Start", &player.animations[0]->frames[i]->startTime, player.animations[0]->startTime, player.timeLimit);
				ImGui::DragFloat3("Values", player.animations[0]->frames[i]->values, 0.25f, -5.0f, 5.0f);
				ImGui::DragFloat("Length", &player.animations[0]->frames[i]->length, 0.1f, 0.1f, 3.0f);
				int lerpChange = player.animations[0]->frames[i]->type;
				ImGui::ListBox("Easing", &lerpChange, lerpTypes, 4);
				player.animations[0]->frames[i]->type = (lerpType)lerpChange;
			}
		}
		if (ImGui::Button("Toggle Play")) {
			if (player.animations[0]->play) player.animations[0]->play = false;
			else player.animations[0]->play = true;
		}
		if (ImGui::Button("Add Keyframe")) {
			player.animations[0]->frames.push_back(addFrame(glm::vec3(0.0f), 0.0f, 1.0f, BASIC));
		}
		if (ImGui::Button("Remove Keyframe")) {
			player.animations[0]->frames.pop_back();
		}
	}
	if (ImGui::CollapsingHeader("Rotate")) {
		for (int i = 0; i < player.animations[1]->frames.size(); i++) {
			std::string headerName = "Frame " + std::to_string(i);
			if (ImGui::CollapsingHeader(headerName.c_str())) {
				ImGui::SliderFloat("Start", &player.animations[1]->frames[i]->startTime, player.animations[1]->startTime, player.timeLimit);
				ImGui::DragFloat3("Values", player.animations[1]->frames[i]->values, 0.25f, -5.0f, 5.0f);
				ImGui::DragFloat("Length", &player.animations[1]->frames[i]->length, 0.1f, 0.1f, 3.0f);
				int lerpChange = player.animations[1]->frames[i]->type;
				ImGui::ListBox("Easing", &lerpChange, lerpTypes, 4);
				player.animations[1]->frames[i]->type = (lerpType)lerpChange;
			}
		}
		if (ImGui::Button("Toggle Play")) {
			if (player.animations[1]->play) player.animations[1]->play = false;
			else player.animations[1]->play = true;
		}
		if (ImGui::Button("Add Keyframe")) {
			player.animations[1]->frames.push_back(addFrame(glm::vec3(0.0f), 0.0f, 1.0f, BASIC));
		}
		if (ImGui::Button("Remove Keyframe")) {
			player.animations[1]->frames.pop_back();
		}
	}
	if (ImGui::CollapsingHeader("Scale")) {
		for (int i = 0; i < player.animations[2]->frames.size(); i++) {
			std::string headerName = "Frame " + std::to_string(i);
			if (ImGui::CollapsingHeader(headerName.c_str())) {
				ImGui::SliderFloat("Start", &player.animations[2]->frames[i]->startTime, player.animations[2]->startTime, player.timeLimit);
				ImGui::DragFloat3("Values", player.animations[2]->frames[i]->values, 0.25f, -5.0f, 5.0f);
				ImGui::DragFloat("Length", &player.animations[2]->frames[i]->length, 0.1f, 0.1f, 3.0f);
				int lerpChange = player.animations[2]->frames[i]->type;
				ImGui::ListBox("Easing", &lerpChange, lerpTypes, 4);
				player.animations[2]->frames[i]->type = (lerpType)lerpChange;
			}
		}
		if (ImGui::Button("Toggle Play")) {
			if (player.animations[2]->play) player.animations[2]->play = false;
			else player.animations[2]->play = true;
		}
		if (ImGui::Button("Add Keyframe")) {
			player.animations[2]->frames.push_back(addFrame(glm::vec3(0.0f), 0.0f, 1.0f, BASIC));
		}
		if (ImGui::Button("Remove Keyframe")) {
			player.animations[2]->frames.pop_back();
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
