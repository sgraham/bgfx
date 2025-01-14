/*
 * Copyright 2014 Dario Manesku. All rights reserved.
 * License: http://www.opensource.org/licenses/BSD-2-Clause
 */

#include <string>
#include <vector>

#include "common.h"

#include <bgfx.h>
#include <bx/timer.h>
#include <bx/readerwriter.h>
#include "entry/entry.h"
#include "fpumath.h"
#include "imgui/imgui.h"

#include <stdio.h>
#include <string.h>

static const char* s_shaderPath = NULL;
static bool s_flipV = false;
static float s_texelHalf = 0.0f;

static void shaderFilePath(char* _out, const char* _name)
{
	strcpy(_out, s_shaderPath);
	strcat(_out, _name);
	strcat(_out, ".bin");
}

long int fsize(FILE* _file)
{
	long int pos = ftell(_file);
	fseek(_file, 0L, SEEK_END);
	long int size = ftell(_file);
	fseek(_file, pos, SEEK_SET);
	return size;
}

static const bgfx::Memory* load(const char* _filePath)
{
	FILE* file = fopen(_filePath, "rb");
	if (NULL != file)
	{
		uint32_t size = (uint32_t)fsize(file);
		const bgfx::Memory* mem = bgfx::alloc(size+1);
		size_t ignore = fread(mem->data, 1, size, file);
		BX_UNUSED(ignore);
		fclose(file);
		mem->data[mem->size-1] = '\0';
		return mem;
	}

	return NULL;
}

static const bgfx::Memory* loadShader(const char* _name)
{
	char filePath[512];
	shaderFilePath(filePath, _name);
	return load(filePath);
}

static bgfx::ProgramHandle loadProgram(const char* _vsName, const char* _fsName)
{
	const bgfx::Memory* mem;

	mem = loadShader(_vsName);
	bgfx::ShaderHandle vs = bgfx::createShader(mem);
	mem = loadShader(_fsName);
	bgfx::ShaderHandle fs = bgfx::createShader(mem);
	bgfx::ProgramHandle program = bgfx::createProgram(vs, fs);
	bgfx::destroyShader(vs);
	bgfx::destroyShader(fs);

	return program;
}

static const bgfx::Memory* loadTexture(const char* _name)
{
	char filePath[512];
	strcpy(filePath, "textures/");
	strcat(filePath, _name);
	return load(filePath);
}

struct Uniforms
{
	void init()
	{
		m_time = 0.0f;
		mtxIdentity(m_mtx);

		u_time    = bgfx::createUniform("u_time",     bgfx::UniformType::Uniform1f);
		u_mtx     = bgfx::createUniform("u_mtx",      bgfx::UniformType::Uniform4x4fv);
		u_params  = bgfx::createUniform("u_params",  bgfx::UniformType::Uniform4fv);
		u_flags   = bgfx::createUniform("u_flags",    bgfx::UniformType::Uniform4fv);
		u_camPos  = bgfx::createUniform("u_camPos",   bgfx::UniformType::Uniform3fv);
		u_rgbDiff = bgfx::createUniform("u_rgbDiff",  bgfx::UniformType::Uniform3fv);
		u_rgbSpec = bgfx::createUniform("u_rgbSpec",  bgfx::UniformType::Uniform3fv);
	}

	// Call this once at initialization.
	void submitConstUniforms()
	{
	}

	// Call this once per frame.
	void submitPerFrameUniforms()
	{
		bgfx::setUniform(u_time, &m_time);
		bgfx::setUniform(u_mtx, m_mtx);
		bgfx::setUniform(u_flags, m_flags);
		bgfx::setUniform(u_camPos, m_camPos);
		bgfx::setUniform(u_rgbDiff, m_rgbDiff);
		bgfx::setUniform(u_rgbSpec, m_rgbSpec);
	}

	// Call this before each draw call.
	void submitPerDrawUniforms()
	{
		bgfx::setUniform(u_params, m_params);
	}

	void destroy()
	{
		bgfx::destroyUniform(u_rgbSpec);
		bgfx::destroyUniform(u_rgbDiff);
		bgfx::destroyUniform(u_camPos);
		bgfx::destroyUniform(u_flags);
		bgfx::destroyUniform(u_params);
		bgfx::destroyUniform(u_mtx);
		bgfx::destroyUniform(u_time);
	}

	union
	{
		struct
		{
			float m_glossiness;
			float m_exposure;
			float m_diffspec;
			float m_unused0;
		};

