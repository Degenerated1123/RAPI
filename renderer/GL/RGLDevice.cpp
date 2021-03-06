#include "pch.h"
#include "RGLDevice.h"
#include "Logger.h"
#include <GL/glew.h>
#include <GLFW/glfw3.h>
#include "RBuffer.h"
#include "RPixelShader.h"
#include "RVertexShader.h"
#include "RTexture.h"

#ifdef RND_GL
using namespace RAPI;

bool RGLDevice::CreateDeviceAPI()
{
    return true;
}

bool RGLDevice::SetWindowAPI()
{
	// Make the window's context current
	glfwMakeContextCurrent(OutputWindow);
	CheckGlError();

    // Init first viewport
    RInt2 windowSize = GetWindowResolutionAPI(OutputWindow);
    glViewport(0, 0, windowSize.x, windowSize.y);
	CheckGlError();

    glClearColor(0.0f, 0.5f, 0.5f,0.0f);
	CheckGlError();

	// start GLEW extension handler
	glewExperimental = GL_TRUE;

	GLenum err = glewInit();
	if (GLEW_OK != err)
		LogError() << "Failed to init GLEW";

	CheckGlError();

	// get version info
	const GLubyte* renderer = glGetString (GL_RENDERER); // get renderer string
	const GLubyte* version = glGetString (GL_VERSION); // version as a string
	LogInfo() << "Renderer " << renderer;
	LogInfo() << "OpenGL version supported " << version;

	CheckGlError();

	if(!glewIsSupported("GL_ARB_explicit_uniform_location"))
	{
		LogError() << "GL_ARB_explicit_uniform_location not supported!";
	}


    return true;
}

bool RGLDevice::OnResizeAPI()
{
	glViewport(0, 0, OutputResolution.x, OutputResolution.y);
    return true;
}

