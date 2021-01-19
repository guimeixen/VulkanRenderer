#include "Window.h"

#include "Input.h"

Window::Window()
{
	inputManager = nullptr;
}

bool Window::Init(InputManager* inputManager, unsigned int width, unsigned int height)
{
		this->inputManager = inputManager;
		this->width = width;
		this->height = height;

		if (glfwInit() != GLFW_TRUE)
		{
			std::cout << "Failed to initialize GLFW\n";
			return false;
		}
		else
		{
			std::cout << "GLFW successfuly initialized\n";
		}

		//glfwWindowHint(GLFW_RESIZABLE, GL_FALSE);
		glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
		window = glfwCreateWindow((int)width, (int)height, "VulkanRenderer", nullptr, nullptr);

		glfwSetWindowPos(window, 400, 100);

		glfwSetWindowUserPointer(window, this);

		glfwSetKeyCallback(window, WindowKeyboardCallback);
		glfwSetCursorPosCallback(window, WindowMouseCallback);
		glfwSetMouseButtonCallback(window, WindowMouseButtonCallback);
		glfwSetWindowFocusCallback(window, WindowFocusCallback);
		glfwSetCharCallback(window, WindowCharCallback);
		glfwSetFramebufferSizeCallback(window, WindowFramebufferSizeCallback);

		glfwSetCursorPos(window, width / 2.0f, height / 2.0f);

		const GLFWvidmode* mode = glfwGetVideoMode(glfwGetPrimaryMonitor());
		monitorWidth = mode->width;
		monitorHeight = mode->height;

		//glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);

		return true;
	}

	void Window::Dispose()
	{
		glfwTerminate();
	}

	void Window::UpdateInput()
	{
		inputManager->Reset();
		inputManager->Update();

		glfwPollEvents();
	}

	bool Window::WasResized()
	{
		if (wasResized)
		{
			wasResized = false;
			return true;
		}
		return false;
	}

	void Window::UpdateKeys(int key, int scancode, int action, int mods)
	{
		/*if (key == GLFW_KEY_ESCAPE)
			glfwSetWindowShouldClose(window, GLFW_TRUE);
		*/
		inputManager->UpdateKeys(key, scancode, action, mods);

		if (key == GLFW_KEY_F11 && action == GLFW_RELEASE)
		{
			int count = 0;
			const GLFWvidmode* vidModes = glfwGetVideoModes(glfwGetPrimaryMonitor(), &count);

			int maxRefreshRate = 0;

			width = 0;
			height = 0;

			if (!isFullscreen)
			{
				for (int i = 0; i < count; i++)
				{
					if ((unsigned int)vidModes[i].width > width && (unsigned int)vidModes[i].height > height)
					{
						width = vidModes[i].width;
						height = vidModes[i].height;

						if (vidModes[i].refreshRate > maxRefreshRate)
							maxRefreshRate = vidModes[i].refreshRate;
					}
					//std::cout << vidModes[i].width << " " << vidModes[i].height << " " << vidModes[i].refreshRate << " " << vidModes[i].redBits << " " << vidModes[i].greenBits << " " << vidModes[i].blueBits << std::endl;
				}
				std::cout << width << " " << height << " " << maxRefreshRate << std::endl;
				glfwSetWindowMonitor(window, glfwGetPrimaryMonitor(), 0, 0, width, height, maxRefreshRate);

				isFullscreen = true;
			}
			else
			{
				width = 1280;
				height = 720;

				// Find max refresh rate for 1280x720
				for (int i = 0; i < count; i++)
				{
					if (vidModes[i].width == width && vidModes[i].height == height && vidModes[i].refreshRate > maxRefreshRate)
					{
						maxRefreshRate = vidModes[i].refreshRate;

						if (maxRefreshRate == 60)			// We dont want more than 60hz
							break;
					}
				}
				//std::cout << "Windowed refresh rate:" << maxRefreshRate << std::endl;
				glfwSetWindowMonitor(window, nullptr, 550, 50, width, height, maxRefreshRate);

				isFullscreen = false;
			}

			wasResized = true;
		}
	}

	void Window::UpdateMousePosition(float xpos, float ypos)
	{
		inputManager->SetMousePosition(glm::vec2(xpos, ypos));
	}

	void Window::UpdateMouseButtonState(int button, int action, int mods)
	{
		inputManager->SetMouseButtonState(button, action);
	}

	void Window::UpdateScroll(double xoffset, double yoffset)
	{
		inputManager->SetScrollWheelYOffset((float)yoffset);
	}

	void Window::UpdateChar(unsigned int c)
	{
		inputManager->UpdateChar(c);
	}

	void Window::UpdateFocus(int focused)
	{
		if (focused == GLFW_TRUE)
			isFocused = true;
		else if (focused == GLFW_FALSE)
			isFocused = false;
	}

	void Window::UpdateFramebufferSize(int width, int height)
	{
		//std::cout << "width : " << width << "    "  << height << '\n';
		this->width = static_cast<unsigned int>(width);
		this->height = static_cast<unsigned int>(height);

		if (width == 0 && height == 0)
			isMinimized = true;
		else
		{
			isMinimized = false;
			wasResized = true;
		}
	}