		float m_params[4];
	};

	union
	{
		struct
		{
			float m_diffuse;
			float m_specular;
			float m_diffuseIbl;
			float m_specularIbl;
		};

		float m_flags[4];
	};

	float m_time;
	float m_mtx[16];
	float m_camPos[3];
	float m_rgbDiff[3];
	float m_rgbSpec[3];

	bgfx::UniformHandle u_time;
	bgfx::UniformHandle u_mtx;
	bgfx::UniformHandle u_params;
	bgfx::UniformHandle u_flags;
	bgfx::UniformHandle u_camPos;
	bgfx::UniformHandle u_rgbDiff;
	bgfx::UniformHandle u_rgbSpec;
};
static Uniforms s_uniforms;

struct Aabb
{
	float m_min[3];
	float m_max[3];
};

struct Obb
{
	float m_mtx[16];
};

struct Sphere
{
	float m_center[3];
	float m_radius;
};

struct Primitive
{
	uint32_t m_startIndex;
	uint32_t m_numIndices;
	uint32_t m_startVertex;
	uint32_t m_numVertices;

	Sphere m_sphere;
	Aabb m_aabb;
	Obb m_obb;
};

typedef std::vector<Primitive> PrimitiveArray;

struct Group
{
	Group()
	{
		reset();
	}

	void reset()
	{
		m_vbh.idx = bgfx::invalidHandle;
		m_ibh.idx = bgfx::invalidHandle;
		m_prims.clear();
	}

	bgfx::VertexBufferHandle m_vbh;
	bgfx::IndexBufferHandle m_ibh;
	Sphere m_sphere;
	Aabb m_aabb;
	Obb m_obb;
	PrimitiveArray m_prims;
};

struct Mesh
{
	void load(const char* _filePath)
	{
#define BGFX_CHUNK_MAGIC_VB BX_MAKEFOURCC('V', 'B', ' ', 0x0)
#define BGFX_CHUNK_MAGIC_IB BX_MAKEFOURCC('I', 'B', ' ', 0x0)
#define BGFX_CHUNK_MAGIC_PRI BX_MAKEFOURCC('P', 'R', 'I', 0x0)

		bx::CrtFileReader reader;
		reader.open(_filePath);

		Group group;

		uint32_t chunk;
		while (4 == bx::read(&reader, chunk) )
		{
			switch (chunk)
			{
			case BGFX_CHUNK_MAGIC_VB:
				{
					bx::read(&reader, group.m_sphere);
					bx::read(&reader, group.m_aabb);
					bx::read(&reader, group.m_obb);

					bx::read(&reader, m_decl);
					uint16_t stride = m_decl.getStride();

					uint16_t numVertices;
					bx::read(&reader, numVertices);
					const bgfx::Memory* mem = bgfx::alloc(numVertices*stride);
					bx::read(&reader, mem->data, mem->size);

					group.m_vbh = bgfx::createVertexBuffer(mem, m_decl);
				}
				break;

			case BGFX_CHUNK_MAGIC_IB:
				{
					uint32_t numIndices;
					bx::read(&reader, numIndices);
					const bgfx::Memory* mem = bgfx::alloc(numIndices*2);
					bx::read(&reader, mem->data, mem->size);
					group.m_ibh = bgfx::createIndexBuffer(mem);
				}
				break;

			case BGFX_CHUNK_MAGIC_PRI:
				{
					uint16_t len;
					bx::read(&reader, len);

					std::string material;
					material.resize(len);
					bx::read(&reader, const_cast<char*>(material.c_str() ), len);

					uint16_t num;
					bx::read(&reader, num);

					for (uint32_t ii = 0; ii < num; ++ii)
					{
						bx::read(&reader, len);

						std::string name;
						name.resize(len);
						bx::read(&reader, const_cast<char*>(name.c_str() ), len);

						Primitive prim;
						bx::read(&reader, prim.m_startIndex);
						bx::read(&reader, prim.m_numIndices);
						bx::read(&reader, prim.m_startVertex);
						bx::read(&reader, prim.m_numVertices);
						bx::read(&reader, prim.m_sphere);
						bx::read(&reader, prim.m_aabb);
						bx::read(&reader, prim.m_obb);

						group.m_prims.push_back(prim);
					}

					m_groups.push_back(group);
					group.reset();
				}
				break;

			default:
				DBG("%08x at %d", chunk, reader.seek() );
				break;
			}
		}

		reader.close();
	}

