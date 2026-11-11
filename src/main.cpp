#define GLFW_INCLUDE_NONE
#include <glad/glad.h>
#include <GLFW/glfw3.h>

#include <iostream>
#include <string>

#include <openglErrorReporting.h>
#include <shader.h>

#ifndef RESOURCES_PATH
#define RESOURCES_PATH "./resources/"
#endif


float triangleData[] = {
	//positions      colors
	//x  y  z        r  g  b
	  0, 1, 0,       1, 0, 0, // vertex 1
	  -1, -1, 0,     0, 1, 0, // vertex 2
	  1, -1, 0,      0, 0, 1  // vertex 3
};

int main(void)
{
	if (!glfwInit())
	{
		std::cout << "GLFW Init Error!\n";
		return 1;
	}

	glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
	glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
	glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
#ifdef __APPLE__
	glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GLFW_TRUE);
#endif

	GLFWwindow *window = glfwCreateWindow(640, 480, "Simple example", NULL, NULL);
	if (!window)
	{
		std::cout << "Window Error!\n";
		glfwTerminate();
		return 1;
	}

	glfwMakeContextCurrent(window);
	glfwSwapInterval(1);

	if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress))
	{
		std::cout << "GLAD Init Error!\n";
		glfwDestroyWindow(window);
		glfwTerminate();
		return 1;
	}

	enableReportGlErrors();

	int framebufferWidth = 0;
	int framebufferHeight = 0;
	glfwGetFramebufferSize(window, &framebufferWidth, &framebufferHeight);
	glViewport(0, 0, framebufferWidth, framebufferHeight);

	glfwSetFramebufferSizeCallback(window, [](GLFWwindow *, int width, int height)
	{
		glViewport(0, 0, width, height);
	});

#pragma region buffer
	// A core profile context requires a bound vertex array object.
	GLuint vertexArray = 0;
	glGenVertexArrays(1, &vertexArray);
	glBindVertexArray(vertexArray);

	//create the buffer
	GLuint buffer = 0;
	glGenBuffers(1, &buffer);

	//send the data to the buffer
	glBindBuffer(GL_ARRAY_BUFFER, buffer);
	glBufferData(GL_ARRAY_BUFFER, sizeof(triangleData), triangleData, GL_STATIC_DRAW);

	//the attribute representing position
	glEnableVertexAttribArray(0);
	glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(float) * 6, 0);

	//the attribute representing color
	glEnableVertexAttribArray(1);
	glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(float) * 6, (void *)(sizeof(float) * 3));
#pragma endregion

#pragma region shader loading
	Shader shader;
	std::string resourcesPath = RESOURCES_PATH;

	bool shaderLoaded = shader.loadShaderProgramFromFile(
		(resourcesPath + "myShader.vert").c_str(),
		(resourcesPath + "myShader.frag").c_str());

	if (!shaderLoaded)
	{
		std::cout << "Shader Error!\n";
		glDeleteBuffers(1, &buffer);
		glDeleteVertexArrays(1, &vertexArray);
		glfwDestroyWindow(window);
		glfwTerminate();
		return 1;
	}

	shader.bind();
#pragma endregion

	glClearColor(0.1f, 0.1f, 0.1f, 1.0f);

	while (!glfwWindowShouldClose(window))
	{
		glClear(GL_COLOR_BUFFER_BIT);
		glDrawArrays(GL_TRIANGLES, 0, 3);
		glfwSwapBuffers(window);
		glfwPollEvents();
	}

	shader.clear();
	glDeleteBuffers(1, &buffer);
	glDeleteVertexArrays(1, &vertexArray);

	glfwDestroyWindow(window);

	glfwTerminate();

	return 0;
}