bool RGLDevice::OnFrameStartAPI()
{
    glClearColor(0.0f, 0.5f, 1.0f,0.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

	glEnable(GL_DEPTH_TEST);
	glDepthFunc(GL_LESS);
	
	glEnable(GL_CULL_FACE);
	glCullFace(GL_BACK);

	glFrontFace(GL_CCW);
    return true;
}

bool RGLDevice::PrepareFrameAPI()
{
    return false;
}

bool RGLDevice::OnFrameEndAPI()
{
    return true;
}

bool RGLDevice::PresentAPI()
{
	glfwSwapBuffers(OutputWindow);
    return true;
}

/**
* Binds the resources of the given pipeline state
*/
bool RGLDevice::BindPipelineState(const RPipelineState& _state, const RStateMachine::ChangesStruct& _changes, RStateMachine& stateMachine)
{
	RStateMachine::ChangesStruct& changes = (RStateMachine::ChangesStruct&)_changes;
	RPipelineState& state = (RPipelineState&)_state;
	//changes.MainTexture = false;
	//changes.VertexBuffers[0] = false;
	//changes.ConstantBuffers[0] = false;

	stateMachine.SetFromPipelineState(&state, changes);
	const RPipelineStateFull& fs = stateMachine.GetCurrentState();
	std::array<RGLShader*, EShaderType::ST_NUM_SHADER_TYPES> shaders;
	shaders.fill(0);
	
	//if(changes.RasterizerState && fs.RasterizerState)
	//	context->RSSetState(fs.RasterizerState->GetState());

	//if(changes.BlendState && fs.BlendState)
	//	context->OMSetBlendState(fs.BlendState->GetState(), (float *)&RFloat4(0.0f,0.0f,0.0f,0.0f), 0xFFFFFFFF);

	//if(changes.DepthStencilState && fs.DepthStencilState)
	//	context->OMSetDepthStencilState(fs.DepthStencilState->GetState(), 0);

	if(changes.SamplerState && fs.SamplerState)
	{
		// TODO: Create actual sampler state object
		glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT ); 
		glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT );
		glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR );
		glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR ); 

		GLfloat aniso = 0.0f;
		glGetFloatv(GL_MAX_TEXTURE_MAX_ANISOTROPY_EXT, &aniso);
		glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAX_ANISOTROPY_EXT, aniso); 
	}

	
	if(changes.VertexBuffers[0])
	{
		if(fs.VertexBuffers[0])
		{
			// Need to update the VAO of this to get the vertexlayout into the buffer
			GLuint vao = fs.VertexBuffers[0]->GetVertexArrayObjectAPI();

			// Create vao, if needed
			if(!vao)
			{
				fs.VertexBuffers[0]->UpdateVAO(fs.InputLayout, fs.VertexBuffers[1]);
				vao = fs.VertexBuffers[0]->GetVertexArrayObjectAPI();
			}

			glBindVertexArray(fs.VertexBuffers[0]->GetVertexArrayObjectAPI());
			CheckGlError();
		}
	}


	if(changes.IndexBuffer && fs.IndexBuffer)
		glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, fs.IndexBuffer->GetBufferObjectAPI());

	/*if(changes.PixelShader && fs.PixelShader)
		shaders.push_back(fs.PixelShader);

	if(changes.VertexShader && fs.VertexShader)
		shaders.push_back(fs.VertexShader);*/

	if(fs.PixelShader)
		shaders[EShaderType::ST_PIXEL] = fs.PixelShader;

	if(fs.VertexShader)
		shaders[EShaderType::ST_VERTEX] = fs.VertexShader;

	// Link shaders/get a program from cache
	GLuint shaderProgram = 0;
	if(!shaders.empty())
	{
		shaderProgram = shaders[0]->LinkShaderObjectAPI(shaders.data(), EShaderType::ST_NUM_SHADER_TYPES);
		glUseProgram(shaderProgram);
		CheckGlError();
	}

	// TODO: Do this for all shader stages
	// TODO: Structured buffers use the same registers as textures. A change of them will
	// not affect the changed state of the textures.
	if(changes.MainTexture)
	{
		for(unsigned int i = 0; i < fs.Textures[EShaderType::ST_PIXEL].size(); i++)
		{
			GLuint tx = fs.Textures[EShaderType::ST_PIXEL][i] ? 
				fs.Textures[EShaderType::ST_PIXEL][i]->GetTextureObjectAPI()
				: GL_INVALID_INDEX;

			if(tx != GL_INVALID_INDEX)
			{
				glActiveTexture(GL_TEXTURE0 + i);
				glBindTexture(GL_TEXTURE_2D, tx);

				GLuint maxMip = std::max(1u, fs.Textures[EShaderType::ST_PIXEL][i]->GetNumMipLevels()) - 1;
				glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAX_LEVEL, maxMip); 

				glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT ); 
				glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT );
				glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR );
				glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR ); 

				GLfloat aniso = 0.0f;
				glGetFloatv(GL_MAX_TEXTURE_MAX_ANISOTROPY_EXT, &aniso);
				glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAX_ANISOTROPY_EXT, aniso / 2); 

				CheckGlError();
			}
		}
	}


	if(changes.ConstantBuffers[EShaderType::ST_VERTEX])
	{
		for(unsigned int j=0;j<fs.ConstantBuffers[EShaderType::ST_VERTEX].size();j++)
		{
			if(fs.VertexShader && fs.ConstantBuffers[EShaderType::ST_VERTEX][j])
			{
				GLuint ubo = fs.ConstantBuffers[EShaderType::ST_VERTEX][j]->GetBufferObjectAPI();
				glBindBufferBase(GL_UNIFORM_BUFFER, j, ubo);
				
				CheckGlError();
			}
		}
	}

	/*if(changes.ConstantBuffers[EShaderType::ST_PIXEL])
	{
		for(unsigned int j=0;j<fs.ConstantBuffers[EShaderType::ST_PIXEL].size();j++)
		{
			if(fs.PixelShader && fs.ConstantBuffers[EShaderType::ST_PIXEL][j])
			{
				// TODO: Cache the call from LinkShaderObjectAPI above somewhere
				GLuint ubo = fs.ConstantBuffers[EShaderType::ST_PIXEL][j]->GetBufferObjectAPI();
				glBindBuffer(GL_UNIFORM_BUFFER, ubo);
				glBindBufferBase( GL_UNIFORM_BUFFER, j, ubo );
				// TODO: Cache the call from LinkShaderObjectAPI above somewhere
				//glBindBuffer(GL_UNIFORM_BUFFER, fs.ConstantBuffers[EShaderType::ST_PIXEL][j]->GetBufferObjectAPI());
				//glUniformBlockBinding(fs.VertexShader->LinkShaderObjectAPI(shaders), j, 0);
				//CheckGlError();
			}
		}
	}
	*/
	//if(changes.StructuredBuffers[EShaderType::ST_VERTEX])
	//{
	//	for(unsigned int j=0;j<fs.StructuredBuffers[EShaderType::ST_VERTEX].size();j++)
	//	{
	//		if(fs.StructuredBuffers[EShaderType::ST_VERTEX][j])
	//		{
	//			context->VSSetShaderResources(j, 1, fs.StructuredBuffers[EShaderType::ST_VERTEX][j]->GetBufferSRVPtr());
	//		}
	//	}
	//}

	//if(changes.Viewport && fs.Viewport)
	//{
	//	context->RSSetViewports(1, (D3D11_VIEWPORT*)&fs.Viewport->GetViewportInfo());
	//}

	return true;
}

