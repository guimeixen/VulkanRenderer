#pragma once

#include "glm/glm.hpp"


	struct Plane
	{
		glm::vec3 normal;
		glm::vec3 pointP0;
		float d;

		void Set3Points(const glm::vec3& v1, const glm::vec3& v2, const glm::vec3& v3)
		{
			glm::vec3 aux1, aux2;

			aux1 = v1 - v2;
			aux2 = v3 - v2;

			normal = glm::normalize(glm::cross(aux2, aux1));

			pointP0 = v2;
			d = -(glm::dot(normal, pointP0));
		}

		void SetNormalAndPoint(const glm::vec3& normal, const glm::vec3& pointP0)
		{
			this->normal = glm::normalize(normal);
			this->pointP0 = pointP0;
			d = -(glm::dot(this->normal, pointP0));
		}

		float Distance(glm::vec3 p) const
		{
			return (d + glm::dot(normal, p));
		}

		bool IntersectRay(const glm::vec3& origin, const glm::vec3& direction, float& t)
		{
			float denom = glm::dot(direction, normal);

			if (std::abs(denom) > 0.0001f)
			{
				glm::vec3 p0l0 = pointP0 - origin;
				t = glm::dot(p0l0, normal) / denom;
				return (t >= 0);
			}

			return false;
		}
	};

	enum class FrustumIntersect
	{
		OUTSIDE,
		INTERSECT,
		INSIDE
	};

	enum class FrustumType
	{
		PERSPECTIVE,
		ORTHOGRAPHIC
	};

	struct FrustumCorners
	{
		glm::vec3 ntl;
		glm::vec3 ntr;
		glm::vec3 nbl;
		glm::vec3 nbr;

		glm::vec3 ftl;
		glm::vec3 ftr;
		glm::vec3 fbl;
		glm::vec3 fbr;
	};

	class Frustum
	{
	public:
		void UpdateProjection(float left, float right, float bottom, float top, float near, float far);
		void UpdateProjection(float fov, float aspectRatio, float near, float far);
		void Update(const glm::vec3& pos, const glm::vec3& center, const glm::vec3& up);

		FrustumIntersect SphereInFrustum(const glm::vec3& sphereCenter, float radius) const;
		FrustumIntersect BoxInFrustum(const glm::vec3& min, const glm::vec3& max) const;

		const FrustumCorners& GetCorners() const { return corners; }

		FrustumType GetType() const { return frustumType; }

	private:
		glm::vec3 GetVertexPositive(const glm::vec3& normal, const glm::vec3& min, const glm::vec3& max) const;
		glm::vec3 GetVertexNegative(const glm::vec3& normal, const glm::vec3& min, const glm::vec3& max) const;

	private:
		FrustumType frustumType;
		Plane planes[6];
		float nh, nw, fh, fw;
		float nearP, farP;
		FrustumCorners corners;

		enum FRUSTUM_PLANES
		{
			TOP = 0,
			BOTTOM,
			LEFT,
			RIGHT,
			NEARP,
			FARP
		};
	};