	void unload()
	{
		for (GroupArray::const_iterator it = m_groups.begin(), itEnd = m_groups.end(); it != itEnd; ++it)
		{
			const Group& group = *it;
			bgfx::destroyVertexBuffer(group.m_vbh);

			if (bgfx::isValid(group.m_ibh) )
			{
				bgfx::destroyIndexBuffer(group.m_ibh);
			}
		}
		m_groups.clear();
	}

	void submit(uint8_t _view, bgfx::ProgramHandle _program, float* _mtx)
	{
		for (GroupArray::const_iterator it = m_groups.begin(), itEnd = m_groups.end(); it != itEnd; ++it)
		{
			const Group& group = *it;

			// Set uniforms.
			s_uniforms.submitPerDrawUniforms();

			// Set model matrix for rendering.
			bgfx::setTransform(_mtx);
			bgfx::setProgram(_program);
			bgfx::setIndexBuffer(group.m_ibh);
			bgfx::setVertexBuffer(group.m_vbh);

			// Set render states.
			bgfx::setState(0
				| BGFX_STATE_RGB_WRITE
				| BGFX_STATE_ALPHA_WRITE
				| BGFX_STATE_DEPTH_WRITE
				| BGFX_STATE_DEPTH_TEST_LESS
				| BGFX_STATE_CULL_CCW
				| BGFX_STATE_MSAA
				);

			// Submit primitive for rendering to view 0.
			bgfx::submit(_view);
		}
	}

	bgfx::VertexDecl m_decl;
	typedef std::vector<Group> GroupArray;
	GroupArray m_groups;
};

struct PosColorTexCoord0Vertex
{
	float m_x;
	float m_y;
	float m_z;
	uint32_t m_rgba;
	float m_u;
	float m_v;

	static void init()
	{
		ms_decl.begin();
		ms_decl.add(bgfx::Attrib::Position, 3, bgfx::AttribType::Float);
		ms_decl.add(bgfx::Attrib::Color0, 4, bgfx::AttribType::Uint8, true);
		ms_decl.add(bgfx::Attrib::TexCoord0, 2, bgfx::AttribType::Float);
		ms_decl.end();
	}

	static bgfx::VertexDecl ms_decl;
};

bgfx::VertexDecl PosColorTexCoord0Vertex::ms_decl;

void screenSpaceQuad(float _textureWidth, float _textureHeight, bool _originBottomLeft = false, float _width = 1.0f, float _height = 1.0f)
{
	if (bgfx::checkAvailTransientVertexBuffer(3, PosColorTexCoord0Vertex::ms_decl) )
	{
		bgfx::TransientVertexBuffer vb;
		bgfx::allocTransientVertexBuffer(&vb, 3, PosColorTexCoord0Vertex::ms_decl);
		PosColorTexCoord0Vertex* vertex = (PosColorTexCoord0Vertex*)vb.data;

		const float zz = 0.0f;

		const float minx = -_width;
		const float maxx =  _width;
		const float miny = 0.0f;
		const float maxy = _height*2.0f;

		const float texelHalfW = s_texelHalf/_textureWidth;
		const float texelHalfH = s_texelHalf/_textureHeight;
		const float minu = -1.0f + texelHalfW;
		const float maxu =  1.0f + texelHalfW;

		float minv = texelHalfH;
		float maxv = 2.0f + texelHalfH;

		if (_originBottomLeft)
		{
			std::swap(minv, maxv);
			minv -= 1.0f;
			maxv -= 1.0f;
		}

		vertex[0].m_x = minx;
		vertex[0].m_y = miny;
		vertex[0].m_z = zz;
		vertex[0].m_rgba = 0xffffffff;
		vertex[0].m_u = minu;
		vertex[0].m_v = minv;

		vertex[1].m_x = maxx;
		vertex[1].m_y = miny;
		vertex[1].m_z = zz;
		vertex[1].m_rgba = 0xffffffff;
		vertex[1].m_u = maxu;
		vertex[1].m_v = minv;

		vertex[2].m_x = maxx;
		vertex[2].m_y = maxy;
		vertex[2].m_z = zz;
		vertex[2].m_rgba = 0xffffffff;
		vertex[2].m_u = maxu;
		vertex[2].m_v = maxv;

		bgfx::setVertexBuffer(&vb);
	}
}

