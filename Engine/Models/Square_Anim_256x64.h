#include "GraphicsTypes.h"

//Model: "Square_Anim_256x64"

//Note: Use for animated characters that have a texture with width = 256, height = 64

static const float g_Square_Anim_256x64_VertexData0[] = {
	/*v:*/-0.5f, -0.25f, 0.0f, /*n:*/0.574267f, 0.583422f, -0.574267f, /*t:*/0.0f,0.0f, 
	/*v:*/-0.5f,  0.25f, 0.0f, /*n:*/0.574267f, 0.583422f, -0.574267f, /*t:*/0.0f,0.25f,
	/*v:*/ 0.5f, -0.25f, 0.0f, /*n:*/0.574267f, 0.583422f, -0.574267f, /*t:*/0.125f,0.0f,  
	/*v:*/ 0.5f,  0.25f, 0.0f, /*n:*/0.574267f, 0.583422f, -0.574267f, /*t:*/0.125f,0.25f,
};

#define g_Square_Anim_256x64_numberOfVertices	4
#define g_Square_Anim_256x64_numberOfPrimitives 1

static PrimitiveData g_Square_Anim_256x64_PrimitiveArray[g_Square_Anim_256x64_numberOfPrimitives]={
	{GL_TRIANGLE_STRIP,(f32*)g_Square_Anim_256x64_VertexData0,NULL,g_Square_Anim_256x64_numberOfVertices,sizeof(g_Square_Anim_256x64_VertexData0),0,0,0},
};

ModelData g_Square_Anim_256x64_modelData = {VertexFormat_VNT,"Square_Anim_256x64",g_Square_Anim_256x64_PrimitiveArray,g_Square_Anim_256x64_numberOfPrimitives,-1};
