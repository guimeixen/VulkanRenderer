#pragma once

#include "GLFW/glfw3.h"

#include <iostream>

class InputManager;

class Window
{
public:
	Window();

	bool Init(InputManager* inputManager, unsigned int width, unsigned int height);
	void Dispose();

	void UpdateInput();

	GLFWwindow* GetHandle() const { return window; }
	int ShouldClose() const { return glfwWindowShouldClose(window); }

	unsigned int GetWidth() const { return width; }
	unsigned int GetHeight() const { return height; }
	unsigned int GetMonitorWidth() const { return monitorWidth; }
	unsigned int GetMonitorHeight() const { return monitorHeight; }
	bool IsMinimized() const { return isMinimized; }

	bool WasResized();

private:
	void UpdateKeys(int key, int scancode, int action, int mods);
	void UpdateMousePosition(float xpos, float ypos);
	void UpdateMouseButtonState(int button, int action, int mods);
	void UpdateScroll(double xoffset, double yoffset);
	void UpdateChar(unsigned int c);
	void UpdateFocus(int focused);
	void UpdateFramebufferSize(int width, int height);

	inline static auto WindowKeyboardCallback(GLFWwindow* win, int key, int scancode, int action, int mods)
	{
		Window* window = static_cast<Window*>(glfwGetWindowUserPointer(win));
		window->UpdateKeys(key, scancode, action, mods);
	}

	inline static auto WindowMouseCallback(GLFWwindow* win, double xpos, double ypos)
	{
		Window* window = static_cast<Window*>(glfwGetWindowUserPointer(win));
		window->UpdateMousePosition(static_cast<float>(xpos), static_cast<float>(ypos));
	}

	inline static auto WindowMouseButtonCallback(GLFWwindow* win, int button, int action, int mods)
	{
		Window* window = static_cast<Window*>(glfwGetWindowUserPointer(win));
		window->UpdateMouseButtonState(button, action, mods);
	}

	inline static auto WindowScrollCallback(GLFWwindow* win, double xoffset, double yoffset)
	{
		Window* window = static_cast<Window*>(glfwGetWindowUserPointer(win));
		window->UpdateScroll(xoffset, yoffset);
	}

	inline static auto WindowCharCallback(GLFWwindow* win, unsigned int c)
	{
		Window* window = static_cast<Window*>(glfwGetWindowUserPointer(win));
		window->UpdateChar(c);
	}

	inline static auto WindowFocusCallback(GLFWwindow* win, int focused)
	{
		Window* window = static_cast<Window*>(glfwGetWindowUserPointer(win));
		window->UpdateFocus(focused);
	}

	inline static auto WindowFramebufferSizeCallback(GLFWwindow* win, int width, int height)
	{
		Window* window = static_cast<Window*>(glfwGetWindowUserPointer(win));
		window->UpdateFramebufferSize(width, height);
	}

private:
	GLFWwindow* window;
	InputManager* inputManager;

	unsigned int width;
	unsigned int height;
	unsigned int monitorWidth;
	unsigned int monitorHeight;
	bool isFocused;
	bool isFullscreen;
	bool wasResized = false;
	bool isMinimized = false;

	float lastXpos = 0.0f;
	float lastYpos = 0.0f;
};
