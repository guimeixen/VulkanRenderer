#pragma once

#include "glm/glm.hpp"

class Camera
{
public:
	Camera();

	void Update(float deltaTime, bool doMovement, bool needRightMouse);

	void SetProjectionMatrix(float fov, int windowWidth, int windowHeight, float near, float far);
	void SetProjectionMatrix(float left, float right, float bottom, float top, float near, float far);
	void SetViewMatrix(const glm::mat4& view);
	void SetViewMatrix(const glm::vec3& pos, const glm::vec3& center, const glm::vec3& up);
	void SetPosition(const glm::vec3& pos);
	void SetPitch(float pitch);
	void SetYaw(float yaw);
	void SetMoveSpeed(float speed) { moveSpeed = speed; }

	const glm::mat4& GetProjectionMatrix() const { return projectionMatrix; }
	const glm::mat4& GetViewMatrix() const { return viewMatrix; }

private:
	void DoMovement(float dt);
	void DoLook();
	void UpdateCameraVectors();

	void Reset() { firstMove = true; }

private:
	bool firstMove;

	glm::vec3 position;
	glm::vec2 lastMousePos;
	glm::mat4 viewMatrix;
	glm::mat4 projectionMatrix;

	glm::vec3 front;
	glm::vec3 up;
	glm::vec3 right;
	glm::vec3 worldUp;

	float moveSpeed;

	float nearPlane;
	float farPlane;
	float aspectRatio;
	float fov;

	int width;
	int height;

	float yaw;
	float pitch;

	float sensitivity;
};

