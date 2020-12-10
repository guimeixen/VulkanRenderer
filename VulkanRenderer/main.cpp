#include <vulkan/vulkan.h>
#include "GLFW/glfw3.h"

#include <iostream>

int main()
{
	if (glfwInit() != GLFW_TRUE)
	{
		std::cout << "Failed to initialize GLFW\n";
	}
	else
	{
		std::cout << "GLFW successfuly initialized\n";
	}

	glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
	GLFWwindow *window = glfwCreateWindow(640, 480, "VulkanRenderer", nullptr, nullptr);

	while (!glfwWindowShouldClose(window))
	{
		glfwPollEvents();
	}

	glfwTerminate();

	return 0;
}