#pragma once
#include "RGLShader.h"

#ifdef RND_GL
namespace RAPI
{
	class RGLPixelShader : public RGLShader
	{
	public:
		/**
		* Loads the given shader
		*/
		bool LoadShaderAPI();
	};

}
#endif