bool RGLDevice::DrawPipelineStateAPI(const struct RPipelineState &state,
                                     const RStateMachine::ChangesStruct &changes, RStateMachine &stateMachine)
{
	// Bind everything
	if(DoDrawcalls)
	{
		BindPipelineState(state, changes, stateMachine);
		CheckGlError();

		// Perform drawcall
		switch(state.IDs.DrawFunctionID)
		{
		case EDrawCallType::DCT_Draw:
			glDrawArrays(state.IDs.PrimitiveType, state.StartVertexOffset, state.NumDrawElements);
			break;

		case EDrawCallType::DCT_DrawIndexed:
			glDrawElements(state.IDs.PrimitiveType, state.NumDrawElements, GL_UNSIGNED_INT, (void*)(state.StartIndexOffset * sizeof(uint32_t))); // TODO: Support GL_UNSIGNED_SHORT
			break;

		case EDrawCallType::DCT_DrawIndexedInstanced:
			glDrawElementsInstancedBaseInstance(state.IDs.PrimitiveType, state.NumDrawElements, GL_UNSIGNED_INT, (void*)(state.StartIndexOffset * sizeof(uint32_t)), state.NumInstances, state.StartInstanceOffset);
			//glDrawElementsInstanced(state.IDs.PrimitiveType, state.NumInstances, GL_UNSIGNED_INT, 0, state.NumDrawElements);
			//context->DrawIndexedInstanced(state.NumDrawElements, state.NumInstances, state.StartIndexOffset, state.StartVertexOffset, state.StartInstanceOffset);
			break;
		}

		CheckGlError();
	}
    return true;
}

bool RGLDevice::DrawPipelineStatesAPI(struct RPipelineState *const *stateArray, unsigned int numStates)
{
    return false;
}

bool RGLDevice::RegisterThreadAPI(uint32_t threadID)
{
    return false;
}

bool RGLDevice::CreateCommandListForThreadAPI(uint32_t threadID)
{
    return false;
}

bool RGLDevice::PrepareContextAPI(unsigned int threadId)
{
    return false;
}

bool RGLDevice::GetDisplayModeListAPI(std::vector<DisplayModeInfo> &modeList, bool includeSuperSampling)
{
    return false;
}

/**
 * Returns the resolution needed for the given window
 */
RInt2 RGLDevice::GetWindowResolutionAPI(WindowHandle hWnd)
{
	RInt2 s;
	glfwGetFramebufferSize(hWnd, &s.x, &s.y);

    return s;
}
#endif