void mtxScaleRotateTranslate(float* _result
							, const float _scaleX
							, const float _scaleY
							, const float _scaleZ
							, const float _rotX
							, const float _rotY
							, const float _rotZ
							, const float _translateX
							, const float _translateY
							, const float _translateZ
							)
{
	float mtxRotateTranslate[16];
	float mtxScale[16];

	mtxRotateXYZ(mtxRotateTranslate, _rotX, _rotY, _rotZ);
	mtxRotateTranslate[12] = _translateX;
	mtxRotateTranslate[13] = _translateY;
	mtxRotateTranslate[14] = _translateZ;

	memset(mtxScale, 0, sizeof(float)*16);
	mtxScale[0]  = _scaleX;
	mtxScale[5]  = _scaleY;
	mtxScale[10] = _scaleZ;
	mtxScale[15] = 1.0f;

	mtxMul(_result, mtxScale, mtxRotateTranslate);
}

void imguiBool(const char* _str, bool& _flag, bool _enabled = true)
{
	if (imguiCheck(_str, _flag, _enabled) )
	{
		_flag = !_flag;
	}
}

int _main_(int /*_argc*/, char** /*_argv*/)
{
	uint32_t width = 1280;
	uint32_t height = 720;
	uint32_t debug = BGFX_DEBUG_TEXT;
	uint32_t reset = BGFX_RESET_VSYNC;

	bgfx::init();
	bgfx::reset(width, height, reset);

	// Enable debug text.
	bgfx::setDebug(debug);

	// Set views  clear state.
	bgfx::setViewClear(0
		, BGFX_CLEAR_COLOR_BIT|BGFX_CLEAR_DEPTH_BIT
		, 0x303030ff
		, 1.0f
		, 0
		);

	// Setup root path for binary shaders. Shader binaries are different
	// for each renderer.
	switch (bgfx::getRendererType() )
	{
	default:
	case bgfx::RendererType::Direct3D9:
		s_shaderPath = "shaders/dx9/";
		break;

	case bgfx::RendererType::Direct3D11:
		s_shaderPath = "shaders/dx11/";
		break;

	case bgfx::RendererType::OpenGL:
		s_shaderPath = "shaders/glsl/";
		s_flipV = true;
		break;

	case bgfx::RendererType::OpenGLES:
		s_shaderPath = "shaders/gles/";
		s_flipV = true;
		break;
	}

	// Imgui.
	FILE* file = fopen("font/droidsans.ttf", "rb");
	uint32_t size = (uint32_t)fsize(file);
	void* data = malloc(size);
	size_t ignore = fread(data, 1, size, file);
	BX_UNUSED(ignore);
	fclose(file);
	imguiCreate(data, size);

	// Uniforms.
	s_uniforms.init();

	// Vertex declarations.
	PosColorTexCoord0Vertex::init();

	struct LightProbe
	{
		void load(const char* _name)
		{
			const uint32_t texFlags = BGFX_TEXTURE_U_CLAMP|BGFX_TEXTURE_V_CLAMP|BGFX_TEXTURE_W_CLAMP;
			char filePath[512];

			strcpy(filePath, _name);
			strcat(filePath, "_lod.dds");
			m_tex = bgfx::createTexture(loadTexture(filePath), texFlags);

			strcpy(filePath, _name);
			strcat(filePath, "_irr.dds");
			m_texIrr = bgfx::createTexture(loadTexture(filePath), texFlags);
		}

		void destroy()
		{
			bgfx::destroyTexture(m_tex);
			bgfx::destroyTexture(m_texIrr);
		}

		bgfx::TextureHandle m_tex;
		bgfx::TextureHandle m_texIrr;
	};

	enum LightProbes
	{
		LPWells,
		LPUffizi,
		LPPisa,
		LPEnnis,
		LPGrace,

		LPCount
	};

	LightProbe lightProbes[LPCount];
	lightProbes[LPWells].load("wells");
	lightProbes[LPUffizi].load("uffizi");
	lightProbes[LPPisa].load("pisa");
	lightProbes[LPEnnis].load("ennis");
	lightProbes[LPGrace].load("grace");
	uint8_t currentLightProbe = LPWells;

	bgfx::UniformHandle u_time   = bgfx::createUniform("u_time",   bgfx::UniformType::Uniform1f);
	bgfx::UniformHandle u_mtx    = bgfx::createUniform("u_mtx",    bgfx::UniformType::Uniform4x4fv);
	bgfx::UniformHandle u_params = bgfx::createUniform("u_params", bgfx::UniformType::Uniform4fv);
	bgfx::UniformHandle u_flags  = bgfx::createUniform("u_flags",  bgfx::UniformType::Uniform4fv);
	bgfx::UniformHandle u_camPos = bgfx::createUniform("u_camPos", bgfx::UniformType::Uniform3fv);

	bgfx::UniformHandle u_texCube    = bgfx::createUniform("u_texCube",    bgfx::UniformType::Uniform1i);
	bgfx::UniformHandle u_texCubeIrr = bgfx::createUniform("u_texCubeIrr", bgfx::UniformType::Uniform1i);

	bgfx::UniformHandle u_texAlbedo    = bgfx::createUniform("u_texAlbedo",    bgfx::UniformType::Uniform1i);
	bgfx::UniformHandle u_texNormal    = bgfx::createUniform("u_texNormal",    bgfx::UniformType::Uniform1i);
	bgfx::UniformHandle u_texSpecular  = bgfx::createUniform("u_texSpecular",  bgfx::UniformType::Uniform1i);
	bgfx::UniformHandle u_texRoughness = bgfx::createUniform("u_texRoughness", bgfx::UniformType::Uniform1i);

	bgfx::ProgramHandle programMesh = loadProgram("vs_ibl_mesh",   "fs_ibl_mesh");
	bgfx::ProgramHandle programSky  = loadProgram("vs_ibl_skybox", "fs_ibl_skybox");

	Mesh meshBunny;
	meshBunny.load("meshes/bunny.bin");

	struct Settings
	{
		float m_speed;
		float m_glossiness;
		float m_exposure;
		float m_diffspec;
		float m_rgbDiff[3];
		float m_rgbSpec[3];
		bool m_diffuse;
		bool m_specular;
		bool m_diffuseIbl;
		bool m_specularIbl;
		bool m_singleSliderDiff;
		bool m_singleSliderSpec;
	};

	Settings settings;
	settings.m_speed = 0.37f;
	settings.m_glossiness = 1.0f;
	settings.m_exposure = 0.0f;
	settings.m_diffspec = 0.65f;
	settings.m_rgbDiff[0] = 0.2f;
	settings.m_rgbDiff[1] = 0.2f;
	settings.m_rgbDiff[2] = 0.2f;
	settings.m_rgbSpec[0] = 1.0f;
	settings.m_rgbSpec[1] = 1.0f;
	settings.m_rgbSpec[2] = 1.0f;
	settings.m_diffuse = true;
	settings.m_specular = true;
	settings.m_diffuseIbl = true;
	settings.m_specularIbl = true;
	settings.m_singleSliderDiff = false;
	settings.m_singleSliderSpec = false;

	float time = 0.0f;

	s_uniforms.submitConstUniforms();

	entry::MouseState mouseState;
	while (!entry::processEvents(width, height, debug, reset, &mouseState) )
	{
		imguiBeginFrame(mouseState.m_mx
			, mouseState.m_my
			, (mouseState.m_buttons[entry::MouseButton::Left  ] ? IMGUI_MBUT_LEFT  : 0)
			| (mouseState.m_buttons[entry::MouseButton::Right ] ? IMGUI_MBUT_RIGHT : 0)
			, 0
			, width
			, height
			);

		static int32_t rightScrollArea = 0;
		imguiBeginScrollArea("Settings", width - 256 - 10, 10, 256, 426, &rightScrollArea);

		imguiLabel("Shade:");
		imguiSeparator();
		imguiBool("Diffuse",      settings.m_diffuse);
		imguiBool("Specular",     settings.m_specular);
		imguiBool("IBL Diffuse",  settings.m_diffuseIbl);
		imguiBool("IBL Specular", settings.m_specularIbl);

		imguiSeparatorLine();
		imguiSlider("Speed", &settings.m_speed, 0.0f, 1.0f, 0.01f);

		imguiSeparatorLine();
		imguiLabel("Environment:");
		currentLightProbe = imguiChoose(currentLightProbe
									   , "Wells"
									   , "Uffizi"
									   , "Pisa"
									   , "Ennis"
									   , "Grace"
									   );

		imguiSeparator();
		imguiSlider("Exposure", &settings.m_exposure, -8.0f, 8.0f, 0.01f);
		imguiEndScrollArea();

		static int32_t leftScrollArea = 0;
		imguiBeginScrollArea("Settings", 10, 70, 256, 576, &leftScrollArea);

		imguiLabel("Material properties:");
		imguiSeparator();
		imguiSlider("Diffuse - Specular", &settings.m_diffspec,   0.0f, 1.0f, 0.01f);
		imguiSlider("Glossiness"        , &settings.m_glossiness, 0.0f, 1.0f, 0.01f);

		imguiSeparatorLine();
		imguiLabel("Diffuse color:");
		imguiSeparator();
		imguiBool("Single slider", settings.m_singleSliderDiff);
		if (settings.m_singleSliderDiff)
		{
			imguiSlider("RGB:", &settings.m_rgbDiff[0], 0.0f, 1.0f, 0.01f);
			settings.m_rgbDiff[1] = settings.m_rgbDiff[0];
			settings.m_rgbDiff[2] = settings.m_rgbDiff[0];

		}
		else
		{
			imguiSlider("R:", &settings.m_rgbDiff[0], 0.0f, 1.0f, 0.01f);
			imguiSlider("G:", &settings.m_rgbDiff[1], 0.0f, 1.0f, 0.01f);
			imguiSlider("B:", &settings.m_rgbDiff[2], 0.0f, 1.0f, 0.01f);
		}

		imguiSeparatorLine();
		imguiLabel("Specular color:");
		imguiSeparator();
		imguiBool("Single slider", settings.m_singleSliderSpec);
		if (settings.m_singleSliderSpec)
		{
			imguiSlider("RGB:", &settings.m_rgbSpec[0], 0.0f, 1.0f, 0.01f);
			settings.m_rgbSpec[1] = settings.m_rgbSpec[0];
			settings.m_rgbSpec[2] = settings.m_rgbSpec[0];

		}
		else
		{
			imguiSlider("R:", &settings.m_rgbSpec[0], 0.0f, 1.0f, 0.01f);
			imguiSlider("G:", &settings.m_rgbSpec[1], 0.0f, 1.0f, 0.01f);
			imguiSlider("B:", &settings.m_rgbSpec[2], 0.0f, 1.0f, 0.01f);
		}

		imguiSeparatorLine();
		imguiLabel("Predefined materials:");
		imguiSeparator();

		if (imguiButton("Gold") )
		{
			settings.m_glossiness = 0.8f;
			settings.m_diffspec   = 1.0f;

			settings.m_rgbDiff[0] = 0.0f;
			settings.m_rgbDiff[1] = 0.0f;
			settings.m_rgbDiff[2] = 0.0f;

			settings.m_rgbSpec[0] = 1.0f;
			settings.m_rgbSpec[1] = 0.86f;
			settings.m_rgbSpec[2] = 0.58f;

			settings.m_singleSliderSpec = false;
		}

		if (imguiButton("Copper") )
		{
			settings.m_glossiness = 0.67f;
			settings.m_diffspec   = 1.0f;

			settings.m_rgbDiff[0] = 0.0f;
			settings.m_rgbDiff[1] = 0.0f;
			settings.m_rgbDiff[2] = 0.0f;

			settings.m_rgbSpec[0] = 0.98f;
			settings.m_rgbSpec[1] = 0.82f;
			settings.m_rgbSpec[2] = 0.76f;

			settings.m_singleSliderSpec = false;
		}

		if (imguiButton("Titanium") )
		{
			settings.m_glossiness = 0.57f;
			settings.m_diffspec   = 1.0f;

			settings.m_rgbDiff[0] = 0.0f;
			settings.m_rgbDiff[1] = 0.0f;
			settings.m_rgbDiff[2] = 0.0f;

			settings.m_rgbSpec[0] = 0.76f;
			settings.m_rgbSpec[1] = 0.73f;
			settings.m_rgbSpec[2] = 0.71f;

			settings.m_singleSliderSpec = false;
		}

		if (imguiButton("Steel") )
		{
			settings.m_glossiness = 0.82f;
			settings.m_diffspec   = 1.0f;

			settings.m_rgbDiff[0] = 0.0f;
			settings.m_rgbDiff[1] = 0.0f;
			settings.m_rgbDiff[2] = 0.0f;

			settings.m_rgbSpec[0] = 0.77f;
			settings.m_rgbSpec[1] = 0.78f;
			settings.m_rgbSpec[2] = 0.77f;

			settings.m_singleSliderSpec = false;
		}

		imguiEndScrollArea();
		imguiEndFrame();

		s_uniforms.m_glossiness = settings.m_glossiness;
		s_uniforms.m_exposure = settings.m_exposure;
		s_uniforms.m_diffspec = settings.m_diffspec;
		s_uniforms.m_flags[0] = float(settings.m_diffuse);
		s_uniforms.m_flags[1] = float(settings.m_specular);
		s_uniforms.m_flags[2] = float(settings.m_diffuseIbl);
		s_uniforms.m_flags[3] = float(settings.m_specularIbl);
		memcpy(s_uniforms.m_rgbDiff, settings.m_rgbDiff, 3*sizeof(float));
		memcpy(s_uniforms.m_rgbSpec, settings.m_rgbSpec, 3*sizeof(float));

		s_uniforms.submitPerFrameUniforms();

		int64_t now = bx::getHPCounter();
		static int64_t last = now;
		const int64_t frameTime = now - last;
		last = now;
		const double freq = double(bx::getHPFrequency() );
		const double toMs = 1000.0/freq;

		time += (float)(frameTime*settings.m_speed/freq);
		s_uniforms.m_time = time;

		// Use debug font to print information about this example.
		bgfx::dbgTextClear();
		bgfx::dbgTextPrintf(0, 1, 0x4f, "bgfx/examples/18-ibl");
		bgfx::dbgTextPrintf(0, 2, 0x6f, "Description: Image based lightning.");
		bgfx::dbgTextPrintf(0, 3, 0x0f, "Frame: % 7.3f[ms]", double(frameTime)*toMs);

		float at[3] = { 0.0f, 0.0f, 0.0f };
		float eye[3] = { 0.0f, 0.0f, -3.0f };

		mtxRotateXY(s_uniforms.m_mtx
			, 0.0f
			, time
			);

		float view[16];
		float proj[16];

		mtxIdentity(view);
		mtxOrtho(proj, 0.0f, 1.0f, 1.0f, 0.0f, 0.0f, 100.0f);
		bgfx::setViewTransform(0, view, proj);

		mtxLookAt(view, eye, at);
		memcpy(s_uniforms.m_camPos, eye, 3*sizeof(float));
		mtxProj(proj, 60.0f, float(width)/float(height), 0.1f, 100.0f);
		bgfx::setViewTransform(1, view, proj);

		bgfx::setViewRect(0, 0, 0, width, height);
		bgfx::setViewRect(1, 0, 0, width, height);

		// View 0.
		bgfx::setTexture(4, u_texCube, lightProbes[currentLightProbe].m_tex);
		bgfx::setProgram(programSky);
		bgfx::setState(BGFX_STATE_RGB_WRITE|BGFX_STATE_ALPHA_WRITE);
		screenSpaceQuad( (float)width, (float)height, true);
		bgfx::submit(0);

		// View 1.
		float mtx[16];
		mtxScaleRotateTranslate(mtx
				, 1.0f
				, 1.0f
				, 1.0f
				, 0.0f
				, (float(M_PI))+time
				, 0.0f
				, 0.0f
				, -1.0f
				, 0.0f
				);

		bgfx::setTexture(4, u_texCube,    lightProbes[currentLightProbe].m_tex);
		bgfx::setTexture(5, u_texCubeIrr, lightProbes[currentLightProbe].m_texIrr);
		meshBunny.submit(1, programMesh, mtx);

		// Advance to next frame. Rendering thread will be kicked to
		// process submitted rendering primitives.
		bgfx::frame();
	}

	meshBunny.unload();

	// Cleanup.
	bgfx::destroyProgram(programMesh);
	bgfx::destroyProgram(programSky);

	bgfx::destroyUniform(u_camPos);
	bgfx::destroyUniform(u_flags);
	bgfx::destroyUniform(u_params);
	bgfx::destroyUniform(u_mtx);
	bgfx::destroyUniform(u_time);

	bgfx::destroyUniform(u_texRoughness);
	bgfx::destroyUniform(u_texSpecular);
	bgfx::destroyUniform(u_texNormal);
	bgfx::destroyUniform(u_texAlbedo);

	bgfx::destroyUniform(u_texCube);
	bgfx::destroyUniform(u_texCubeIrr);

	for (uint8_t ii = 0; ii < LPCount; ++ii)
	{
		lightProbes[ii].destroy();
	}

	s_uniforms.destroy();

	imguiDestroy();

	// Shutdown bgfx.
	bgfx::shutdown();

	return 0;
}
