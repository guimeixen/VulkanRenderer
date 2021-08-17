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
	forward = glm::vec3(0.0f, 0.0f, -1.0f);
	up = worldUp;
	right = glm::vec3(1.0f, 0.0f, 0.0f);
	
	pitch = 0.0f;
	yaw = 180.0f;

	projectionMatrix = glm::mat4(1.0f);
	viewMatrix = glm::mat4(1.0f);

	moveSpeed = 4.0f;

	aspectRatio = 0.0f;
	nearPlane = 0.0f;
	farPlane = 0.0f;
	fov = 0.0f;
	width = 0.0f;
	height = 0.0f;

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
		position += forward * velocity;
	}
	if (Input::IsKeyPressed(KEY_S))		// s
	{
		position -= forward * velocity;
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
	frustum.UpdateProjection(glm::radians(fov), aspectRatio, near, far);
}

void Camera::SetProjectionMatrix(float left, float right, float bottom, float top, float near, float far)
{
	nearPlane = near;
	farPlane = far;

	projectionMatrix = glm::orthoRH(left, right, bottom, top, near, far);
	frustum.UpdateProjection(left, right, bottom, top, near, far);
}

void Camera::SetViewMatrix(const glm::mat4& view)
{
	viewMatrix = view;
	//frustum.Update(position, position + forward, up);
}

void Camera::SetViewMatrix(const glm::vec3& pos, const glm::vec3& center, const glm::vec3& up)
{
	viewMatrix = glm::lookAt(pos, center, up);
	frustum.Update(position, position + forward, up);
}

void Camera::SetPosition(const glm::vec3& pos)
{
	position = pos;
	viewMatrix = glm::lookAt(position, position + forward, up);
	frustum.Update(position, position + forward, up);
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

	forward = glm::normalize(m[2]);
	right = glm::normalize(glm::cross(forward, glm::vec3(0.0f, 1.0f, 0.0f)));
	up = glm::normalize(glm::cross(right, forward));

	viewMatrix = glm::lookAt(position, position + forward, up);
	frustum.Update(position, position + forward, up);
}
