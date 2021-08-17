#include "Frustum.h"

void Frustum::UpdateProjection(float left, float right, float bottom, float top, float near, float far)
	{
		frustumType = FrustumType::ORTHOGRAPHIC;

		nearP = near;
		farP = far;

		nh = (top - bottom) * 0.5f;
		fh = nh;
		nw = (right - left) * 0.5f;
		fw = nw;
	}

	void Frustum::UpdateProjection(float fov, float aspectRatio, float near, float far)
	{
		frustumType = FrustumType::PERSPECTIVE;

		nearP = near;
		farP = far;

		float tang = glm::tan(fov * 0.5f);
		nh = near * tang;
		nw = nh * aspectRatio;
		fh = far * tang;
		fw = fh * aspectRatio;
	}

	void Frustum::Update(const glm::vec3& pos, const glm::vec3& center, const glm::vec3& up)
	{
		glm::vec3 nc, fc, X, Y, Z;

		Z = glm::normalize(pos - center);
		X = glm::normalize(glm::cross(up, Z));
		Y = glm::cross(Z, X);

		nc = pos - Z * nearP;
		fc = pos - Z * farP;

		corners.ntl = nc + Y * nh - X * nw;		// near top left
		corners.ntr = nc + Y * nh + X * nw;		// near top right
		corners.nbl = nc - Y * nh - X * nw;		// near bottom left
		corners.nbr = nc - Y * nh + X * nw;		// near bottom right

		corners.ftl = fc + Y * fh - X * fw;		// far top left
		corners.ftr = fc + Y * fh + X * fw;		// far top right
		corners.fbl = fc - Y * fh - X * fw;		// far bottom left
		corners.fbr = fc - Y * fh + X * fw;		// far bottom right

		planes[TOP].Set3Points(corners.ntr, corners.ntl, corners.ftl);
		planes[BOTTOM].Set3Points(corners.nbl, corners.nbr, corners.fbr);
		planes[LEFT].Set3Points(corners.ntl, corners.nbl, corners.fbl);
		planes[RIGHT].Set3Points(corners.nbr, corners.ntr, corners.fbr);
		planes[NEARP].Set3Points(corners.ntl, corners.ntr, corners.nbr);
		planes[FARP].Set3Points(corners.ftr, corners.ftl, corners.fbl);
	}

	FrustumIntersect Frustum::SphereInFrustum(const glm::vec3& sphereCenter, float radius) const
	{
		FrustumIntersect result = FrustumIntersect::INSIDE;
		float distance;

		for (int i = 0; i < 6; i++)
		{
			distance = planes[i].Distance(sphereCenter);
			if (distance < -radius)
				return FrustumIntersect::OUTSIDE;
			else if (distance < radius)
				result = FrustumIntersect::INTERSECT;
		}
		return result;
	}

	FrustumIntersect Frustum::BoxInFrustum(const glm::vec3& min, const glm::vec3& max) const
	{
		FrustumIntersect result = FrustumIntersect::INSIDE;
		for (int i = 0; i < 6; i++)
		{
			if (planes[i].Distance(GetVertexPositive(planes[i].normal, min, max)) < 0.0f)
				return FrustumIntersect::OUTSIDE;
			else if (planes[i].Distance(GetVertexNegative(planes[i].normal, min, max)) < 0.0f)
				result = FrustumIntersect::INTERSECT;
		}
		return result;
	}

	glm::vec3 Frustum::GetVertexPositive(const glm::vec3& normal, const glm::vec3& min, const glm::vec3& max) const
	{
		glm::vec3 minn(min.x, min.y, min.z);

		if (normal.x >= 0)
			minn.x = max.x;
		if (normal.y >= 0)
			minn.y = max.y;
		if (normal.z >= 0)
			minn.z = max.z;

		return minn;
	}

	glm::vec3 Frustum::GetVertexNegative(const glm::vec3& normal, const glm::vec3& min, const glm::vec3& max) const
	{
		glm::vec3 maxx(max.x, max.y, max.z);

		if (normal.x <= 0)
			maxx.x = min.x;
		if (normal.y <= 0)
			maxx.y = min.y;
		if (normal.z <= 0)
			maxx.z = min.z;

		return maxx;
	}
