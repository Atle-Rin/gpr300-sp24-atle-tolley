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

struct ExposureTransform {
	float pos[3];
	float rot[3];
	float sca[3];
	ExposureTransform* parent;
};

std::vector<ExposureTransform*> transforms;

void addTransform(glm::vec3 pos, glm::vec3 rot, glm::vec3 sca, int parentIndex) {
	ExposureTransform* ret = new ExposureTransform;
	for (int i = 0; i < 3; i++) {
		ret->pos[i] = pos[i];
		ret->rot[i] = rot[i];
		ret->sca[i] = sca[i];
	}
	if (parentIndex == -1) {
		ret->parent = nullptr;
	}
	else {
		ret->parent = transforms[parentIndex];
	}
	transforms.push_back(ret);
}

glm::quat localRot(ExposureTransform* input) {
	glm::quat ret;
	if (input->parent != nullptr) {
		ret = localRot(input->parent);
		ret *= EulToQuat(input->rot);
	}
	else {
		ret = EulToQuat(input->rot);
	}
	return ret;
}

glm::vec3 localPos(ExposureTransform* input) {
	glm::vec3 ret;
	if (input->parent != nullptr) {
		ret = localPos(input->parent);
		ret += RotateVec3(VecFy(input->pos), localRot(input));
	}
	else {
		ret = VecFy(input->pos);
	}
	return ret;
}

glm::vec3 localSca(ExposureTransform* input) {
	glm::vec3 ret;
	if (input->parent != nullptr) {
		ret = localSca(input->parent);
		ret *= VecFy(input->sca);
	}
	else {
		ret = VecFy(input->sca);
	}
	return ret;
}

int selectedPart = 0;

char** selectorParts;

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
	ew::Transform planeTrans;
	ew::Transform lightTrans;
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
	
	selectorParts = new char* [8];
	for (int i = 0; i < 4; i++) {
		selectorParts[i] = new char[8];
	}
	selectorParts[0] = "Body";
	selectorParts[1] = "Shoulder";
	selectorParts[2] = "Elbow";
	selectorParts[3] = "Wrist";
	selectorParts[4] = "Hand";
	selectorParts[5] = "Thigh";
	selectorParts[6] = "Calf";
	selectorParts[7] = "Back-head";

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

	addTransform(glm::vec3(0.0f), glm::vec3(0.0f), glm::vec3(1.0f), -1);
	addTransform(glm::vec3(1.0f, 0.0f, 0.0f), glm::vec3(0.0f), glm::vec3(0.8f), 0);
	addTransform(glm::vec3(1.0f, 0.0f, 0.0f), glm::vec3(0.0f), glm::vec3(0.8f), 1);
	addTransform(glm::vec3(1.0f, 0.0f, 0.0f), glm::vec3(0.0f), glm::vec3(0.8f), 2);
	addTransform(glm::vec3(1.0f, 0.0f, 0.0f), glm::vec3(0.0f), glm::vec3(0.8f), 3);
	addTransform(glm::vec3(0.0f, -1.0f, 0.0f), glm::vec3(0.0f), glm::vec3(0.8f), 0);
	addTransform(glm::vec3(0.0f, -1.0f, 0.0f), glm::vec3(0.0f), glm::vec3(0.8f), 5);
	addTransform(glm::vec3(0.0f, 0.0f, -1.0f), glm::vec3(0.0f), glm::vec3(0.8f), 0);

	while (!glfwWindowShouldClose(window)) {
		glfwPollEvents();

		light.aspectRatio = (float)screenWidth / screenHeight;
		light.position = -lightDir;
		lightTrans.position = -lightDir;
		shadow.setVec3("_EyePos", light.position);
		cam.aspectRatio = (float)screenWidth / screenHeight;
		camCon.move(window, &cam, deltaTime);
		shader.setVec3("_EyePos", cam.position);

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
		shadow.setMat4("_ViewProjection", light.projectionMatrix() * light.viewMatrix());
		shadow.setFloat("_Material.Ka", material.Ka);
		shadow.setFloat("_Material.Kd", material.Kd);
		shadow.setFloat("_Material.Ks", material.Ks);
		shadow.setFloat("_Material.Shininess", material.Shiny);
		for (int i = 0; i < transforms.size(); i++) {
			monkeyTrans.position = localPos(transforms[i]);
			monkeyTrans.rotation = localRot(transforms[i]);
			monkeyTrans.scale = localSca(transforms[i]);
			shadow.setMat4("_Model", monkeyTrans.modelMatrix());
			monkey.draw();
		}

		glCullFace(GL_BACK);
		shadow.setMat4("_Model", planeTrans.modelMatrix());
		plane.draw();

		glBindFramebuffer(GL_FRAMEBUFFER, 0);
		glCullFace(GL_BACK);

		if (shadowToggle)
		{
			glBindTextureUnit(1, depthMap);
			shaded.use();
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
			for (int i = 0; i < transforms.size(); i++) {
				monkeyTrans.position = localPos(transforms[i]);
				monkeyTrans.rotation = localRot(transforms[i]);
				monkeyTrans.scale = localSca(transforms[i]);
				shaded.setMat4("_Model", monkeyTrans.modelMatrix());
				monkey.draw();
			}

			shaded.setMat4("_Model", planeTrans.modelMatrix());
			plane.draw();

			shaded.setMat4("_Model", lightTrans.modelMatrix());
			pointLight.draw();
		}
		else
		{
			shader.use();
			shader.setMat4("_ViewProjection", cam.projectionMatrix() * cam.viewMatrix());
			shader.setFloat("_Material.Ka", material.Ka);
			shader.setFloat("_Material.Kd", material.Kd);
			shader.setFloat("_Material.Ks", material.Ks);
			shader.setFloat("_Material.Shininess", material.Shiny);
			for (int i = 0; i < transforms.size(); i++) {
				monkeyTrans.position = localPos(transforms[i]);
				monkeyTrans.rotation = localRot(transforms[i]);
				monkeyTrans.scale = localSca(transforms[i]);
				shader.setMat4("_Model", monkeyTrans.modelMatrix());
				monkey.draw();
			}

			shader.setMat4("_Model", planeTrans.modelMatrix());
			plane.draw();

			shader.setMat4("_Model", lightTrans.modelMatrix());
			pointLight.draw();
		}

		drawUI();

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

	ImGui::Begin("Skeleton");
	ImGui::ListBox("Select Part:", &selectedPart, selectorParts, 8);
	ImGui::End();

	ImGui::Begin("Inspector");
	if (ImGui::CollapsingHeader(selectorParts[selectedPart])) {
		ImGui::DragFloat3("Position", transforms[selectedPart]->pos, 0.1f, -10.0f, 10.0f);
		ImGui::DragFloat3("Rotation", transforms[selectedPart]->rot, 0.1f, -10.0f, 10.0f);
		ImGui::DragFloat3("Scale", transforms[selectedPart]->sca, 0.1f, -10.0f, 10.0f);
	}
	ImGui::End();

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
