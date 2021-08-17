#pragma once

#include "Frustum.h"

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

	const Frustum& GetFrustum() const { return frustum; }
	const glm::mat4& GetProjectionMatrix() const { return projectionMatrix; }
	const glm::mat4& GetViewMatrix() const { return viewMatrix; }
	const glm::vec3& GetPosition() const { return position; }
	const glm::vec3& GetForward() const { return forward; }
	const glm::vec3& GetRight() const { return right; }
	const glm::vec3& GetUp() const { return up; }
	float GetNearPlane() const { return nearPlane; }
	float GetFarPlane() const { return farPlane; }

private:
	void DoMovement(float dt);
	void DoLook();
	void UpdateCameraVectors();

	void Reset() { firstMove = true; }

private:
	Frustum frustum;
	bool firstMove;

	glm::vec3 position;
	glm::vec2 lastMousePos;
	glm::mat4 viewMatrix;
	glm::mat4 projectionMatrix;

	glm::vec3 forward;
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

