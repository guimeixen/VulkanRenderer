mat4 GetModelMatrix(uint startIndex)
{
	return instanceData[startIndex + gl_InstanceIndex];
}