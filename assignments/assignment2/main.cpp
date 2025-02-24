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

int main() {
	GLFWwindow* window = initWindow("Assignment 0", screenWidth, screenHeight);
	glfwSetFramebufferSizeCallback(window, framebufferSizeCallback);
	ew::Shader shader = ew::Shader("assets/lit.vert", "assets/lit.frag");
	ew::Shader shadow = ew::Shader("assets/light.vert", "assets/light.frag");
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
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);

	glBindFramebuffer(GL_FRAMEBUFFER, fbo);
	glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, depthMap, 0);
	glDrawBuffer(GL_NONE);
	glReadBuffer(GL_NONE);
	glBindFramebuffer(GL_FRAMEBUFFER, 0);

	glBindTextureUnit(0, ornamentTexture);

	while (!glfwWindowShouldClose(window)) {
		glfwPollEvents();

		light.aspectRatio = (float)screenWidth / screenHeight;
		light.position = -lightDir;
		lightTrans.position = -lightDir;
		shadow.setVec3("_EyePos", light.position);
		cam.aspectRatio = (float)screenWidth / screenHeight;
		camCon.move(window, &cam, deltaTime);
		shader.setVec3("_EyePos", cam.position);

		monkeyTrans.rotation = glm::rotate(monkeyTrans.rotation, deltaTime, glm::vec3(0.0f, 1.0f, 0.0f));

		float time = (float)glfwGetTime();
		deltaTime = time - prevFrameTime;
		prevFrameTime = time;

		//RENDER
		glClearColor(0.6f,0.8f,0.92f,1.0f);
		glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

		glBindFramebuffer(GL_FRAMEBUFFER, fbo);
		glClear(GL_DEPTH_BUFFER_BIT);

		shadow.use();
		shadow.setMat4("_Model", monkeyTrans.modelMatrix());
		shadow.setMat4("_ViewProjection", light.projectionMatrix() * light.viewMatrix());
		shadow.setFloat("_Material.Ka", material.Ka);
		shadow.setFloat("_Material.Kd", material.Kd);
		shadow.setFloat("_Material.Ks", material.Ks);
		shadow.setFloat("_Material.Shininess", material.Shiny);
		monkey.draw();

		shadow.setMat4("_Model", planeTrans.modelMatrix());
		plane.draw();

		glBindFramebuffer(GL_FRAMEBUFFER, 0);

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

	ImGui::End();

	ImGui::Begin("Shadow Map");
	//Using a Child allow to fill all the space of the window.
	ImGui::BeginChild("Shadow Map");
	//Stretch image to be window size
	ImVec2 windowSize = ImGui::GetWindowSize();
	//Invert 0-1 V to flip vertically for ImGui display
	//shadowMap is the texture2D handle
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
