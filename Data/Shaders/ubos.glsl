layout(std140, set = 0, binding = 0) uniform ViewUniforms
{
	mat4 projectionMatrix;
	mat4 viewMatrix;
	mat4 projView;
	mat4 invView;
	mat4 invProj;
	vec4 clipPlane;
	vec4 camPos;				// w not used
	vec2 nearFarPlane;
};

layout(std140, set = 0, binding = 1) readonly buffer InstanceDataBuffer
{
	mat4 instanceData[];
};

layout(std140, set = 0, binding = 2) uniform FrameUniforms
{
	mat4 orthoProjX;
	mat4 orthoProjY;
	mat4 orthoProjZ;
	mat4 previousFrameView;
	mat4 cloudsInvProjJitter;
	mat4 projGridViewFrame;
	vec4 viewCorner0;
	vec4 viewCorner1;
	vec4 viewCorner2;
	vec4 viewCorner3;
	vec4 waterNormalMapOffset;
	float timeElapsed;
	float giIntensity;
	float skyColorMultiplier;
	float aoIntensity;
	float voxelGridSize;
	float voxelScale;
	int enableGI;
	float timeOfDay;
	float bloomIntensity;
	float bloomThreshold;
	vec2 windDirStr;
	vec4 lightShaftsParams;
	vec4 lightShaftsColor;
	vec2 lightScreenPos;
	vec2 fogParams;				// x - fog height, y - fog density
	
	//float bloomRadius;
	//float sampleScale;
	vec4 fogInscatteringColor;
	vec4 lightInscatteringColor;
	
	//vec4
	float cloudCoverage;
	float cloudStartHeight;
	float cloudLayerThickness;
	float cloudLayerTopHeight;
	
	//vec4
	float timeScale;
	float hgForward;
	float densityMult;
	float ambientMult;
	
	//vec4
	float directLightMult;
	float detailScale;
	float highCloudsCoverage;
	float highCloudsTimeScale;
	
	//vec4
	float silverLiningIntensity;
	float silverLiningSpread;
	float forwardSilverLiningIntensity;
	float lightShaftsIntensity;
	
	vec4 ambientTopColor;
	vec4 ambientBottomColor;
	
	vec2 screenRes;
	//vec2 invScreenRes;
	vec2 vignetteParams;				// x -> intensity, y -> falloff
	
	vec4 terrainEditParams; 			// xy -> intersection point, z = 0  editing disabled z = 1 editing enabled, w - brush radius
	vec4 terrainEditParams2;		// x - brushStrength
	
	uint frameNumber;
	uint cloudUpdateBlockSize;
	float deltaTime;
	float padding;
};

layout(std140, set = 0, binding = 3) uniform DirLight
{
	mat4 lightSpaceMatrix[4];
	vec4 dirAndIntensity;			// xyz - direction, w - intensity
	vec4 dirLightColor;				// xyz - color, w - ambient
	vec4 cascadeEnd;
	vec3 skyColor;
};
