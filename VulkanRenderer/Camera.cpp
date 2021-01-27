#include "Camera.h"

#include "Input.h"

#include "glm/gtc/matrix_transform.hpp"
#include "glm/gtx/euler_angles.hpp"

Camera::Camera()
{
	firstMove = true;
	sensitivity = 0.3f;
	lastMousePos = glm::vec2();

	position = glm::vec3();
	worldUp = glm::vec3(0.0f, 1.0f, 0.0f);
	front = glm::vec3(0.0f, 0.0f, -1.0f);
	up = worldUp;
	right = glm::vec3(1.0f, 0.0f, 0.0f);
	
	pitch = 0.0f;
	yaw = 180.0f;

	projectionMatrix = glm::mat4(1.0f);
	viewMatrix = glm::mat4(1.0f);

	moveSpeed = 4.0f;

	//UpdateCameraVectors();
}

void Camera::Update(float deltaTime, bool doMovement, bool needRightMouse)
{
	if (needRightMouse)
	{
		if (Input::IsMouseButtonDown(MouseButtonType::Right))
		{
			if (doMovement)
			{
				DoMovement(deltaTime);
			}

			DoLook();
		}
		else
		{
			Reset();
		}
	}
	else
	{
		if (doMovement)
		{
			DoMovement(deltaTime);
		}

		DoLook();
	}
}

void Camera::DoMovement(float dt)
{
	float velocity = moveSpeed * dt;

	if (Input::IsKeyPressed(KEY_W))		// w
	{
		position += front * velocity;
	}
	if (Input::IsKeyPressed(KEY_S))		// s
	{
		position -= front * velocity;
	}
	if (Input::IsKeyPressed(KEY_A))		// a
	{
		position -= right * velocity;
	}
	if (Input::IsKeyPressed(KEY_D))		// d
	{
		position += right * velocity;
	}
	if (Input::IsKeyPressed(KEY_E))		// e
	{
		position += worldUp * velocity;
	}
	if (Input::IsKeyPressed(KEY_Q))		// q
	{
		position -= worldUp * velocity;
	}
}

void Camera::DoLook()
{
	glm::vec2 mousePos = Input::GetMousePosition();

	if (firstMove == true)
	{
		lastMousePos = mousePos;
		firstMove = false;
	}

	float offsetX = lastMousePos.x - mousePos.x;
	float offsetY = mousePos.y - lastMousePos.y;
	lastMousePos = mousePos;

	offsetX *= sensitivity;
	offsetY *= sensitivity;

	yaw += offsetX;
	pitch += offsetY;

	if (pitch > 89.0f)
		pitch = 89.0f;
	if (pitch < -89.0f)
		pitch = -89.0f;

	UpdateCameraVectors();
}

void Camera::SetProjectionMatrix(float fov, int windowWidth, int windowHeight, float near, float far)
{
	lastMousePos.x = windowWidth / 2.0f;
	lastMousePos.y = windowHeight / 2.0f;

	width = windowWidth;
	height = windowHeight;

	this->fov = fov;
	aspectRatio = (float)windowWidth / windowHeight;
	nearPlane = near;
	farPlane = far;

	projectionMatrix = glm::perspective(glm::radians(fov), aspectRatio, near, far);
}

void Camera::SetPosition(const glm::vec3& pos)
{
	position = pos;
	viewMatrix = glm::lookAt(position, position + front, up);
}

void Camera::SetYaw(float yaw)
{
	this->yaw = yaw;
	UpdateCameraVectors();
}

void Camera::SetPitch(float pitch)
{
	this->pitch = pitch;
	UpdateCameraVectors();
}

void Camera::UpdateCameraVectors()
{
	glm::mat4 m = glm::eulerAngleYX(glm::radians(yaw), glm::radians(pitch));

	front = glm::normalize(m[2]);
	right = glm::normalize(glm::cross(front, glm::vec3(0.0f, 1.0f, 0.0f)));
	up = glm::normalize(glm::cross(right, front));

	viewMatrix = glm::lookAt(position, position + front, up);
}