/*
 * Copyright 2011-2014 Branimir Karadzic. All rights reserved.
 * License: http://www.opensource.org/licenses/BSD-2-Clause
 */

#include "bgfx_p.h"

#if (BGFX_CONFIG_RENDERER_OPENGLES|BGFX_CONFIG_RENDERER_OPENGL)
#	include "renderer_gl.h"
#	include <bx/timer.h>
#	include <bx/uint32_t.h>

namespace bgfx
{
	static char s_viewName[BGFX_CONFIG_MAX_VIEWS][256];

	static const GLenum s_primType[] =
	{
		GL_TRIANGLES,
		GL_LINES,
		GL_POINTS,
	};

	static const char* s_attribName[Attrib::Count] =
	{
		"a_position",
		"a_normal",
		"a_tangent",
		"a_color0",
		"a_color1",
		"a_indices",
		"a_weight",
		"a_texcoord0",
		"a_texcoord1",
		"a_texcoord2",
		"a_texcoord3",
		"a_texcoord4",
		"a_texcoord5",
		"a_texcoord6",
		"a_texcoord7",
	};

	static const char* s_instanceDataName[BGFX_CONFIG_MAX_INSTANCE_DATA_COUNT] =
	{
		"i_data0",
		"i_data1",
		"i_data2",
		"i_data3",
		"i_data4",
	};

	static const GLenum s_attribType[AttribType::Count] =
	{
		GL_UNSIGNED_BYTE,
		GL_SHORT,
		GL_HALF_FLOAT,
		GL_FLOAT,
	};

	struct Blend
	{
		GLenum m_src;
		GLenum m_dst;
		bool m_factor;
	};

	static const Blend s_blendFactor[] =
	{
		{ 0,                           0,                           false }, // ignored
		{ GL_ZERO,                     GL_ZERO,                     false }, // ZERO
		{ GL_ONE,                      GL_ONE,                      false }, // ONE
		{ GL_SRC_COLOR,                GL_SRC_COLOR,                false }, // SRC_COLOR
		{ GL_ONE_MINUS_SRC_COLOR,      GL_ONE_MINUS_SRC_COLOR,      false }, // INV_SRC_COLOR
		{ GL_SRC_ALPHA,                GL_SRC_ALPHA,                false }, // SRC_ALPHA
		{ GL_ONE_MINUS_SRC_ALPHA,      GL_ONE_MINUS_SRC_ALPHA,      false }, // INV_SRC_ALPHA
		{ GL_DST_ALPHA,                GL_DST_ALPHA,                false }, // DST_ALPHA
		{ GL_ONE_MINUS_DST_ALPHA,      GL_ONE_MINUS_DST_ALPHA,      false }, // INV_DST_ALPHA
		{ GL_DST_COLOR,                GL_DST_COLOR,                false }, // DST_COLOR
		{ GL_ONE_MINUS_DST_COLOR,      GL_ONE_MINUS_DST_COLOR,      false }, // INV_DST_COLOR
		{ GL_SRC_ALPHA_SATURATE,       GL_ONE,                      false }, // SRC_ALPHA_SAT
		{ GL_CONSTANT_COLOR,           GL_CONSTANT_COLOR,           true  }, // FACTOR
		{ GL_ONE_MINUS_CONSTANT_COLOR, GL_ONE_MINUS_CONSTANT_COLOR, true  }, // INV_FACTOR
	};

	static const GLenum s_blendEquation[] =
	{
		GL_FUNC_ADD,
		GL_FUNC_SUBTRACT,
		GL_FUNC_REVERSE_SUBTRACT,
		GL_MIN,
		GL_MAX,
	};

	static const GLenum s_cmpFunc[] =
	{
		0, // ignored
		GL_LESS,
		GL_LEQUAL,
		GL_EQUAL,
		GL_GEQUAL,
		GL_GREATER,
		GL_NOTEQUAL,
		GL_NEVER,
		GL_ALWAYS,
	};

	static const GLenum s_stencilOp[] =
	{
		GL_ZERO,
		GL_KEEP,
		GL_REPLACE,
		GL_INCR_WRAP,
		GL_INCR,
		GL_DECR_WRAP,
		GL_DECR,
		GL_INVERT,
	};

	static const GLenum s_stencilFace[] =
	{
		GL_FRONT_AND_BACK,
		GL_FRONT,
		GL_BACK,
	};

	static const GLenum s_textureAddress[] =
	{
		GL_REPEAT,
		GL_MIRRORED_REPEAT,
		GL_CLAMP_TO_EDGE,
	};

	static const GLenum s_textureFilterMag[] =
	{
		GL_LINEAR,
		GL_NEAREST,
		GL_LINEAR,
	};

	static const GLenum s_textureFilterMin[][3] =
	{
		{ GL_LINEAR,  GL_LINEAR_MIPMAP_LINEAR,  GL_NEAREST_MIPMAP_LINEAR  },
		{ GL_NEAREST, GL_LINEAR_MIPMAP_NEAREST, GL_NEAREST_MIPMAP_NEAREST },
		{ GL_LINEAR,  GL_LINEAR_MIPMAP_LINEAR,  GL_NEAREST_MIPMAP_LINEAR  },
	};

	struct TextureFormatInfo
	{
		GLenum m_internalFmt;
		GLenum m_fmt;
		GLenum m_type;
		bool m_supported;
	};

	static TextureFormatInfo s_textureFormat[TextureFormat::Count] =
	{
		{ GL_COMPRESSED_RGBA_S3TC_DXT1_EXT,            GL_COMPRESSED_RGBA_S3TC_DXT1_EXT,            GL_ZERO,                        false }, // BC1
		{ GL_COMPRESSED_RGBA_S3TC_DXT3_EXT,            GL_COMPRESSED_RGBA_S3TC_DXT3_EXT,            GL_ZERO,                        false }, // BC2
		{ GL_COMPRESSED_RGBA_S3TC_DXT5_EXT,            GL_COMPRESSED_RGBA_S3TC_DXT5_EXT,            GL_ZERO,                        false }, // BC3
		{ GL_COMPRESSED_LUMINANCE_LATC1_EXT,           GL_COMPRESSED_LUMINANCE_LATC1_EXT,           GL_ZERO,                        false }, // BC4
		{ GL_COMPRESSED_LUMINANCE_ALPHA_LATC2_EXT,     GL_COMPRESSED_LUMINANCE_ALPHA_LATC2_EXT,     GL_ZERO,                        false }, // BC5
		{ GL_ETC1_RGB8_OES,                            GL_ETC1_RGB8_OES,                            GL_ZERO,                        false }, // ETC1
		{ GL_COMPRESSED_RGB8_ETC2,                     GL_COMPRESSED_RGB8_ETC2,                     GL_ZERO,                        false }, // ETC2
		{ GL_COMPRESSED_RGBA8_ETC2_EAC,                GL_COMPRESSED_RGBA8_ETC2_EAC,                GL_ZERO,                        false }, // ETC2A
		{ GL_COMPRESSED_RGB8_PUNCHTHROUGH_ALPHA1_ETC2, GL_COMPRESSED_RGB8_PUNCHTHROUGH_ALPHA1_ETC2, GL_ZERO,                        false }, // ETC2A1
		{ GL_COMPRESSED_RGB_PVRTC_2BPPV1_IMG,          GL_COMPRESSED_RGB_PVRTC_2BPPV1_IMG,          GL_ZERO,                        false }, // PTC12
		{ GL_COMPRESSED_RGB_PVRTC_4BPPV1_IMG,          GL_COMPRESSED_RGB_PVRTC_4BPPV1_IMG,          GL_ZERO,                        false }, // PTC14
		{ GL_COMPRESSED_RGBA_PVRTC_2BPPV1_IMG,         GL_COMPRESSED_RGBA_PVRTC_2BPPV1_IMG,         GL_ZERO,                        false }, // PTC12A
		{ GL_COMPRESSED_RGBA_PVRTC_4BPPV1_IMG,         GL_COMPRESSED_RGBA_PVRTC_4BPPV1_IMG,         GL_ZERO,                        false }, // PTC14A
		{ GL_COMPRESSED_RGBA_PVRTC_2BPPV2_IMG,         GL_COMPRESSED_RGBA_PVRTC_2BPPV2_IMG,         GL_ZERO,                        false }, // PTC22
		{ GL_COMPRESSED_RGBA_PVRTC_4BPPV2_IMG,         GL_COMPRESSED_RGBA_PVRTC_4BPPV2_IMG,         GL_ZERO,                        false }, // PTC24
		{ GL_ZERO,                                     GL_ZERO,                                     GL_ZERO,                        true  }, // Unknown
		{ GL_LUMINANCE,                                GL_LUMINANCE,                                GL_UNSIGNED_BYTE,               true  }, // R8
		{ GL_R16,                                      GL_RED,                                      GL_UNSIGNED_SHORT,              true  }, // R16
		{ GL_R16F,                                     GL_RED,                                      GL_HALF_FLOAT,                  true  }, // R16F
		{ GL_RGBA,                                     GL_RGBA,                                     GL_UNSIGNED_BYTE,               true  }, // BGRA8
		{ GL_RGBA16,                                   GL_RGBA,                                     GL_UNSIGNED_BYTE,               true  }, // RGBA16
		{ GL_RGBA16F,                                  GL_RGBA,                                     GL_HALF_FLOAT,                  true  }, // RGBA16F
		{ GL_RGB565,                                   GL_RGB,                                      GL_UNSIGNED_SHORT_5_6_5,        true  }, // R5G6B5
		{ GL_RGBA4,                                    GL_RGBA,                                     GL_UNSIGNED_SHORT_4_4_4_4,      true  }, // RGBA4
		{ GL_RGB5_A1,                                  GL_RGBA,                                     GL_UNSIGNED_SHORT_5_5_5_1,      true  }, // RGB5A1
		{ GL_RGB10_A2,                                 GL_RGBA,                                     GL_UNSIGNED_INT_2_10_10_10_REV, true  }, // RGB10A2
		{ GL_ZERO,                                     GL_ZERO,                                     GL_ZERO,                        true  }, // UnknownDepth
		{ GL_DEPTH_COMPONENT16,                        GL_DEPTH_COMPONENT,                          GL_UNSIGNED_SHORT,              false }, // D16
		{ GL_DEPTH_COMPONENT24,                        GL_DEPTH_COMPONENT,                          GL_UNSIGNED_INT,                false }, // D24
		{ GL_DEPTH24_STENCIL8,                         GL_DEPTH_STENCIL,                            GL_UNSIGNED_INT_24_8,           false }, // D24S8
		{ GL_DEPTH_COMPONENT32,                        GL_DEPTH_COMPONENT,                          GL_UNSIGNED_INT,                false }, // D32
		{ GL_DEPTH_COMPONENT32F,                       GL_DEPTH_COMPONENT,                          GL_FLOAT,                       false }, // D16F
		{ GL_DEPTH_COMPONENT32F,                       GL_DEPTH_COMPONENT,                          GL_FLOAT,                       false }, // D24F
		{ GL_DEPTH_COMPONENT32F,                       GL_DEPTH_COMPONENT,                          GL_FLOAT,                       false }, // D32F
		{ GL_STENCIL_INDEX8,                           GL_DEPTH_STENCIL,                            GL_UNSIGNED_BYTE,               false }, // D0S8
	};

	static const Matrix4 s_bias =
	{{{
		0.5f, 0.0f, 0.0f, 0.0f,
		0.0f, 0.5f, 0.0f, 0.0f,
		0.0f, 0.0f, 0.5f, 0.0f,
		0.5f, 0.5f, 0.5f, 1.0f,
	}}};

	struct Extension
	{
		enum Enum
		{
			ANGLE_depth_texture,
			ANGLE_framebuffer_blit,
			ANGLE_framebuffer_multisample,
			ANGLE_instanced_arrays,
			ANGLE_texture_compression_dxt1,
			ANGLE_texture_compression_dxt3,
			ANGLE_texture_compression_dxt5,
			ANGLE_translated_shader_source,

			APPLE_texture_format_BGRA8888,
			APPLE_texture_max_level,

			ARB_debug_label,
			ARB_debug_output,
			ARB_depth_clamp,
			ARB_draw_buffers_blend,
			ARB_ES3_compatibility,
			ARB_framebuffer_object,
			ARB_framebuffer_sRGB,
			ARB_get_program_binary,
			ARB_half_float_pixel,
			ARB_half_float_vertex,
			ARB_instanced_arrays,
			ARB_map_buffer_range,
			ARB_multisample,
			ARB_sampler_objects,
			ARB_seamless_cube_map,
			ARB_shader_texture_lod,
			ARB_texture_compression_rgtc,
			ARB_texture_float,
			ARB_texture_multisample,
			ARB_texture_storage,
			ARB_texture_swizzle,
			ARB_timer_query,
			ARB_uniform_buffer_object,
			ARB_vertex_array_object,
			ARB_vertex_type_2_10_10_10_rev,

			ATI_meminfo,

			CHROMIUM_depth_texture,
			CHROMIUM_framebuffer_multisample,
			CHROMIUM_texture_compression_dxt3,
			CHROMIUM_texture_compression_dxt5,

			EXT_bgra,
			EXT_blend_color,
			EXT_blend_minmax,
			EXT_blend_subtract,
			EXT_debug_label,
			EXT_debug_marker,
			EXT_frag_depth,
			EXT_framebuffer_blit,
			EXT_framebuffer_object,
			EXT_framebuffer_sRGB,
			EXT_occlusion_query_boolean,
			EXT_read_format_bgra,
			EXT_shader_texture_lod,
			EXT_shadow_samplers,
			EXT_texture_array,
			EXT_texture_compression_dxt1,
			EXT_texture_compression_latc,
			EXT_texture_compression_rgtc,
			EXT_texture_compression_s3tc,
			EXT_texture_filter_anisotropic,
			EXT_texture_format_BGRA8888,
			EXT_texture_sRGB,
			EXT_texture_storage,
			EXT_texture_swizzle,
			EXT_texture_type_2_10_10_10_REV,
			EXT_timer_query,
			EXT_unpack_subimage,

			GOOGLE_depth_texture,

			GREMEDY_string_marker,
			GREMEDY_frame_terminator,

			IMG_multisampled_render_to_texture,
			IMG_read_format,
			IMG_shader_binary,
			IMG_texture_compression_pvrtc,
			IMG_texture_compression_pvrtc2,
			IMG_texture_format_BGRA8888,

			KHR_debug,

			MOZ_WEBGL_compressed_texture_s3tc,
			MOZ_WEBGL_depth_texture,

			NV_draw_buffers,
			NVX_gpu_memory_info,

			OES_compressed_ETC1_RGB8_texture,
			OES_depth24,
			OES_depth32,
			OES_depth_texture,
			OES_fragment_precision_high,
			OES_get_program_binary,
			OES_required_internalformat,
			OES_packed_depth_stencil,
			OES_read_format,
			OES_rgb8_rgba8,
			OES_standard_derivatives,
			OES_texture_3D,
			OES_texture_float,
			OES_texture_float_linear,
			OES_texture_npot,
			OES_texture_half_float,
			OES_texture_half_float_linear,
			OES_vertex_array_object,
			OES_vertex_half_float,
			OES_vertex_type_10_10_10_2,

			WEBGL_compressed_texture_etc1,
			WEBGL_compressed_texture_s3tc,
			WEBGL_compressed_texture_pvrtc,
			WEBGL_depth_texture,

			WEBKIT_EXT_texture_filter_anisotropic,
			WEBKIT_WEBGL_compressed_texture_s3tc,
			WEBKIT_WEBGL_depth_texture,

			Count
		};

		const char* m_name;
		bool m_supported;
		bool m_initialize;
	};

	static Extension s_extension[Extension::Count] =
	{
		{ "ANGLE_depth_texture",                   false,                             true  },
		{ "ANGLE_framebuffer_blit",                false,                             true  },
		{ "ANGLE_framebuffer_multisample",         false,                             false },
		{ "ANGLE_instanced_arrays",                false,                             true  },
		{ "ANGLE_texture_compression_dxt1",        false,                             true  },
		{ "ANGLE_texture_compression_dxt3",        false,                             true  },
		{ "ANGLE_texture_compression_dxt5",        false,                             true  },
		{ "ANGLE_translated_shader_source",        false,                             true  },

		{ "APPLE_texture_format_BGRA8888",         false,                             true  },
		{ "APPLE_texture_max_level",               false,                             true  },

		{ "ARB_debug_label",                       false,                             true  },
		{ "ARB_debug_output",                      BGFX_CONFIG_RENDERER_OPENGL >= 43, true  },
		{ "ARB_depth_clamp",                       BGFX_CONFIG_RENDERER_OPENGL >= 32, true  },
		{ "ARB_draw_buffers_blend",                BGFX_CONFIG_RENDERER_OPENGL >= 40, true  },
		{ "ARB_ES3_compatibility",                 BGFX_CONFIG_RENDERER_OPENGL >= 43, true  },
		{ "ARB_framebuffer_object",                BGFX_CONFIG_RENDERER_OPENGL >= 30, true  },
		{ "ARB_framebuffer_sRGB",                  BGFX_CONFIG_RENDERER_OPENGL >= 30, true  },
		{ "ARB_get_program_binary",                BGFX_CONFIG_RENDERER_OPENGL >= 41, true  },
		{ "ARB_half_float_pixel",                  BGFX_CONFIG_RENDERER_OPENGL >= 30, true  },
		{ "ARB_half_float_vertex",                 BGFX_CONFIG_RENDERER_OPENGL >= 30, true  },
		{ "ARB_instanced_arrays",                  BGFX_CONFIG_RENDERER_OPENGL >= 33, true  },
		{ "ARB_map_buffer_range",                  BGFX_CONFIG_RENDERER_OPENGL >= 30, true  },
		{ "ARB_multisample",                       false,                             true  },
		{ "ARB_sampler_objects",                   BGFX_CONFIG_RENDERER_OPENGL >= 33, true  },
		{ "ARB_seamless_cube_map",                 BGFX_CONFIG_RENDERER_OPENGL >= 32, true  },
		{ "ARB_shader_texture_lod",                BGFX_CONFIG_RENDERER_OPENGL >= 30, true  },
		{ "ARB_texture_compression_rgtc",          BGFX_CONFIG_RENDERER_OPENGL >= 30, true  },
		{ "ARB_texture_float",                     BGFX_CONFIG_RENDERER_OPENGL >= 30, true  },
		{ "ARB_texture_multisample",               BGFX_CONFIG_RENDERER_OPENGL >= 32, true  },
		{ "ARB_texture_storage",                   BGFX_CONFIG_RENDERER_OPENGL >= 42, true  },
		{ "ARB_texture_swizzle",                   BGFX_CONFIG_RENDERER_OPENGL >= 33, true  },
		{ "ARB_timer_query",                       BGFX_CONFIG_RENDERER_OPENGL >= 33, true  },
		{ "ARB_uniform_buffer_object",             BGFX_CONFIG_RENDERER_OPENGL >= 31, true  },
		{ "ARB_vertex_array_object",               BGFX_CONFIG_RENDERER_OPENGL >= 30, true  },
		{ "ARB_vertex_type_2_10_10_10_rev",        false,                             true  },

		{ "ATI_meminfo",                           false,                             true  },

		{ "CHROMIUM_depth_texture",                false,                             true  },
		{ "CHROMIUM_framebuffer_multisample",      false,                             true  },
		{ "CHROMIUM_texture_compression_dxt3",     false,                             true  },
		{ "CHROMIUM_texture_compression_dxt5",     false,                             true  },

		{ "EXT_bgra",                              false,                             true  },
		{ "EXT_blend_color",                       BGFX_CONFIG_RENDERER_OPENGL >= 31, true  },
		{ "EXT_blend_minmax",                      BGFX_CONFIG_RENDERER_OPENGL >= 14, true  },
		{ "EXT_blend_subtract",                    BGFX_CONFIG_RENDERER_OPENGL >= 14, true  },
		{ "EXT_debug_label",                       false,                             true  },
		{ "EXT_debug_marker",                      false,                             true  },
		{ "EXT_frag_depth",                        false,                             true  }, // GLES2 extension.
		{ "EXT_framebuffer_blit",                  BGFX_CONFIG_RENDERER_OPENGL >= 30, true  },
		{ "EXT_framebuffer_object",                BGFX_CONFIG_RENDERER_OPENGL >= 30, true  },
		{ "EXT_framebuffer_sRGB",                  BGFX_CONFIG_RENDERER_OPENGL >= 30, true  },
		{ "EXT_occlusion_query_boolean",           false,                             true  },
		{ "EXT_read_format_bgra",                  false,                             true  },
		{ "EXT_shader_texture_lod",                false,                             true  }, // GLES2 extension.
		{ "EXT_shadow_samplers",                   false,                             true  },
		{ "EXT_texture_array",                     BGFX_CONFIG_RENDERER_OPENGL >= 30, true  },
		{ "EXT_texture_compression_dxt1",          false,                             true  },
		{ "EXT_texture_compression_latc",          false,                             true  },
		{ "EXT_texture_compression_rgtc",          BGFX_CONFIG_RENDERER_OPENGL >= 30, true  },
		{ "EXT_texture_compression_s3tc",          false,                             true  },
		{ "EXT_texture_filter_anisotropic",        false,                             true  },
		{ "EXT_texture_format_BGRA8888",           false,                             true  },
		{ "EXT_texture_sRGB",                      false,                             true  },
		{ "EXT_texture_storage",                   false,                             true  },
		{ "EXT_texture_swizzle",                   false,                             true  },
		{ "EXT_texture_type_2_10_10_10_REV",       false,                             true  },
		{ "EXT_timer_query",                       false,                             true  },
		{ "EXT_unpack_subimage",                   false,                             true  },

		{ "GOOGLE_depth_texture",                  false,                             true  },

		{ "GREMEDY_string_marker",                 false,                             true  },
		{ "GREMEDY_frame_terminator",              false,                             true  },

		{ "IMG_multisampled_render_to_texture",    false,                             true  },
		{ "IMG_read_format",                       false,                             true  },
		{ "IMG_shader_binary",                     false,                             true  },
		{ "IMG_texture_compression_pvrtc",         false,                             true  },
		{ "IMG_texture_compression_pvrtc2",        false,                             true  },
		{ "IMG_texture_format_BGRA8888",           false,                             true  },

		{ "KHR_debug",                             BGFX_CONFIG_RENDERER_OPENGL >= 43, true  },

		{ "MOZ_WEBGL_compressed_texture_s3tc",     false,                             true  },
		{ "MOZ_WEBGL_depth_texture",               false,                             true  },

		{ "NV_draw_buffers",                       false,                             true  }, // GLES2 extension.
		{ "NVX_gpu_memory_info",                   false,                             true  },

		{ "OES_compressed_ETC1_RGB8_texture",      false,                             true  },
		{ "OES_depth24",                           false,                             true  },
		{ "OES_depth32",                           false,                             true  },
		{ "OES_depth_texture",                     false,                             true  },
		{ "OES_fragment_precision_high",           false,                             true  },
		{ "OES_get_program_binary",                false,                             true  },
		{ "OES_required_internalformat",           false,                             true  },
		{ "OES_packed_depth_stencil",              false,                             true  },
		{ "OES_read_format",                       false,                             true  },
		{ "OES_rgb8_rgba8",                        false,                             true  },
		{ "OES_standard_derivatives",              false,                             true  },
		{ "OES_texture_3D",                        false,                             true  },
		{ "OES_texture_float",                     false,                             true  },
		{ "OES_texture_float_linear",              false,                             true  },
		{ "OES_texture_npot",                      false,                             true  },
		{ "OES_texture_half_float",                false,                             true  },
		{ "OES_texture_half_float_linear",         false,                             true  },
		{ "OES_vertex_array_object",               false,                             !BX_PLATFORM_IOS },
		{ "OES_vertex_half_float",                 false,                             true  },
		{ "OES_vertex_type_10_10_10_2",            false,                             true  },

		{ "WEBGL_compressed_texture_etc1",         false,                             true  },
		{ "WEBGL_compressed_texture_s3tc",         false,                             true  },
		{ "WEBGL_compressed_texture_pvrtc",        false,                             true  },
		{ "WEBGL_depth_texture",                   false,                             true  },

		{ "WEBKIT_EXT_texture_filter_anisotropic", false,                             true  },
		{ "WEBKIT_WEBGL_compressed_texture_s3tc",  false,                             true  },
		{ "WEBKIT_WEBGL_depth_texture",            false,                             true  },
	};

	static const char* s_ARB_shader_texture_lod[] =
	{
		"texture2DLod",
		"texture2DProjLod",
		"texture3DLod",
		"texture3DProjLod",
		"textureCubeLod",
		"shadow2DLod",
		"shadow2DProjLod",
		NULL
		// "texture1DLod",
		// "texture1DProjLod",
		// "shadow1DLod",
		// "shadow1DProjLod",
	};

	static const char* s_EXT_shader_texture_lod[] =
	{
		"texture2DLod",
		"texture2DProjLod",
		"textureCubeLod",
		NULL
		// "texture2DGrad",
		// "texture2DProjGrad",
		// "textureCubeGrad",
	};

	static const char* s_EXT_shadow_samplers[] =
	{
		"shadow2D",
		"shadow2DProj",
		NULL
	};

	static const char* s_OES_standard_derivatives[] =
	{
		"dFdx",
		"dFdy",
		"fwidth",
		NULL
	};

	static const char* s_OES_texture_3D[] =
	{
		"texture3D",
		"texture3DProj",
		"texture3DLod",
		"texture3DProjLod",
		NULL
	};

	static void GL_APIENTRY stubVertexAttribDivisor(GLuint /*_index*/, GLuint /*_divisor*/)
	{
	}

	static void GL_APIENTRY stubDrawArraysInstanced(GLenum _mode, GLint _first, GLsizei _count, GLsizei /*_primcount*/)
	{
		GL_CHECK(glDrawArrays(_mode, _first, _count) );
	}

	static void GL_APIENTRY stubDrawElementsInstanced(GLenum _mode, GLsizei _count, GLenum _type, const GLvoid* _indices, GLsizei /*_primcount*/)
	{
		GL_CHECK(glDrawElements(_mode, _count, _type, _indices) );
	}

	static void GL_APIENTRY stubFrameTerminatorGREMEDY()
	{
	}

	static void GL_APIENTRY stubInsertEventMarker(GLsizei /*_length*/, const char* /*_marker*/)
	{
	}

	static void GL_APIENTRY stubInsertEventMarkerGREMEDY(GLsizei _length, const char* _marker)
	{
		// If <marker> is a null-terminated string then <length> should not
		// include the terminator.
		//
		// If <length> is 0 then <marker> is assumed to be null-terminated.

		uint32_t size = (0 == _length ? (uint32_t)strlen(_marker) : _length) + 1;
		size *= sizeof(wchar_t);
		wchar_t* name = (wchar_t*)alloca(size);
		mbstowcs(name, _marker, size-2);
		GL_CHECK(glStringMarkerGREMEDY(_length, _marker) );
	}

	static void GL_APIENTRY stubObjectLabel(GLenum /*_identifier*/, GLuint /*_name*/, GLsizei /*_length*/, const char* /*_label*/)
	{
	}

	typedef void (*PostSwapBuffersFn)(uint32_t _width, uint32_t _height);

	static const char* getGLString(GLenum _name)
	{
		const char* str = (const char*)glGetString(_name);
		glGetError(); // ignore error if glGetString returns NULL.
		if (NULL != str)
		{
			return str;
		}

		return "<unknown>";
	}

	static uint32_t getGLStringHash(GLenum _name)
	{
		const char* str = (const char*)glGetString(_name);
		glGetError(); // ignore error if glGetString returns NULL.
		if (NULL != str)
		{
			return bx::hashMurmur2A(str, (uint32_t)strlen(str) );
		}

		return 0;
	}

	void dumpExtensions(const char* _extensions)
	{
		if (NULL != _extensions)
		{
			char name[1024];
			const char* pos = _extensions;
			const char* end = _extensions + strlen(_extensions);
			while (pos < end)
			{
				uint32_t len;
				const char* space = strchr(pos, ' ');
				if (NULL != space)
				{
					len = bx::uint32_min(sizeof(name), (uint32_t)(space - pos) );
				}
				else
				{
					len = bx::uint32_min(sizeof(name), (uint32_t)strlen(pos) );
				}

				strncpy(name, pos, len);
				name[len] = '\0';

				BX_TRACE("\t%s", name);

				pos += len+1;
			}
		}
	}

	const char* toString(GLenum _enum)
	{
#if defined(GL_DEBUG_SOURCE_API_ARB)
		switch (_enum)
		{
		case GL_DEBUG_SOURCE_API_ARB:               return "API";
		case GL_DEBUG_SOURCE_WINDOW_SYSTEM_ARB:     return "WinSys";
		case GL_DEBUG_SOURCE_SHADER_COMPILER_ARB:   return "Shader";
		case GL_DEBUG_SOURCE_THIRD_PARTY_ARB:       return "3rdparty";
		case GL_DEBUG_SOURCE_APPLICATION_ARB:       return "Application";
		case GL_DEBUG_SOURCE_OTHER_ARB:             return "Other";
		case GL_DEBUG_TYPE_ERROR_ARB:               return "Error";
		case GL_DEBUG_TYPE_DEPRECATED_BEHAVIOR_ARB: return "Deprecated behavior";
		case GL_DEBUG_TYPE_UNDEFINED_BEHAVIOR_ARB:  return "Undefined behavior";
		case GL_DEBUG_TYPE_PORTABILITY_ARB:         return "Portability";
		case GL_DEBUG_TYPE_PERFORMANCE_ARB:         return "Performance";
		case GL_DEBUG_TYPE_OTHER_ARB:               return "Other";
		case GL_DEBUG_SEVERITY_HIGH_ARB:            return "High";
		case GL_DEBUG_SEVERITY_MEDIUM_ARB:          return "Medium";
		case GL_DEBUG_SEVERITY_LOW_ARB:             return "Low";
		default:
			break;
		}
#else
		BX_UNUSED(_enum);
#endif // defined(GL_DEBUG_SOURCE_API_ARB)

		return "<unknown>";
	}

	void GL_APIENTRY debugProcCb(GLenum _source, GLenum _type, GLuint _id, GLenum _severity, GLsizei /*_length*/, const GLchar* _message, const void* /*_userParam*/)
	{
		BX_TRACE("src %s, type %s, id %d, severity %s, '%s'"
				, toString(_source)
				, toString(_type)
				, _id
				, toString(_severity)
				, _message
				);
		BX_UNUSED(_source, _type, _id, _severity, _message);
	}

	GLint glGet(GLenum _pname)
	{
		GLint result = 0;
		glGetIntegerv(_pname, &result);
		GLenum err = glGetError();
		BX_WARN(0 == err, "glGetIntegerv(0x%04x, ...) failed with GL error: 0x%04x.", _pname, err);
		return 0 == err ? result : 0;
	}

	void setTextureFormat(TextureFormat::Enum _format, GLenum _internalFmt, GLenum _fmt, GLenum _type = GL_ZERO)
	{
		TextureFormatInfo& tfi = s_textureFormat[_format];
		tfi.m_internalFmt = _internalFmt;
		tfi.m_fmt         = _fmt;
		tfi.m_type        = _type;
	}

	bool isTextureFormatValid(TextureFormat::Enum _format)
	{
		GLuint id;
		GL_CHECK(glGenTextures(1, &id) );
		GL_CHECK(glBindTexture(GL_TEXTURE_2D, id) );

		const TextureFormatInfo& tfi = s_textureFormat[_format];

		GLsizei size = (16*16*getBitsPerPixel(_format) )/8;
		void* data = alloca(size);

		if (isCompressed(_format) )
		{
			glCompressedTexImage2D(GL_TEXTURE_2D, 0, tfi.m_internalFmt, 16, 16, 0, size, data);
		}
		else
		{
			glTexImage2D(GL_TEXTURE_2D, 0, tfi.m_internalFmt, 16, 16, 0, tfi.m_fmt, tfi.m_type, data);
		}

		GLenum err = glGetError();
		BX_WARN(0 == err, "TextureFormat::%s is not supported (%x: %s).", getName(_format), err, glEnumName(err) );

		GL_CHECK(glDeleteTextures(1, &id) );

		return 0 == err;
	}

	struct RendererContext
	{
		RendererContext()
			: m_rtMsaa(false)
			, m_capture(NULL)
			, m_captureSize(0)
			, m_maxAnisotropy(0.0f)
			, m_maxMsaa(0)
			, m_vao(0)
			, m_vaoSupport(false)
			, m_samplerObjectSupport(false)
			, m_shadowSamplersSupport(false)
			, m_programBinarySupport(false)
			, m_textureSwizzleSupport(false)
			, m_depthTextureSupport(false)
			, m_useClearQuad(true)
			, m_flip(false)
			, m_hash( (BX_PLATFORM_WINDOWS<<1) | BX_ARCH_64BIT)
			, m_backBufferFbo(0)
			, m_msaaBackBufferFbo(0)
		{
			m_fbh.idx = invalidHandle;
			memset(m_uniforms, 0, sizeof(m_uniforms) );
			memset(&m_resolution, 0, sizeof(m_resolution) );
		}

		void updateResolution(const Resolution& _resolution)
		{
			if (m_resolution.m_width != _resolution.m_width
			||  m_resolution.m_height != _resolution.m_height
			||  m_resolution.m_flags != _resolution.m_flags)
			{
				m_textVideoMem.resize(false, _resolution.m_width, _resolution.m_height);
				m_textVideoMem.clear();

				m_resolution = _resolution;

				uint32_t msaa = (m_resolution.m_flags&BGFX_RESET_MSAA_MASK)>>BGFX_RESET_MSAA_SHIFT;
				msaa = bx::uint32_min(m_maxMsaa, msaa == 0 ? 0 : 1<<msaa);
				bool vsync = !!(m_resolution.m_flags&BGFX_RESET_VSYNC);
				setRenderContextSize(_resolution.m_width, _resolution.m_height, msaa, vsync);
				updateCapture();
			}
		}

		uint32_t setFrameBuffer(FrameBufferHandle _fbh, uint32_t _height, bool _msaa = true)
		{
			if (isValid(m_fbh)
			&&  m_fbh.idx != _fbh.idx
			&&  m_rtMsaa)
			{
				FrameBuffer& frameBuffer = m_frameBuffers[m_fbh.idx];
				frameBuffer.resolve();
			}

			if (!isValid(_fbh) )
			{
				GL_CHECK(glBindFramebuffer(GL_FRAMEBUFFER, m_msaaBackBufferFbo) );
			}
			else
			{
				FrameBuffer& frameBuffer = m_frameBuffers[_fbh.idx];
				GL_CHECK(glBindFramebuffer(GL_FRAMEBUFFER, frameBuffer.m_fbo[0]) );
				_height = frameBuffer.m_height;
			}

			m_fbh = _fbh;
			m_rtMsaa = _msaa;

			return _height;
		}

		uint32_t getNumRt() const
		{
			if (isValid(m_fbh) )
			{
				const FrameBuffer& frameBuffer = m_frameBuffers[m_fbh.idx];
				return frameBuffer.m_num;
			}

			return 1;
		}

		void createMsaaFbo(uint32_t _width, uint32_t _height, uint32_t _msaa)
		{
			if (0 == m_msaaBackBufferFbo // iOS
			&&  1 < _msaa)
			{
				GL_CHECK(glGenFramebuffers(1, &m_msaaBackBufferFbo) );
				GL_CHECK(glBindFramebuffer(GL_FRAMEBUFFER, m_msaaBackBufferFbo) );
				GL_CHECK(glGenRenderbuffers(BX_COUNTOF(m_msaaBackBufferRbos), m_msaaBackBufferRbos) );
				GL_CHECK(glBindRenderbuffer(GL_RENDERBUFFER, m_msaaBackBufferRbos[0]) );
				GL_CHECK(glRenderbufferStorageMultisample(GL_RENDERBUFFER, _msaa, GL_RGBA8, _width, _height) );
				GL_CHECK(glBindRenderbuffer(GL_RENDERBUFFER, m_msaaBackBufferRbos[1]) );
				GL_CHECK(glRenderbufferStorageMultisample(GL_RENDERBUFFER, _msaa, GL_DEPTH24_STENCIL8, _width, _height) );
				GL_CHECK(glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_RENDERBUFFER, m_msaaBackBufferRbos[0]) );

				GLenum attachment = BX_ENABLED(BGFX_CONFIG_RENDERER_OPENGL) || BX_ENABLED(BGFX_CONFIG_RENDERER_OPENGLES >= 30) 
					? GL_DEPTH_ATTACHMENT 
					: GL_DEPTH_STENCIL_ATTACHMENT
					;
				GL_CHECK(glFramebufferRenderbuffer(GL_FRAMEBUFFER, attachment, GL_RENDERBUFFER, m_msaaBackBufferRbos[1]) );

				BX_CHECK(GL_FRAMEBUFFER_COMPLETE ==  glCheckFramebufferStatus(GL_FRAMEBUFFER)
					, "glCheckFramebufferStatus failed 0x%08x"
					, glCheckFramebufferStatus(GL_FRAMEBUFFER)
					);

				GL_CHECK(glBindFramebuffer(GL_FRAMEBUFFER, m_msaaBackBufferFbo) );
			}
		}

		void destroyMsaaFbo()
		{
			if (m_backBufferFbo != m_msaaBackBufferFbo // iOS
			&&  0 != m_msaaBackBufferFbo)
			{
				GL_CHECK(glDeleteFramebuffers(1, &m_msaaBackBufferFbo) );
				GL_CHECK(glDeleteRenderbuffers(BX_COUNTOF(m_msaaBackBufferRbos), m_msaaBackBufferRbos) );
				m_msaaBackBufferFbo = 0;
			}
		}

		void blitMsaaFbo()
		{
			if (m_backBufferFbo != m_msaaBackBufferFbo // iOS
			&&  0 != m_msaaBackBufferFbo)
			{
				GL_CHECK(glDisable(GL_SCISSOR_TEST) );
				GL_CHECK(glBindFramebuffer(GL_FRAMEBUFFER, m_backBufferFbo) );
				GL_CHECK(glBindFramebuffer(GL_READ_FRAMEBUFFER, m_msaaBackBufferFbo) );
				GL_CHECK(glBindFramebuffer(GL_DRAW_FRAMEBUFFER, 0) );
				uint32_t width = m_resolution.m_width;
				uint32_t height = m_resolution.m_height;
				GLenum filter = BX_ENABLED(BGFX_CONFIG_RENDERER_OPENGL) || BX_ENABLED(BGFX_CONFIG_RENDERER_OPENGLES < 30) 
					? GL_NEAREST 
					: GL_LINEAR
					;
				GL_CHECK(glBlitFramebuffer(0
					, 0
					, width
					, height
					, 0
					, 0
					, width
					, height
					, GL_COLOR_BUFFER_BIT
					, filter
					) );
				GL_CHECK(glBindFramebuffer(GL_FRAMEBUFFER, m_backBufferFbo) );
			}
		}

		void setRenderContextSize(uint32_t _width, uint32_t _height, uint32_t _msaa = 0, bool _vsync = false)
		{
			if (_width != 0
			||  _height != 0)
			{
				if (!m_glctx.isValid() )
				{
					m_glctx.create(_width, _height);

#if BX_PLATFORM_IOS
					// iOS: need to figure out how to deal with FBO created by context.
					m_backBufferFbo = m_glctx.m_fbo;
					m_msaaBackBufferFbo = m_glctx.m_fbo;
#endif // BX_PLATFORM_IOS
				}
				else
				{
					destroyMsaaFbo();

					m_glctx.resize(_width, _height, _vsync);

					createMsaaFbo(_width, _height, _msaa);
				}
			}

			m_flip = true;
		}

		void flip()
		{
			if (m_flip)
			{
				m_glctx.swap();
			}
		}

		void invalidateCache()
		{
			if (m_vaoSupport)
			{
				m_vaoStateCache.invalidate();
			}

			if ( (BX_ENABLED(BGFX_CONFIG_RENDERER_OPENGL) ||  BX_ENABLED(BGFX_CONFIG_RENDERER_OPENGLES >= 30) )
			&&  m_samplerObjectSupport)
			{
				m_samplerStateCache.invalidate();
			}
		}

		void setSamplerState(uint32_t _stage, uint32_t _numMips, uint32_t _flags)
		{
			if (BX_ENABLED(BGFX_CONFIG_RENDERER_OPENGL)
			||  BX_ENABLED(BGFX_CONFIG_RENDERER_OPENGLES >= 30) )
			{
				if (0 == (BGFX_SAMPLER_DEFAULT_FLAGS & _flags) )
				{
					_flags &= ~BGFX_TEXTURE_RESERVED_MASK;
					_flags &= BGFX_TEXTURE_SAMPLER_BITS_MASK;
					_flags |= _numMips<<BGFX_TEXTURE_RESERVED_SHIFT;
					GLuint sampler = m_samplerStateCache.find(_flags);

					if (UINT32_MAX == sampler)
					{
						sampler = m_samplerStateCache.add(_flags);

						GL_CHECK(glSamplerParameteri(sampler, GL_TEXTURE_WRAP_S, s_textureAddress[(_flags&BGFX_TEXTURE_U_MASK)>>BGFX_TEXTURE_U_SHIFT]) );
						GL_CHECK(glSamplerParameteri(sampler, GL_TEXTURE_WRAP_T, s_textureAddress[(_flags&BGFX_TEXTURE_V_MASK)>>BGFX_TEXTURE_V_SHIFT]) );
						GL_CHECK(glSamplerParameteri(sampler, GL_TEXTURE_WRAP_R, s_textureAddress[(_flags&BGFX_TEXTURE_W_MASK)>>BGFX_TEXTURE_W_SHIFT]) );

						const uint32_t mag = (_flags&BGFX_TEXTURE_MAG_MASK)>>BGFX_TEXTURE_MAG_SHIFT;
						const uint32_t min = (_flags&BGFX_TEXTURE_MIN_MASK)>>BGFX_TEXTURE_MIN_SHIFT;
						const uint32_t mip = (_flags&BGFX_TEXTURE_MIP_MASK)>>BGFX_TEXTURE_MIP_SHIFT;
						GLenum minFilter = s_textureFilterMin[min][1 < _numMips ? mip+1 : 0];
						GL_CHECK(glSamplerParameteri(sampler, GL_TEXTURE_MAG_FILTER, s_textureFilterMag[mag]) );
						GL_CHECK(glSamplerParameteri(sampler, GL_TEXTURE_MIN_FILTER, minFilter) );
						if (0 != (_flags & (BGFX_TEXTURE_MIN_ANISOTROPIC|BGFX_TEXTURE_MAG_ANISOTROPIC) )
						&&  0.0f < m_maxAnisotropy)
						{
							GL_CHECK(glSamplerParameterf(sampler, GL_TEXTURE_MAX_ANISOTROPY_EXT, m_maxAnisotropy) );
						}

						if (BX_ENABLED(BGFX_CONFIG_RENDERER_OPENGLES >= 30)
						||  m_shadowSamplersSupport)
						{
							const uint32_t cmpFunc = (_flags&BGFX_TEXTURE_COMPARE_MASK)>>BGFX_TEXTURE_COMPARE_SHIFT;
							if (0 == cmpFunc)
							{
								GL_CHECK(glSamplerParameteri(sampler, GL_TEXTURE_COMPARE_MODE, GL_NONE) );
							}
							else
							{
								GL_CHECK(glSamplerParameteri(sampler, GL_TEXTURE_COMPARE_MODE, GL_COMPARE_REF_TO_TEXTURE) );
								GL_CHECK(glSamplerParameteri(sampler, GL_TEXTURE_COMPARE_FUNC, s_cmpFunc[cmpFunc]) );
							}
						}
					}

					GL_CHECK(glBindSampler(_stage, sampler) );
				}
				else
				{
					GL_CHECK(glBindSampler(_stage, 0) );
				}
			}
		}

		void updateCapture()
		{
			if (m_resolution.m_flags&BGFX_RESET_CAPTURE)
			{
				m_captureSize = m_resolution.m_width*m_resolution.m_height*4;
				m_capture = BX_REALLOC(g_allocator, m_capture, m_captureSize);
				g_callback->captureBegin(m_resolution.m_width, m_resolution.m_height, m_resolution.m_width*4, TextureFormat::BGRA8, true);
			}
			else
			{
				captureFinish();
			}
		}

		void capture()
		{
			if (NULL != m_capture)
			{
				GL_CHECK(glReadPixels(0
					, 0
					, m_resolution.m_width
					, m_resolution.m_height
					, m_readPixelsFmt
					, GL_UNSIGNED_BYTE
					, m_capture
					) );

				g_callback->captureFrame(m_capture, m_captureSize);
			}
		}

		void captureFinish()
		{
			if (NULL != m_capture)
			{
				g_callback->captureEnd();
				BX_FREE(g_allocator, m_capture);
				m_capture = NULL;
				m_captureSize = 0;
			}
		}

		void saveScreenShot(const char* _filePath)
		{
			uint32_t length = m_resolution.m_width*m_resolution.m_height*4;
			uint8_t* data = (uint8_t*)BX_ALLOC(g_allocator, length);

			uint32_t width = m_resolution.m_width;
			uint32_t height = m_resolution.m_height;

			GL_CHECK(glReadPixels(0
				, 0
				, width
				, height
				, m_readPixelsFmt
				, GL_UNSIGNED_BYTE
				, data
				) );

			if (GL_RGBA == m_readPixelsFmt)
			{
				imageSwizzleBgra8(width, height, width*4, data, data);
			}

			g_callback->screenShot(_filePath
				, width
				, height
				, width*4
				, data
				, length
				, true
				);
			BX_FREE(g_allocator, data);
		}

		void init()
		{
			setRenderContextSize(BGFX_DEFAULT_WIDTH, BGFX_DEFAULT_HEIGHT);

			m_vendor = getGLString(GL_VENDOR);
			m_renderer = getGLString(GL_RENDERER);
			m_version = getGLString(GL_VERSION);
			m_glslVersion = getGLString(GL_SHADING_LANGUAGE_VERSION);

			GLint numCmpFormats = 0;
			GL_CHECK(glGetIntegerv(GL_NUM_COMPRESSED_TEXTURE_FORMATS, &numCmpFormats) );
			BX_TRACE("GL_NUM_COMPRESSED_TEXTURE_FORMATS %d", numCmpFormats);

			GLint* cmpFormat = NULL;

			if (0 < numCmpFormats)
			{
				numCmpFormats = numCmpFormats > 256 ? 256 : numCmpFormats;
				cmpFormat = (GLint*)alloca(sizeof(GLint)*numCmpFormats);
				GL_CHECK(glGetIntegerv(GL_COMPRESSED_TEXTURE_FORMATS, cmpFormat) );

				for (GLint ii = 0; ii < numCmpFormats; ++ii)
				{
					GLint internalFmt = cmpFormat[ii];
					uint32_t fmt = uint32_t(TextureFormat::Unknown);
					for (uint32_t jj = 0; jj < fmt; ++jj)
					{
						if (s_textureFormat[jj].m_internalFmt == (GLenum)internalFmt)
						{
							s_textureFormat[jj].m_supported = true;
							fmt = jj;
						}
					}

					BX_TRACE("  %3d: %8x %s", ii, internalFmt, getName( (TextureFormat::Enum)fmt) );
				}
			}

			if (BX_ENABLED(BGFX_CONFIG_DEBUG) )
			{
#define GL_GET(_pname, _min) BX_TRACE("  " #_pname " %d (min: %d)", glGet(_pname), _min)
				BX_TRACE("Defaults:");
#if BGFX_CONFIG_RENDERER_OPENGL >= 41 || BGFX_CONFIG_RENDERER_OPENGLES
				GL_GET(GL_MAX_FRAGMENT_UNIFORM_VECTORS, 16);
				GL_GET(GL_MAX_VERTEX_UNIFORM_VECTORS, 128);
				GL_GET(GL_MAX_VARYING_VECTORS, 8);
#else
				GL_GET(GL_MAX_FRAGMENT_UNIFORM_COMPONENTS, 16 * 4);
				GL_GET(GL_MAX_VERTEX_UNIFORM_COMPONENTS, 128 * 4);
				GL_GET(GL_MAX_VARYING_FLOATS, 8 * 4);
#endif // BGFX_CONFIG_RENDERER_OPENGL >= 41 || BGFX_CONFIG_RENDERER_OPENGLES
				GL_GET(GL_MAX_VERTEX_ATTRIBS, 8);
				GL_GET(GL_MAX_COMBINED_TEXTURE_IMAGE_UNITS, 8);
				GL_GET(GL_MAX_CUBE_MAP_TEXTURE_SIZE, 16);
				GL_GET(GL_MAX_TEXTURE_IMAGE_UNITS, 8);
				GL_GET(GL_MAX_TEXTURE_SIZE, 64);
				GL_GET(GL_MAX_VERTEX_TEXTURE_IMAGE_UNITS, 0);
				GL_GET(GL_MAX_RENDERBUFFER_SIZE, 1);
#undef GL_GET

				BX_TRACE("      Vendor: %s", m_vendor);
				BX_TRACE("    Renderer: %s", m_renderer);
				BX_TRACE("     Version: %s", m_version);
				BX_TRACE("GLSL version: %s", m_glslVersion);
			}

			// Initial binary shader hash depends on driver version.
			m_hash = ( (BX_PLATFORM_WINDOWS<<1) | BX_ARCH_64BIT)
				^ (uint64_t(getGLStringHash(GL_VENDOR  ) )<<32)
				^ (uint64_t(getGLStringHash(GL_RENDERER) )<<0 )
				^ (uint64_t(getGLStringHash(GL_VERSION ) )<<16)
				;

			if (BX_ENABLED(BGFX_CONFIG_RENDERER_USE_EXTENSIONS) )
			{
				const char* extensions = (const char*)glGetString(GL_EXTENSIONS);
				glGetError(); // ignore error if glGetString returns NULL.
				if (NULL != extensions)
				{
					char name[1024];
					const char* pos = extensions;
					const char* end = extensions + strlen(extensions);
					uint32_t index = 0;
					while (pos < end)
					{
						uint32_t len;
						const char* space = strchr(pos, ' ');
						if (NULL != space)
						{
							len = bx::uint32_min(sizeof(name), (uint32_t)(space - pos) );
						}
						else
						{
							len = bx::uint32_min(sizeof(name), (uint32_t)strlen(pos) );
						}

						strncpy(name, pos, len);
						name[len] = '\0';

						bool supported = false;
						for (uint32_t ii = 0; ii < Extension::Count; ++ii)
						{
							Extension& extension = s_extension[ii];
							if (!extension.m_supported
							&&  extension.m_initialize)
							{
								const char* ext = name;
								if (0 == strncmp(ext, "GL_", 3) ) // skip GL_
								{
									ext += 3;
								}

								if (0 == strcmp(ext, extension.m_name) )
								{
									extension.m_supported = true;
									supported = true;
									break;
								}
							}
						}

						BX_TRACE("GL_EXTENSION %3d%s: %s", index, supported ? " (supported)" : "", name);
						BX_UNUSED(supported);

						pos += len+1;
						++index;
					}

					BX_TRACE("Supported extensions:");
					for (uint32_t ii = 0; ii < Extension::Count; ++ii)
					{
						if (s_extension[ii].m_supported)
						{
							BX_TRACE("\t%2d: %s", ii, s_extension[ii].m_name);
						}
					}
				}
			}

			bool bc123Supported = 0
				|| s_extension[Extension::EXT_texture_compression_s3tc        ].m_supported
				|| s_extension[Extension::MOZ_WEBGL_compressed_texture_s3tc   ].m_supported
				|| s_extension[Extension::WEBGL_compressed_texture_s3tc       ].m_supported
				|| s_extension[Extension::WEBKIT_WEBGL_compressed_texture_s3tc].m_supported
				;
			s_textureFormat[TextureFormat::BC1].m_supported |= bc123Supported
				|| s_extension[Extension::ANGLE_texture_compression_dxt1].m_supported
				|| s_extension[Extension::EXT_texture_compression_dxt1  ].m_supported
				;

			if (!s_textureFormat[TextureFormat::BC1].m_supported
			&& ( s_textureFormat[TextureFormat::BC2].m_supported || s_textureFormat[TextureFormat::BC3].m_supported) )
			{
				// If RGBA_S3TC_DXT1 is not supported, maybe RGB_S3TC_DXT1 is?
				for (GLint ii = 0; ii < numCmpFormats; ++ii)
				{
					if (GL_COMPRESSED_RGB_S3TC_DXT1_EXT == cmpFormat[ii])
					{
						setTextureFormat(TextureFormat::BC1, GL_COMPRESSED_RGB_S3TC_DXT1_EXT, GL_COMPRESSED_RGB_S3TC_DXT1_EXT);
						s_textureFormat[TextureFormat::BC1].m_supported   = true;
						break;
					}
				}
			}

			s_textureFormat[TextureFormat::BC2].m_supported |= bc123Supported
				|| s_extension[Extension::ANGLE_texture_compression_dxt3   ].m_supported
				|| s_extension[Extension::CHROMIUM_texture_compression_dxt3].m_supported
				;

			s_textureFormat[TextureFormat::BC3].m_supported |= bc123Supported
				|| s_extension[Extension::ANGLE_texture_compression_dxt5   ].m_supported
				|| s_extension[Extension::CHROMIUM_texture_compression_dxt5].m_supported
				;

			if (s_extension[Extension::EXT_texture_compression_latc].m_supported)
			{
				setTextureFormat(TextureFormat::BC4, GL_COMPRESSED_LUMINANCE_LATC1_EXT,       GL_COMPRESSED_LUMINANCE_LATC1_EXT);
				setTextureFormat(TextureFormat::BC5, GL_COMPRESSED_LUMINANCE_ALPHA_LATC2_EXT, GL_COMPRESSED_LUMINANCE_ALPHA_LATC2_EXT);
			}

			if (s_extension[Extension::ARB_texture_compression_rgtc].m_supported
			||  s_extension[Extension::EXT_texture_compression_rgtc].m_supported)
			{
				setTextureFormat(TextureFormat::BC4, GL_COMPRESSED_RED_RGTC1, GL_COMPRESSED_RED_RGTC1);
				setTextureFormat(TextureFormat::BC5, GL_COMPRESSED_RG_RGTC2,  GL_COMPRESSED_RG_RGTC2);
			}

			bool etc1Supported = 0
				|| s_extension[Extension::OES_compressed_ETC1_RGB8_texture].m_supported
				|| s_extension[Extension::WEBGL_compressed_texture_etc1   ].m_supported
				;
			s_textureFormat[TextureFormat::ETC1].m_supported |= etc1Supported;

			bool etc2Supported = !!(BGFX_CONFIG_RENDERER_OPENGLES >= 30)
				|| s_extension[Extension::ARB_ES3_compatibility].m_supported
				;
			s_textureFormat[TextureFormat::ETC2  ].m_supported |= etc2Supported;
			s_textureFormat[TextureFormat::ETC2A ].m_supported |= etc2Supported;
			s_textureFormat[TextureFormat::ETC2A1].m_supported |= etc2Supported;

			if (!s_textureFormat[TextureFormat::ETC1].m_supported
			&&   s_textureFormat[TextureFormat::ETC2].m_supported)
			{
				// When ETC2 is supported override ETC1 texture format settings.
				s_textureFormat[TextureFormat::ETC1].m_internalFmt = GL_COMPRESSED_RGB8_ETC2;
				s_textureFormat[TextureFormat::ETC1].m_fmt         = GL_COMPRESSED_RGB8_ETC2;
				s_textureFormat[TextureFormat::ETC1].m_supported   = true;
			}

			bool ptc1Supported = 0
				|| s_extension[Extension::IMG_texture_compression_pvrtc ].m_supported
				|| s_extension[Extension::WEBGL_compressed_texture_pvrtc].m_supported
				;
			s_textureFormat[TextureFormat::PTC12 ].m_supported |= ptc1Supported;
			s_textureFormat[TextureFormat::PTC14 ].m_supported |= ptc1Supported;
			s_textureFormat[TextureFormat::PTC12A].m_supported |= ptc1Supported;
			s_textureFormat[TextureFormat::PTC14A].m_supported |= ptc1Supported;

			bool ptc2Supported = s_extension[Extension::IMG_texture_compression_pvrtc2].m_supported;
			s_textureFormat[TextureFormat::PTC22].m_supported |= ptc2Supported;
			s_textureFormat[TextureFormat::PTC24].m_supported |= ptc2Supported;

			if (BX_ENABLED(BGFX_CONFIG_RENDERER_OPENGLES) )
			{
				setTextureFormat(TextureFormat::D32, GL_DEPTH_COMPONENT, GL_DEPTH_COMPONENT, GL_UNSIGNED_INT);

				if (BX_ENABLED(BGFX_CONFIG_RENDERER_OPENGLES >= 30) )
				{
					setTextureFormat(TextureFormat::R16,    GL_R16UI,    GL_RED_INTEGER,  GL_UNSIGNED_SHORT);
					setTextureFormat(TextureFormat::RGBA16, GL_RGBA16UI, GL_RGBA_INTEGER, GL_UNSIGNED_SHORT);
				}
				else
				{
					setTextureFormat(TextureFormat::RGBA16F, GL_RGBA, GL_RGBA, GL_HALF_FLOAT);

					if (BX_ENABLED(BX_PLATFORM_IOS) )
					{
						setTextureFormat(TextureFormat::D16,   GL_DEPTH_COMPONENT, GL_DEPTH_COMPONENT, GL_UNSIGNED_SHORT);
						setTextureFormat(TextureFormat::D24S8, GL_DEPTH_STENCIL,   GL_DEPTH_STENCIL,   GL_UNSIGNED_INT_24_8);
					}
				}
			}

			if (s_extension[Extension::EXT_texture_format_BGRA8888  ].m_supported
			||  s_extension[Extension::EXT_bgra                     ].m_supported
			||  s_extension[Extension::IMG_texture_format_BGRA8888  ].m_supported
			||  s_extension[Extension::APPLE_texture_format_BGRA8888].m_supported)
			{
				if (BX_ENABLED(BGFX_CONFIG_RENDERER_OPENGL) )
				{
					m_readPixelsFmt = GL_BGRA;
				}

				s_textureFormat[TextureFormat::BGRA8].m_fmt = GL_BGRA;

				// Mixing GLES and GL extensions here. OpenGL EXT_bgra and
				// APPLE_texture_format_BGRA8888 wants
				// format to be BGRA but internal format to stay RGBA, but
				// EXT_texture_format_BGRA8888 wants both format and internal
				// format to be BGRA.
				//
				// Reference:
				// https://www.khronos.org/registry/gles/extensions/EXT/EXT_texture_format_BGRA8888.txt
				// https://www.opengl.org/registry/specs/EXT/bgra.txt
				// https://www.khronos.org/registry/gles/extensions/APPLE/APPLE_texture_format_BGRA8888.txt
				if (!s_extension[Extension::EXT_bgra                     ].m_supported
				&&  !s_extension[Extension::APPLE_texture_format_BGRA8888].m_supported)
				{
					s_textureFormat[TextureFormat::BGRA8].m_internalFmt = GL_BGRA;
				}

				if (!isTextureFormatValid(TextureFormat::BGRA8) )
				{
					// Revert back to RGBA if texture can't be created.
					setTextureFormat(TextureFormat::BGRA8, GL_RGBA, GL_RGBA, GL_UNSIGNED_BYTE);
				}
			}

			if (!BX_ENABLED(BX_PLATFORM_EMSCRIPTEN) )
			{
				for (uint32_t ii = 0; ii < TextureFormat::Count; ++ii)
				{
					if (TextureFormat::Unknown != ii
					&&  TextureFormat::UnknownDepth != ii)
					{
						s_textureFormat[ii].m_supported = isTextureFormatValid( (TextureFormat::Enum)ii);
					}
				}
			}

			uint64_t supportedTextureFormats = 0
				| (s_textureFormat[TextureFormat::BC1   ].m_supported ? BGFX_CAPS_TEXTURE_FORMAT_BC1    : 0)
				| (s_textureFormat[TextureFormat::BC2   ].m_supported ? BGFX_CAPS_TEXTURE_FORMAT_BC2    : 0)
				| (s_textureFormat[TextureFormat::BC3   ].m_supported ? BGFX_CAPS_TEXTURE_FORMAT_BC3    : 0)
				| (s_textureFormat[TextureFormat::BC4   ].m_supported ? BGFX_CAPS_TEXTURE_FORMAT_BC4    : 0)
				| (s_textureFormat[TextureFormat::BC5   ].m_supported ? BGFX_CAPS_TEXTURE_FORMAT_BC5    : 0)
				| (s_textureFormat[TextureFormat::ETC1  ].m_supported ? BGFX_CAPS_TEXTURE_FORMAT_ETC1   : 0)
				| (s_textureFormat[TextureFormat::ETC2  ].m_supported ? BGFX_CAPS_TEXTURE_FORMAT_ETC2   : 0)
				| (s_textureFormat[TextureFormat::ETC2A ].m_supported ? BGFX_CAPS_TEXTURE_FORMAT_ETC2A  : 0)
				| (s_textureFormat[TextureFormat::ETC2A1].m_supported ? BGFX_CAPS_TEXTURE_FORMAT_ETC2A1 : 0)
				| (s_textureFormat[TextureFormat::PTC12 ].m_supported ? BGFX_CAPS_TEXTURE_FORMAT_PTC12  : 0)
				| (s_textureFormat[TextureFormat::PTC14 ].m_supported ? BGFX_CAPS_TEXTURE_FORMAT_PTC14  : 0)
				| (s_textureFormat[TextureFormat::PTC14A].m_supported ? BGFX_CAPS_TEXTURE_FORMAT_PTC14A : 0)
				| (s_textureFormat[TextureFormat::PTC12A].m_supported ? BGFX_CAPS_TEXTURE_FORMAT_PTC12A : 0)
				| (s_textureFormat[TextureFormat::PTC22 ].m_supported ? BGFX_CAPS_TEXTURE_FORMAT_PTC22  : 0)
				| (s_textureFormat[TextureFormat::PTC24 ].m_supported ? BGFX_CAPS_TEXTURE_FORMAT_PTC24  : 0)
				| 0
				| (s_textureFormat[TextureFormat::D16   ].m_supported ? BGFX_CAPS_TEXTURE_FORMAT_D16    : 0)
				| (s_textureFormat[TextureFormat::D24   ].m_supported ? BGFX_CAPS_TEXTURE_FORMAT_D24    : 0)
				| (s_textureFormat[TextureFormat::D24S8 ].m_supported ? BGFX_CAPS_TEXTURE_FORMAT_D24S8  : 0)
				| (s_textureFormat[TextureFormat::D32   ].m_supported ? BGFX_CAPS_TEXTURE_FORMAT_D32    : 0)
				| (s_textureFormat[TextureFormat::D16F  ].m_supported ? BGFX_CAPS_TEXTURE_FORMAT_D16F   : 0)
				| (s_textureFormat[TextureFormat::D24F  ].m_supported ? BGFX_CAPS_TEXTURE_FORMAT_D24F   : 0)
				| (s_textureFormat[TextureFormat::D32F  ].m_supported ? BGFX_CAPS_TEXTURE_FORMAT_D32F   : 0)
				| (s_textureFormat[TextureFormat::D0S8  ].m_supported ? BGFX_CAPS_TEXTURE_FORMAT_D0S8   : 0)
				;

			g_caps.supported |= supportedTextureFormats;

			g_caps.supported |= !!(BGFX_CONFIG_RENDERER_OPENGL || BGFX_CONFIG_RENDERER_OPENGLES >= 30) || s_extension[Extension::OES_texture_3D].m_supported
				? BGFX_CAPS_TEXTURE_3D
				: 0
				;
			g_caps.supported |= !!(BGFX_CONFIG_RENDERER_OPENGL || BGFX_CONFIG_RENDERER_OPENGLES >= 30) || s_extension[Extension::EXT_shadow_samplers].m_supported
				? BGFX_CAPS_TEXTURE_COMPARE_ALL
				: 0
				;
			g_caps.supported |= !!(BGFX_CONFIG_RENDERER_OPENGL || BGFX_CONFIG_RENDERER_OPENGLES >= 30) || s_extension[Extension::OES_vertex_half_float].m_supported
				? BGFX_CAPS_VERTEX_ATTRIB_HALF
				: 0
				;
			g_caps.supported |= !!(BGFX_CONFIG_RENDERER_OPENGL || BGFX_CONFIG_RENDERER_OPENGLES >= 30) || s_extension[Extension::EXT_frag_depth].m_supported
				? BGFX_CAPS_FRAGMENT_DEPTH
				: 0
				;

			g_caps.supported |= s_extension[Extension::ARB_draw_buffers_blend].m_supported
				? BGFX_CAPS_BLEND_INDEPENDENT
				: 0
				;
			g_caps.maxTextureSize = glGet(GL_MAX_TEXTURE_SIZE);

			if (BX_ENABLED(BGFX_CONFIG_RENDERER_OPENGL)
			||  BX_ENABLED(BGFX_CONFIG_RENDERER_OPENGLES >= 30) )
			{
				g_caps.maxFBAttachments = bx::uint32_min(glGet(GL_MAX_COLOR_ATTACHMENTS), BGFX_CONFIG_MAX_FRAME_BUFFER_ATTACHMENTS);
			}

			m_vaoSupport = !!(BGFX_CONFIG_RENDERER_OPENGLES >= 30)
				|| s_extension[Extension::ARB_vertex_array_object].m_supported
				|| s_extension[Extension::OES_vertex_array_object].m_supported
				;

			if (BX_ENABLED(BX_PLATFORM_NACL) )
			{
				m_vaoSupport &= NULL != glGenVertexArrays
					&& NULL != glDeleteVertexArrays
					&& NULL != glBindVertexArray
					;
			}

			if (m_vaoSupport)
			{
				GL_CHECK(glGenVertexArrays(1, &m_vao) );
			}

			m_samplerObjectSupport = !!(BGFX_CONFIG_RENDERER_OPENGLES >= 30)
				|| s_extension[Extension::ARB_sampler_objects].m_supported
				;

			m_shadowSamplersSupport = !!(BGFX_CONFIG_RENDERER_OPENGL || BGFX_CONFIG_RENDERER_OPENGLES >= 30)
				|| s_extension[Extension::EXT_shadow_samplers].m_supported
				;

			m_programBinarySupport = !!(BGFX_CONFIG_RENDERER_OPENGLES >= 30)
				|| s_extension[Extension::ARB_get_program_binary].m_supported
				|| s_extension[Extension::OES_get_program_binary].m_supported
				|| s_extension[Extension::IMG_shader_binary     ].m_supported
				;

			m_textureSwizzleSupport = false
				|| s_extension[Extension::ARB_texture_swizzle].m_supported
				|| s_extension[Extension::EXT_texture_swizzle].m_supported
				;

			m_depthTextureSupport = !!(BGFX_CONFIG_RENDERER_OPENGL || BGFX_CONFIG_RENDERER_OPENGLES >= 30)
				|| s_extension[Extension::ANGLE_depth_texture       ].m_supported
				|| s_extension[Extension::CHROMIUM_depth_texture    ].m_supported
				|| s_extension[Extension::GOOGLE_depth_texture      ].m_supported
				|| s_extension[Extension::OES_depth_texture         ].m_supported
				|| s_extension[Extension::MOZ_WEBGL_depth_texture   ].m_supported
				|| s_extension[Extension::WEBGL_depth_texture       ].m_supported
				|| s_extension[Extension::WEBKIT_WEBGL_depth_texture].m_supported
				;

			g_caps.supported |= m_depthTextureSupport 
				? BGFX_CAPS_TEXTURE_COMPARE_LEQUAL
				: 0
				;

			if (s_extension[Extension::EXT_texture_filter_anisotropic].m_supported)
			{
				GL_CHECK(glGetFloatv(GL_MAX_TEXTURE_MAX_ANISOTROPY_EXT, &m_maxAnisotropy) );
			}

			if (s_extension[Extension::ARB_texture_multisample].m_supported
			||  s_extension[Extension::ANGLE_framebuffer_multisample].m_supported)
			{
				GL_CHECK(glGetIntegerv(GL_MAX_SAMPLES, &m_maxMsaa) );
			}

			if (s_extension[Extension::OES_read_format].m_supported
			&& (s_extension[Extension::IMG_read_format].m_supported	|| s_extension[Extension::EXT_read_format_bgra].m_supported) )
			{
				m_readPixelsFmt = GL_BGRA;
			}
			else
			{
				m_readPixelsFmt = GL_RGBA;
			}

			if (BX_ENABLED(BGFX_CONFIG_RENDERER_OPENGLES >= 30) )
			{
				g_caps.supported |= BGFX_CAPS_INSTANCING;
			}
			else
			{
				if (!BX_ENABLED(BX_PLATFORM_IOS) )
				{
					if (s_extension[Extension::ARB_instanced_arrays].m_supported
					||  s_extension[Extension::ANGLE_instanced_arrays].m_supported)
					{
						if (NULL != glVertexAttribDivisor
						&&  NULL != glDrawArraysInstanced
						&&  NULL != glDrawElementsInstanced)
						{
							g_caps.supported |= BGFX_CAPS_INSTANCING;
						}
					}
				}

				if (0 == (g_caps.supported & BGFX_CAPS_INSTANCING) )
				{
					glVertexAttribDivisor   = stubVertexAttribDivisor;
					glDrawArraysInstanced   = stubDrawArraysInstanced;
					glDrawElementsInstanced = stubDrawElementsInstanced;
				}
			}

			if (BX_ENABLED(BGFX_CONFIG_RENDERER_OPENGL >= 31)
			||  BX_ENABLED(BGFX_CONFIG_RENDERER_OPENGLES >= 30) )
			{
				s_textureFormat[TextureFormat::R8].m_internalFmt = GL_R8;
				s_textureFormat[TextureFormat::R8].m_fmt         = GL_RED;
			}

#if BGFX_CONFIG_RENDERER_OPENGL
			if (s_extension[Extension::ARB_debug_output].m_supported
			||  s_extension[Extension::KHR_debug].m_supported)
			{
				GL_CHECK(glDebugMessageCallback(debugProcCb, NULL) );
				GL_CHECK(glDebugMessageControl(GL_DONT_CARE, GL_DONT_CARE, GL_DEBUG_SEVERITY_MEDIUM_ARB, 0, NULL, GL_TRUE) );
			}

			if (s_extension[Extension::ARB_seamless_cube_map].m_supported)
			{
				GL_CHECK(glEnable(GL_TEXTURE_CUBE_MAP_SEAMLESS) );
			}

			if (s_extension[Extension::ARB_depth_clamp].m_supported)
			{
				GL_CHECK(glEnable(GL_DEPTH_CLAMP) );
			}
#endif // BGFX_CONFIG_RENDERER_OPENGL

			if (NULL == glFrameTerminatorGREMEDY
			||  !s_extension[Extension::GREMEDY_frame_terminator].m_supported)
			{
				glFrameTerminatorGREMEDY = stubFrameTerminatorGREMEDY;
			}

			if (NULL == glInsertEventMarker
			||  !s_extension[Extension::EXT_debug_marker].m_supported)
			{
				glInsertEventMarker = (NULL != glStringMarkerGREMEDY && s_extension[Extension::GREMEDY_string_marker].m_supported)
					? stubInsertEventMarkerGREMEDY
					: stubInsertEventMarker
					;
			}

			if (NULL == glObjectLabel)
			{
				glObjectLabel = stubObjectLabel;
			}

			if (BX_ENABLED(BGFX_CONFIG_RENDERER_OPENGL) )
			{
				m_queries.create();
			}
		}

		void shutdown()
		{
			captureFinish();

			invalidateCache();

			if (BX_ENABLED(BGFX_CONFIG_RENDERER_OPENGL) )
			{
				m_queries.destroy();
			}

			destroyMsaaFbo();
			m_glctx.destroy();

			m_flip = false;
		}

		IndexBuffer m_indexBuffers[BGFX_CONFIG_MAX_INDEX_BUFFERS];
		VertexBuffer m_vertexBuffers[BGFX_CONFIG_MAX_VERTEX_BUFFERS];
		Shader m_shaders[BGFX_CONFIG_MAX_SHADERS];
		Program m_program[BGFX_CONFIG_MAX_PROGRAMS];
		Texture m_textures[BGFX_CONFIG_MAX_TEXTURES];
		VertexDecl m_vertexDecls[BGFX_CONFIG_MAX_VERTEX_DECLS];
		FrameBuffer m_frameBuffers[BGFX_CONFIG_MAX_FRAME_BUFFERS];
		UniformRegistry m_uniformReg;
		void* m_uniforms[BGFX_CONFIG_MAX_UNIFORMS];
		Queries m_queries;

		VaoStateCache m_vaoStateCache;
		SamplerStateCache m_samplerStateCache;

		TextVideoMem m_textVideoMem;
		bool m_rtMsaa;

		FrameBufferHandle m_fbh;

		Resolution m_resolution;
		void* m_capture;
		uint32_t m_captureSize;
		float m_maxAnisotropy;
		int32_t m_maxMsaa;
		GLuint m_vao;
		bool m_vaoSupport;
		bool m_samplerObjectSupport;
		bool m_shadowSamplersSupport;
		bool m_programBinarySupport;
		bool m_textureSwizzleSupport;
		bool m_depthTextureSupport;
		bool m_useClearQuad;
		bool m_flip;

		uint64_t m_hash;

		GLenum m_readPixelsFmt;
		GLuint m_backBufferFbo;
		GLuint m_msaaBackBufferFbo;
		GLuint m_msaaBackBufferRbos[2];
		GlContext m_glctx;

		const char* m_vendor;
		const char* m_renderer;
		const char* m_version;
		const char* m_glslVersion;
	};

	RendererContext* s_renderCtx;

	const char* glslTypeName(GLuint _type)
	{
#define GLSL_TYPE(_ty) case _ty: return #_ty

		switch (_type)
		{
			GLSL_TYPE(GL_FLOAT);
			GLSL_TYPE(GL_FLOAT_VEC2);
			GLSL_TYPE(GL_FLOAT_VEC3);
			GLSL_TYPE(GL_FLOAT_VEC4);
			GLSL_TYPE(GL_FLOAT_MAT2);
			GLSL_TYPE(GL_FLOAT_MAT3);
			GLSL_TYPE(GL_FLOAT_MAT4);
// 			GLSL_TYPE(GL_FLOAT_MAT2x3);
// 			GLSL_TYPE(GL_FLOAT_MAT2x4);
// 			GLSL_TYPE(GL_FLOAT_MAT3x2);
// 			GLSL_TYPE(GL_FLOAT_MAT3x4);
// 			GLSL_TYPE(GL_FLOAT_MAT4x2);
// 			GLSL_TYPE(GL_FLOAT_MAT4x3);
// 			GLSL_TYPE(GL_SAMPLER_1D);
 			GLSL_TYPE(GL_SAMPLER_2D);
			GLSL_TYPE(GL_SAMPLER_3D);
			GLSL_TYPE(GL_SAMPLER_CUBE);
// 			GLSL_TYPE(GL_SAMPLER_1D_SHADOW);
			GLSL_TYPE(GL_SAMPLER_2D_SHADOW);
		}

#undef GLSL_TYPE

		return "UNKNOWN GLSL TYPE!";
	}

	const char* glEnumName(GLenum _enum)
	{
#define GLENUM(_ty) case _ty: return #_ty

		switch (_enum)
		{
			GLENUM(GL_TEXTURE);
			GLENUM(GL_RENDERBUFFER);

			GLENUM(GL_INVALID_ENUM);
			GLENUM(GL_INVALID_VALUE);
			GLENUM(GL_INVALID_OPERATION);
			GLENUM(GL_OUT_OF_MEMORY);

			GLENUM(GL_FRAMEBUFFER_INCOMPLETE_ATTACHMENT);
			GLENUM(GL_FRAMEBUFFER_INCOMPLETE_MISSING_ATTACHMENT);
//			GLENUM(GL_FRAMEBUFFER_INCOMPLETE_DRAW_BUFFER);
//			GLENUM(GL_FRAMEBUFFER_INCOMPLETE_READ_BUFFER);
			GLENUM(GL_FRAMEBUFFER_UNSUPPORTED);
		}

#undef GLENUM

		return "<GLenum?>";
	}

	UniformType::Enum convertGlType(GLenum _type)
	{
		switch (_type)
		{
		case GL_FLOAT:
			return UniformType::Uniform1fv;

		case GL_FLOAT_VEC2:
			return UniformType::Uniform2fv;

		case GL_FLOAT_VEC3:
			return UniformType::Uniform3fv;

		case GL_FLOAT_VEC4:
			return UniformType::Uniform4fv;

		case GL_FLOAT_MAT2:
			break;

		case GL_FLOAT_MAT3:
			return UniformType::Uniform3x3fv;

		case GL_FLOAT_MAT4:
			return UniformType::Uniform4x4fv;

// 		case GL_FLOAT_MAT2x3:
// 		case GL_FLOAT_MAT2x4:
// 		case GL_FLOAT_MAT3x2:
// 		case GL_FLOAT_MAT3x4:
// 		case GL_FLOAT_MAT4x2:
// 		case GL_FLOAT_MAT4x3:
// 			break;

// 		case GL_SAMPLER_1D:
		case GL_SAMPLER_2D:
		case GL_SAMPLER_3D:
		case GL_SAMPLER_CUBE:
// 		case GL_SAMPLER_1D_SHADOW:
 		case GL_SAMPLER_2D_SHADOW:
			return UniformType::Uniform1iv;
		};

		return UniformType::End;
	}

	void Program::create(const Shader& _vsh, const Shader& _fsh)
	{
		m_id = glCreateProgram();
		BX_TRACE("program create: %d: %d, %d", m_id, _vsh.m_id, _fsh.m_id);

		bool cached = false;

		uint64_t id = (uint64_t(_vsh.m_hash)<<32) | _fsh.m_hash;
		id ^= s_renderCtx->m_hash;

		if (s_renderCtx->m_programBinarySupport)
		{
			uint32_t length = g_callback->cacheReadSize(id);
			cached = length > 0;

			if (cached)
			{
				void* data = BX_ALLOC(g_allocator, length);
				if (g_callback->cacheRead(id, data, length) )
				{
					bx::MemoryReader reader(data, length);

					GLenum format;
					bx::read(&reader, format);

					GL_CHECK(glProgramBinary(m_id, format, reader.getDataPtr(), (GLsizei)reader.remaining() ) );
				}

				BX_FREE(g_allocator, data);
			}

#if BGFX_CONFIG_RENDERER_OPENGL
			GL_CHECK(glProgramParameteri(m_id, GL_PROGRAM_BINARY_RETRIEVABLE_HINT, GL_TRUE) );
#endif // BGFX_CONFIG_RENDERER_OPENGL
		}

		if (!cached)
		{
			GL_CHECK(glAttachShader(m_id, _vsh.m_id) );
			GL_CHECK(glAttachShader(m_id, _fsh.m_id) );
			GL_CHECK(glLinkProgram(m_id) );

			GLint linked = 0;
			GL_CHECK(glGetProgramiv(m_id, GL_LINK_STATUS, &linked) );

			if (0 == linked)
			{
				char log[1024];
				GL_CHECK(glGetProgramInfoLog(m_id, sizeof(log), NULL, log) );
				BX_TRACE("%d: %s", linked, log);

				GL_CHECK(glDeleteProgram(m_id) );
				return;
			}

			if (s_renderCtx->m_programBinarySupport)
			{
				GLint programLength;
				GLenum format;
				GL_CHECK(glGetProgramiv(m_id, GL_PROGRAM_BINARY_LENGTH, &programLength) );

				if (0 < programLength)
				{
					uint32_t length = programLength + 4;
					uint8_t* data = (uint8_t*)BX_ALLOC(g_allocator, length);
					GL_CHECK(glGetProgramBinary(m_id, programLength, NULL, &format, &data[4]) );
					*(uint32_t*)data = format;

					g_callback->cacheWrite(id, data, length);

					BX_FREE(g_allocator, data);
				}
			}
		}

		init();

		if (!cached)
		{
			// Must be after init, otherwise init might fail to lookup shader
			// info (NVIDIA Tegra 3 OpenGL ES 2.0 14.01003).
			GL_CHECK(glDetachShader(m_id, _vsh.m_id) );
			GL_CHECK(glDetachShader(m_id, _fsh.m_id) );
		}
	}

	void Program::destroy()
	{
		if (NULL != m_constantBuffer)
		{
			ConstantBuffer::destroy(m_constantBuffer);
			m_constantBuffer = NULL;
		}
		m_numPredefined = 0;

		if (0 != m_id)
		{
			GL_CHECK(glUseProgram(0) );
			GL_CHECK(glDeleteProgram(m_id) );
			m_id = 0;
		}

		m_vcref.invalidate(s_renderCtx->m_vaoStateCache);
	}

	void Program::init()
	{
		GLint activeAttribs;
		GLint activeUniforms;

#if BGFX_CONFIG_RENDERER_OPENGL >= 31
		GL_CHECK(glBindFragDataLocation(m_id, 0, "bgfx_FragColor") );
#endif // BGFX_CONFIG_RENDERER_OPENGL >= 31

		GL_CHECK(glGetProgramiv(m_id, GL_ACTIVE_ATTRIBUTES, &activeAttribs) );
		GL_CHECK(glGetProgramiv(m_id, GL_ACTIVE_UNIFORMS, &activeUniforms) );

		GLint max0, max1;
		GL_CHECK(glGetProgramiv(m_id, GL_ACTIVE_ATTRIBUTE_MAX_LENGTH, &max0) );
		GL_CHECK(glGetProgramiv(m_id, GL_ACTIVE_UNIFORM_MAX_LENGTH, &max1) );
		uint32_t maxLength = bx::uint32_max(max0, max1);
		char* name = (char*)alloca(maxLength + 1);

		BX_TRACE("Program %d", m_id);
		BX_TRACE("Attributes (%d):", activeAttribs);
		for (int32_t ii = 0; ii < activeAttribs; ++ii)
		{
			GLint size;
			GLenum type;

			GL_CHECK(glGetActiveAttrib(m_id, ii, maxLength + 1, NULL, &size, &type, name) );

			BX_TRACE("\t%s %s is at location %d"
				, glslTypeName(type)
				, name
				, glGetAttribLocation(m_id, name)
				);
		}

		m_numPredefined = 0;
		m_constantBuffer = ConstantBuffer::create(1024);
 		m_numSamplers = 0;

		BX_TRACE("Uniforms (%d):", activeUniforms);
		for (int32_t ii = 0; ii < activeUniforms; ++ii)
		{
			GLint num;
			GLenum gltype;

			GL_CHECK(glGetActiveUniform(m_id, ii, maxLength + 1, NULL, &num, &gltype, name) );
			GLint loc = glGetUniformLocation(m_id, name);

			int offset = 0;
			char* array = strchr(name, '[');
			if (NULL != array)
			{
				BX_TRACE("--- %s", name);
				*array = '\0';
				array++;
				char* end = strchr(array, ']');
				*end = '\0';
				offset = atoi(array);
			}

			switch (gltype)
			{
			case GL_SAMPLER_2D:
			case GL_SAMPLER_3D:
			case GL_SAMPLER_CUBE:
			case GL_SAMPLER_2D_SHADOW:
				BX_TRACE("Sampler %d at %d.", m_numSamplers, loc);
				m_sampler[m_numSamplers] = loc;
				m_numSamplers++;
				break;

			default:
				break;
			}

			PredefinedUniform::Enum predefined = nameToPredefinedUniformEnum(name);
			if (PredefinedUniform::Count != predefined)
			{
				m_predefined[m_numPredefined].m_loc = loc;
				m_predefined[m_numPredefined].m_type = predefined;
				m_predefined[m_numPredefined].m_count = num;
				m_numPredefined++;
			}
			else
			{
				const UniformInfo* info = s_renderCtx->m_uniformReg.find(name);
				if (NULL != info)
				{
					UniformType::Enum type = convertGlType(gltype);
					m_constantBuffer->writeUniformHandle(type, 0, info->m_handle, num);
					m_constantBuffer->write(loc);
					BX_TRACE("store %s %d", name, info->m_handle);
				}
			}

			BX_TRACE("\tuniform %s %s%s is at location %d, size %d, offset %d"
				, glslTypeName(gltype)
				, name
				, PredefinedUniform::Count != predefined ? "*" : ""
				, loc
				, num
				, offset
				);
			BX_UNUSED(offset);
		}

		m_constantBuffer->finish();

		memset(m_attributes, 0xff, sizeof(m_attributes) );
		uint32_t used = 0;
		for (uint32_t ii = 0; ii < Attrib::Count; ++ii)
		{
			GLint loc = glGetAttribLocation(m_id, s_attribName[ii]);
			if (-1 != loc)
			{
				BX_TRACE("attr %s: %d", s_attribName[ii], loc);
				m_attributes[ii] = loc;
				m_used[used++] = ii;
			}
		}
		m_used[used] = Attrib::Count;

		used = 0;
		for (uint32_t ii = 0; ii < BX_COUNTOF(s_instanceDataName); ++ii)
		{
			GLuint loc = glGetAttribLocation(m_id, s_instanceDataName[ii]);
			if (GLuint(-1) != loc )
			{
				BX_TRACE("instance data %s: %d", s_instanceDataName[ii], loc);
				m_instanceData[used++] = loc;
			}
		}
		m_instanceData[used] = 0xffff;
	}

	void Program::bindAttributes(const VertexDecl& _vertexDecl, uint32_t _baseVertex) const
	{
		for (uint32_t ii = 0; Attrib::Count != m_used[ii]; ++ii)
		{
			Attrib::Enum attr = Attrib::Enum(m_used[ii]);
			GLint loc = m_attributes[attr];

			uint8_t num;
			AttribType::Enum type;
			bool normalized;
			bool asInt;
			_vertexDecl.decode(attr, num, type, normalized, asInt);

			if (-1 != loc)
			{
				if (0xff != _vertexDecl.m_attributes[attr])
				{
					GL_CHECK(glEnableVertexAttribArray(loc) );
					GL_CHECK(glVertexAttribDivisor(loc, 0) );

					uint32_t baseVertex = _baseVertex*_vertexDecl.m_stride + _vertexDecl.m_offset[attr];
					GL_CHECK(glVertexAttribPointer(loc, num, s_attribType[type], normalized, _vertexDecl.m_stride, (void*)(uintptr_t)baseVertex) );
				}
				else
				{
					GL_CHECK(glDisableVertexAttribArray(loc) );
				}
			}
		}
	}

	void Program::bindInstanceData(uint32_t _stride, uint32_t _baseVertex) const
	{
		uint32_t baseVertex = _baseVertex;
		for (uint32_t ii = 0; 0xffff != m_instanceData[ii]; ++ii)
		{
			GLint loc = m_instanceData[ii];
			GL_CHECK(glEnableVertexAttribArray(loc) );
			GL_CHECK(glVertexAttribPointer(loc, 4, GL_FLOAT, GL_FALSE, _stride, (void*)(uintptr_t)baseVertex) );
			GL_CHECK(glVertexAttribDivisor(loc, 1) );
			baseVertex += 16;
		}
	}

	void IndexBuffer::destroy()
	{
		GL_CHECK(glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0) );
		GL_CHECK(glDeleteBuffers(1, &m_id) );

		m_vcref.invalidate(s_renderCtx->m_vaoStateCache);
	}

	void VertexBuffer::destroy()
	{
		GL_CHECK(glBindBuffer(GL_ARRAY_BUFFER, 0) );
		GL_CHECK(glDeleteBuffers(1, &m_id) );

		m_vcref.invalidate(s_renderCtx->m_vaoStateCache);
	}

	static void texImage(GLenum _target, GLint _level, GLint _internalFormat, GLsizei _width, GLsizei _height, GLsizei _depth, GLint _border, GLenum _format, GLenum _type, const GLvoid* _data)
	{
		if (_target == GL_TEXTURE_3D)
		{
			GL_CHECK(glTexImage3D(_target, _level, _internalFormat, _width, _height, _depth, _border, _format, _type, _data) );
		}
		else
		{
			BX_UNUSED(_depth);
			GL_CHECK(glTexImage2D(_target, _level, _internalFormat, _width, _height, _border, _format, _type, _data) );
		}
	}

	static void texSubImage(GLenum _target, GLint _level, GLint _xoffset, GLint _yoffset, GLint _zoffset, GLsizei _width, GLsizei _height, GLsizei _depth, GLenum _format, GLenum _type, const GLvoid* _data)
	{
		if (_target == GL_TEXTURE_3D)
		{
			GL_CHECK(glTexSubImage3D(_target, _level, _xoffset, _yoffset, _zoffset, _width, _height, _depth, _format, _type, _data) );
		}
		else
		{
			BX_UNUSED(_zoffset, _depth);
			GL_CHECK(glTexSubImage2D(_target, _level, _xoffset, _yoffset, _width, _height, _format, _type, _data) );
		}
	}

	static void compressedTexImage(GLenum _target, GLint _level, GLenum _internalformat, GLsizei _width, GLsizei _height, GLsizei _depth, GLint _border, GLsizei _imageSize, const GLvoid* _data)
	{
		if (_target == GL_TEXTURE_3D)
		{
			GL_CHECK(glCompressedTexImage3D(_target, _level, _internalformat, _width, _height, _depth, _border, _imageSize, _data) );
		}
		else
		{
			BX_UNUSED(_depth);
			GL_CHECK(glCompressedTexImage2D(_target, _level, _internalformat, _width, _height, _border, _imageSize, _data) );
		}
	}

	static void compressedTexSubImage(GLenum _target, GLint _level, GLint _xoffset, GLint _yoffset, GLint _zoffset, GLsizei _width, GLsizei _height, GLsizei _depth, GLenum _format, GLsizei _imageSize, const GLvoid* _data)
	{
		if (_target == GL_TEXTURE_3D)
		{
			GL_CHECK(glCompressedTexSubImage3D(_target, _level, _xoffset, _yoffset, _zoffset, _width, _height, _depth, _format, _imageSize, _data) );
		}
		else
		{
			BX_UNUSED(_zoffset, _depth);
			GL_CHECK(glCompressedTexSubImage2D(_target, _level, _xoffset, _yoffset, _width, _height, _format, _imageSize, _data) );
		}
	}

	bool Texture::init(GLenum _target, uint32_t _width, uint32_t _height, uint8_t _format, uint8_t _numMips, uint32_t _flags)
	{
		m_target = _target;
		m_numMips = _numMips;
		m_flags = _flags;
		m_currentFlags = UINT32_MAX;
		m_width = _width;
		m_height = _height;
		m_requestedFormat = _format;
		m_textureFormat   = _format;

		const bool bufferOnly = 0 != (m_flags&BGFX_TEXTURE_RT_BUFFER_ONLY);

		if (!bufferOnly)
		{
			GL_CHECK(glGenTextures(1, &m_id) );
			BX_CHECK(0 != m_id, "Failed to generate texture id.");
			GL_CHECK(glBindTexture(_target, m_id) );

			setSamplerState(_flags);

			const TextureFormatInfo& tfi = s_textureFormat[_format];
			m_fmt = tfi.m_fmt;
			m_type = tfi.m_type;

			const bool compressed = isCompressed(TextureFormat::Enum(_format) );
			const bool decompress = !tfi.m_supported && compressed;

			if (decompress)
			{
				m_textureFormat = (uint8_t)TextureFormat::BGRA8;
				const TextureFormatInfo& tfi = s_textureFormat[TextureFormat::BGRA8];
				m_fmt = tfi.m_fmt;
				m_type = tfi.m_type;
			}

			if (BX_ENABLED(BGFX_CONFIG_RENDERER_OPENGL)
			&&  TextureFormat::BGRA8 == m_textureFormat
			&&  GL_RGBA == m_fmt
			&&  s_renderCtx->m_textureSwizzleSupport)
			{
				GLint swizzleMask[] = { GL_BLUE, GL_GREEN, GL_RED, GL_ALPHA };
				GL_CHECK(glTexParameteriv(GL_TEXTURE_2D, GL_TEXTURE_SWIZZLE_RGBA, swizzleMask) );
			}
		}

		const bool renderTarget = 0 != (m_flags&BGFX_TEXTURE_RT_MASK);

		if (renderTarget)
		{
			uint32_t msaaQuality = ( (m_flags&BGFX_TEXTURE_RT_MSAA_MASK)>>BGFX_TEXTURE_RT_MSAA_SHIFT);
			msaaQuality = bx::uint32_satsub(msaaQuality, 1);
			msaaQuality = bx::uint32_min(s_renderCtx->m_maxMsaa, msaaQuality == 0 ? 0 : 1<<msaaQuality);

			if (0 != msaaQuality
			||  bufferOnly)
			{
				GL_CHECK(glGenRenderbuffers(1, &m_rbo) );
				BX_CHECK(0 != m_rbo, "Failed to generate renderbuffer id.");
				GL_CHECK(glBindRenderbuffer(GL_RENDERBUFFER, m_rbo) );

				if (0 == msaaQuality)
				{
					GL_CHECK(glRenderbufferStorage(GL_RENDERBUFFER
						, s_textureFormat[m_textureFormat].m_internalFmt
						, _width
						, _height
						) );
				}
				else if (BX_ENABLED(BGFX_CONFIG_RENDERER_OPENGL || BGFX_CONFIG_RENDERER_OPENGLES >= 30) )
				{
					GL_CHECK(glRenderbufferStorageMultisample(GL_RENDERBUFFER
						, msaaQuality
						, s_textureFormat[m_textureFormat].m_internalFmt
						, _width
						, _height
						) );
				}

				GL_CHECK(glBindRenderbuffer(GL_RENDERBUFFER, 0) );

				if (bufferOnly)
				{
					// This is render buffer, there is no sampling, no need
					// to create texture.
					return false;
				}
			}
		}

		return true;
	}

	void Texture::create(const Memory* _mem, uint32_t _flags, uint8_t _skip)
	{
		ImageContainer imageContainer;

		if (imageParse(imageContainer, _mem->data, _mem->size) )
		{
			uint8_t numMips = imageContainer.m_numMips;
			const uint32_t startLod = bx::uint32_min(_skip, numMips-1);
			numMips -= startLod;
			const ImageBlockInfo& blockInfo = getBlockInfo(TextureFormat::Enum(imageContainer.m_format) );
			const uint32_t textureWidth  = bx::uint32_max(blockInfo.blockWidth,  imageContainer.m_width >>startLod);
			const uint32_t textureHeight = bx::uint32_max(blockInfo.blockHeight, imageContainer.m_height>>startLod);

			GLenum target = GL_TEXTURE_2D;
			if (imageContainer.m_cubeMap)
			{
				target = GL_TEXTURE_CUBE_MAP;
			}
			else if (imageContainer.m_depth > 1)
			{
				target = GL_TEXTURE_3D;
			}

			if (!init(target
					, textureWidth
					, textureHeight
					, imageContainer.m_format
					, numMips
					, _flags
					) )
			{
				return;
			}

			target = GL_TEXTURE_CUBE_MAP == m_target ? GL_TEXTURE_CUBE_MAP_POSITIVE_X : m_target;

			const GLenum internalFmt = s_textureFormat[m_textureFormat].m_internalFmt;

			const bool swizzle = true
				&& TextureFormat::BGRA8 == m_textureFormat
				&& GL_RGBA == m_fmt
				&& !s_renderCtx->m_textureSwizzleSupport
				;
			const bool convert    = m_textureFormat != m_requestedFormat;
			const bool compressed = isCompressed(TextureFormat::Enum(m_textureFormat) );
			uint32_t blockWidth  = 1;
			uint32_t blockHeight = 1;
			
			if (convert && compressed)
			{
				blockWidth  = blockInfo.blockWidth;
				blockHeight = blockInfo.blockHeight;
			}

			BX_WARN(!swizzle && !convert, "Texture %s%s%s from %s to %s."
					, swizzle ? "swizzle" : ""
					, swizzle&&convert ? " and " : ""
					, convert ? "convert" : ""
					, getName( (TextureFormat::Enum)m_requestedFormat)
					, getName( (TextureFormat::Enum)m_textureFormat)
					);

			uint8_t* temp = NULL;
			if (convert || swizzle)
			{
				temp = (uint8_t*)BX_ALLOC(g_allocator, textureWidth*textureHeight*4);
			}

			for (uint8_t side = 0, numSides = imageContainer.m_cubeMap ? 6 : 1; side < numSides; ++side)
			{
				uint32_t width  = textureWidth;
				uint32_t height = textureHeight;
				uint32_t depth  = imageContainer.m_depth;

				for (uint32_t lod = 0, num = numMips; lod < num; ++lod)
				{
					width  = bx::uint32_max(blockWidth,  width);
					height = bx::uint32_max(blockHeight, height);
					depth  = bx::uint32_max(1, depth);

					ImageMip mip;
					if (imageGetRawData(imageContainer, side, lod+startLod, _mem->data, _mem->size, mip) )
					{
						if (compressed)
						{
							compressedTexImage(target+side
								, lod
								, internalFmt
								, width
								, height
								, depth
								, 0
								, mip.m_size
								, mip.m_data
								);
						}
						else
						{
							const uint8_t* data = mip.m_data;

							if (convert)
							{
								imageDecodeToBgra8(temp, mip.m_data, mip.m_width, mip.m_height, mip.m_width*4, mip.m_format);
								data = temp;
							}

							if (swizzle)
							{
								imageSwizzleBgra8(width, height, mip.m_width*4, data, temp);
								data = temp;
							}

							texImage(target+side
								, lod
								, internalFmt
								, width
								, height
								, depth
								, 0
								, m_fmt
								, m_type
								, data
								);
						}
					}
					else
					{
						if (compressed)
						{
							uint32_t size = bx::uint32_max(1, (width  + 3)>>2)
										  * bx::uint32_max(1, (height + 3)>>2)
										  * 4*4*getBitsPerPixel(TextureFormat::Enum(m_textureFormat) )/8
										  ;

							compressedTexImage(target+side
								, lod
								, internalFmt
								, width
								, height
								, depth
								, 0
								, size
								, NULL
								);
						}
						else
						{
							texImage(target+side
								, lod
								, internalFmt
								, width
								, height
								, depth
								, 0
								, m_fmt
								, m_type
								, NULL
								);
						}
					}

					width  >>= 1;
					height >>= 1;
					depth  >>= 1;
				}
			}

			if (NULL != temp)
			{
				BX_FREE(g_allocator, temp);
			}
		}

		GL_CHECK(glBindTexture(m_target, 0) );
	}

	void Texture::destroy()
	{
		if (0 != m_id)
		{
			GL_CHECK(glBindTexture(m_target, 0) );
			GL_CHECK(glDeleteTextures(1, &m_id) );
			m_id = 0;
		}

		if (0 != m_rbo)
		{
			GL_CHECK(glDeleteRenderbuffers(1, &m_rbo) );
			m_rbo = 0;
		}
	}

	void Texture::update(uint8_t _side, uint8_t _mip, const Rect& _rect, uint16_t _z, uint16_t _depth, uint16_t _pitch, const Memory* _mem)
	{
		BX_UNUSED(_z, _depth);

		const uint32_t bpp = getBitsPerPixel(TextureFormat::Enum(m_textureFormat) );
		const uint32_t rectpitch = _rect.m_width*bpp/8;
		uint32_t srcpitch  = UINT16_MAX == _pitch ? rectpitch : _pitch;

		GL_CHECK(glBindTexture(m_target, m_id) );
		GL_CHECK(glPixelStorei(GL_UNPACK_ALIGNMENT, 1) );

		GLenum target = GL_TEXTURE_CUBE_MAP == m_target ? GL_TEXTURE_CUBE_MAP_POSITIVE_X : m_target;

		const bool swizzle = true
			&& TextureFormat::BGRA8 == m_textureFormat
			&& GL_RGBA == m_fmt
			&& !s_renderCtx->m_textureSwizzleSupport
			;
		const bool unpackRowLength = !!BGFX_CONFIG_RENDERER_OPENGL || s_extension[Extension::EXT_unpack_subimage].m_supported;
		const bool convert         = m_textureFormat != m_requestedFormat;
		const bool compressed      = isCompressed(TextureFormat::Enum(m_textureFormat) );

		const uint32_t width  = _rect.m_width;
		const uint32_t height = _rect.m_height;

		uint8_t* temp = NULL;
		if (convert
		||  swizzle
		||  !unpackRowLength)
		{
			temp = (uint8_t*)BX_ALLOC(g_allocator, rectpitch*height);
		}
		else if (unpackRowLength)
		{
			GL_CHECK(glPixelStorei(GL_UNPACK_ROW_LENGTH, srcpitch*8/bpp) );
		}

		if (compressed)
		{
			const uint8_t* data = _mem->data;

			if (!unpackRowLength)
			{
				imageCopy(width, height, bpp, srcpitch, data, temp);
				data = temp;
			}

			GL_CHECK(compressedTexSubImage(target+_side
				, _mip
				, _rect.m_x
				, _rect.m_y
				, _z
				, _rect.m_width
				, _rect.m_height
				, _depth
				, m_fmt
				, _mem->size
				, data
				) );
		}
		else
		{
			const uint8_t* data = _mem->data;

			if (convert)
			{
				imageDecodeToBgra8(temp, data, width, height, srcpitch, m_requestedFormat);
				data = temp;
				srcpitch = rectpitch;
			}

			if (swizzle)
			{
				imageSwizzleBgra8(width, height, srcpitch, data, temp);
				data = temp;
			}
			else if (!unpackRowLength && !convert)
			{
				imageCopy(width, height, bpp, srcpitch, data, temp);
				data = temp;
			}

			GL_CHECK(texSubImage(target+_side
				, _mip
				, _rect.m_x
				, _rect.m_y
				, _z
				, _rect.m_width
				, _rect.m_height
				, _depth
				, m_fmt
				, m_type
				, data
				) );
		}

		if (NULL != temp)
		{
			BX_FREE(g_allocator, temp);
		}
	}

	void Texture::setSamplerState(uint32_t _flags)
	{
		const uint32_t flags = (0 != (BGFX_SAMPLER_DEFAULT_FLAGS & _flags) ? m_flags : _flags) & BGFX_TEXTURE_SAMPLER_BITS_MASK;
		if (flags != m_currentFlags)
		{
			const GLenum target = m_target;
			const uint8_t numMips = m_numMips;

			GL_CHECK(glTexParameteri(target, GL_TEXTURE_WRAP_S, s_textureAddress[(flags&BGFX_TEXTURE_U_MASK)>>BGFX_TEXTURE_U_SHIFT]) );
			GL_CHECK(glTexParameteri(target, GL_TEXTURE_WRAP_T, s_textureAddress[(flags&BGFX_TEXTURE_V_MASK)>>BGFX_TEXTURE_V_SHIFT]) );

			if (BX_ENABLED(BGFX_CONFIG_RENDERER_OPENGL || BGFX_CONFIG_RENDERER_OPENGLES >= 30)
			||  s_extension[Extension::APPLE_texture_max_level].m_supported)
			{
				GL_CHECK(glTexParameteri(target, GL_TEXTURE_MAX_LEVEL, numMips-1) );
			}

			if (target == GL_TEXTURE_3D)
			{
				GL_CHECK(glTexParameteri(target, GL_TEXTURE_WRAP_R, s_textureAddress[(flags&BGFX_TEXTURE_W_MASK)>>BGFX_TEXTURE_W_SHIFT]) );
			}

			const uint32_t mag = (flags&BGFX_TEXTURE_MAG_MASK)>>BGFX_TEXTURE_MAG_SHIFT;
			const uint32_t min = (flags&BGFX_TEXTURE_MIN_MASK)>>BGFX_TEXTURE_MIN_SHIFT;
			const uint32_t mip = (flags&BGFX_TEXTURE_MIP_MASK)>>BGFX_TEXTURE_MIP_SHIFT;
			const GLenum minFilter = s_textureFilterMin[min][1 < numMips ? mip+1 : 0];
			GL_CHECK(glTexParameteri(target, GL_TEXTURE_MAG_FILTER, s_textureFilterMag[mag]) );
			GL_CHECK(glTexParameteri(target, GL_TEXTURE_MIN_FILTER, minFilter) );
			if (0 != (flags & (BGFX_TEXTURE_MIN_ANISOTROPIC|BGFX_TEXTURE_MAG_ANISOTROPIC) )
			&&  0.0f < s_renderCtx->m_maxAnisotropy)
			{
				GL_CHECK(glTexParameterf(target, GL_TEXTURE_MAX_ANISOTROPY_EXT, s_renderCtx->m_maxAnisotropy) );
			}

			if (BX_ENABLED(BGFX_CONFIG_RENDERER_OPENGLES >= 30)
			||  s_renderCtx->m_shadowSamplersSupport)
			{
				const uint32_t cmpFunc = (flags&BGFX_TEXTURE_COMPARE_MASK)>>BGFX_TEXTURE_COMPARE_SHIFT;
				if (0 == cmpFunc)
				{
					GL_CHECK(glTexParameteri(m_target, GL_TEXTURE_COMPARE_MODE, GL_NONE) );
				}
				else
				{
					GL_CHECK(glTexParameteri(m_target, GL_TEXTURE_COMPARE_MODE, GL_COMPARE_REF_TO_TEXTURE) );
					GL_CHECK(glTexParameteri(m_target, GL_TEXTURE_COMPARE_FUNC, s_cmpFunc[cmpFunc]) );
				}
			}

			m_currentFlags = flags;
		}
	}

	void Texture::commit(uint32_t _stage, uint32_t _flags)
	{
		GL_CHECK(glActiveTexture(GL_TEXTURE0+_stage) );
		GL_CHECK(glBindTexture(m_target, m_id) );

		if (BX_ENABLED(BGFX_CONFIG_RENDERER_OPENGLES)
		&&  BX_ENABLED(BGFX_CONFIG_RENDERER_OPENGLES < 30) )
		{
			// GLES2 doesn't have support for sampler object.
			setSamplerState(_flags);
		}
		else if (BX_ENABLED(BGFX_CONFIG_RENDERER_OPENGL)
			 &&  BX_ENABLED(BGFX_CONFIG_RENDERER_OPENGL < 31) )
		{
			// In case that GL 2.1 sampler object is supported via extension.
			if (s_renderCtx->m_samplerObjectSupport)
			{
				s_renderCtx->setSamplerState(_stage, m_numMips, _flags);
			}
			else
			{
				setSamplerState(_flags);
			}
		}
		else
		{
			// Everything else has sampler object.
			s_renderCtx->setSamplerState(_stage, m_numMips, _flags);
		}
	}

	void writeString(bx::WriterI* _writer, const char* _str)
	{
		bx::write(_writer, _str, (int32_t)strlen(_str) );
	}

	void writeStringf(bx::WriterI* _writer, const char* _format, ...)
	{
		char temp[512];

		va_list argList;
		va_start(argList, _format);
		int len = bx::vsnprintf(temp, BX_COUNTOF(temp), _format, argList);
		va_end(argList);

		bx::write(_writer, temp, len);
	}

	void strins(char* _str, const char* _insert)
	{
		size_t len = strlen(_insert);
		memmove(&_str[len], _str, strlen(_str)+1);
		memcpy(_str, _insert, len);
	}

	void Shader::create(Memory* _mem)
	{
		bx::MemoryReader reader(_mem->data, _mem->size);
		m_hash = bx::hashMurmur2A(_mem->data, _mem->size);

		uint32_t magic;
		bx::read(&reader, magic);

		switch (magic)
		{
		case BGFX_CHUNK_MAGIC_FSH:
			m_type = GL_FRAGMENT_SHADER;
			break;

		case BGFX_CHUNK_MAGIC_VSH:
			m_type = GL_VERTEX_SHADER;
			break;

		default:
			BGFX_FATAL(false, Fatal::InvalidShader, "Unknown shader format %x.", magic);
			break;
		}

		uint32_t iohash;
		bx::read(&reader, iohash);

		uint16_t count;
		bx::read(&reader, count);

		for (uint32_t ii = 0; ii < count; ++ii)
		{
			uint8_t nameSize;
			bx::read(&reader, nameSize);

			char name[256];
			bx::read(&reader, &name, nameSize);
			name[nameSize] = '\0';

			uint8_t type;
			bx::read(&reader, type);

			uint8_t num;
			bx::read(&reader, num);

			uint16_t regIndex;
			bx::read(&reader, regIndex);

			uint16_t regCount;
			bx::read(&reader, regCount);
		}

		uint32_t shaderSize;
		bx::read(&reader, shaderSize);

		m_id = glCreateShader(m_type);

		const char* code = (const char*)reader.getDataPtr();

		if (0 != m_id)
		{
			int32_t codeLen = (int32_t)strlen(code);
			int32_t tempLen = codeLen + (4<<10);
			char* temp = (char*)alloca(tempLen);
			bx::StaticMemoryBlockWriter writer(temp, tempLen);

			if (BX_ENABLED(BGFX_CONFIG_RENDERER_OPENGLES)
			&&  BX_ENABLED(BGFX_CONFIG_RENDERER_OPENGLES < 30) )
			{
				bool usesDerivatives = s_extension[Extension::OES_standard_derivatives].m_supported 
					&& bx::findIdentifierMatch(code, s_OES_standard_derivatives)
					;

				bool usesFragDepth = !!bx::findIdentifierMatch(code, "gl_FragDepth");

				bool usesShadowSamplers = !!bx::findIdentifierMatch(code, s_EXT_shadow_samplers);

				bool usesTexture3D = s_extension[Extension::OES_texture_3D].m_supported
					&& bx::findIdentifierMatch(code, s_OES_texture_3D)
					;

				bool usesTextureLod = !!bx::findIdentifierMatch(code, s_EXT_shader_texture_lod);

				if (usesDerivatives)
				{
					writeString(&writer, "#extension GL_OES_standard_derivatives : enable\n");
				}

				bool insertFragDepth = false;
				if (usesFragDepth)
				{
					BX_WARN(s_extension[Extension::EXT_frag_depth].m_supported, "EXT_frag_depth is used but not supported by GLES2 driver.");
					if (s_extension[Extension::EXT_frag_depth].m_supported)
					{
						writeString(&writer
							, "#extension GL_EXT_frag_depth : enable\n"
							  "#define bgfx_FragDepth gl_FragDepthEXT\n"
							);

						char str[128];
						bx::snprintf(str, BX_COUNTOF(str), "%s float gl_FragDepthEXT;\n"
							, s_extension[Extension::OES_fragment_precision_high].m_supported ? "highp" : "mediump"
							);
						writeString(&writer, str);
					}
					else
					{
						insertFragDepth = true;
					}
				}

				if (usesShadowSamplers)
				{
					if (s_renderCtx->m_shadowSamplersSupport)
					{
						writeString(&writer
							, "#extension GL_EXT_shadow_samplers : enable\n"
							  "#define shadow2D shadow2DEXT\n"
							  "#define shadow2DProj shadow2DProjEXT\n"
							);
					}
					else
					{
						writeString(&writer
							, "#define sampler2DShadow sampler2D\n"
							  "#define shadow2D(_sampler, _coord) step(_coord.z, texture2D(_sampler, _coord.xy).x)\n"
							  "#define shadow2DProj(_sampler, _coord) step(_coord.z/_coord.w, texture2DProj(_sampler, _coord).x)\n"
							);
					}
				}

				if (usesTexture3D)
				{
					writeString(&writer, "#extension GL_OES_texture_3D : enable\n");
				}

				if (usesTextureLod)
				{
					BX_WARN(s_extension[Extension::EXT_shader_texture_lod].m_supported, "EXT_shader_texture_lod is used but not supported by GLES2 driver.");
					if (s_extension[Extension::EXT_shader_texture_lod].m_supported)
					{
						writeString(&writer
							, "#extension GL_EXT_shader_texture_lod : enable\n"
							  "#define texture2DLod texture2DLodEXT\n"
							  "#define texture2DProjLod texture2DProjLodEXT\n"
							  "#define textureCubeLod textureCubeLodEXT\n"
							);
					}
					else
					{
						writeString(&writer
							, "#define texture2DLod(_sampler, _coord, _level) texture2D(_sampler, _coord)\n"
							  "#define texture2DProjLod(_sampler, _coord, _level) texture2DProj(_sampler, _coord)\n"
							  "#define textureCubeLod(_sampler, _coord, _level) textureCube(_sampler, _coord)\n"
							);
					}
				}

				writeString(&writer, "precision mediump float;\n");

				bx::write(&writer, code, codeLen);
				bx::write(&writer, '\0');

				if (insertFragDepth)
				{
					char* entry = strstr(temp, "void main ()");
					if (NULL != entry)
					{
						char* brace = strstr(entry, "{");
						if (NULL != brace)
						{
							const char* end = bx::strmb(brace, '{', '}');
							if (NULL != end)
							{
								strins(brace+1, "\n  float bgfx_FragDepth = 0.0;\n");
							}
						}
					}
				}

				// Replace all instances of gl_FragDepth with bgfx_FragDepth.
				for (const char* fragDepth = bx::findIdentifierMatch(temp, "gl_FragDepth"); NULL != fragDepth; fragDepth = bx::findIdentifierMatch(fragDepth, "gl_FragDepth") )
				{
					char* insert = const_cast<char*>(fragDepth);
					strins(insert, "bg");
					memcpy(insert + 2, "fx", 2);
				}
			}
			else if (BX_ENABLED(BGFX_CONFIG_RENDERER_OPENGL)
				 &&  BX_ENABLED(BGFX_CONFIG_RENDERER_OPENGL <= 21) )
			{
				bool usesTextureLod = s_extension[Extension::ARB_shader_texture_lod].m_supported
					&& bx::findIdentifierMatch(code, s_ARB_shader_texture_lod)
					;

				if (usesTextureLod)
				{
					writeString(&writer, "#version 120\n");
					if (m_type == GL_FRAGMENT_SHADER)
					{
						writeString(&writer, "#extension GL_ARB_shader_texture_lod : enable\n");
					}
				}

				writeString(&writer
						, "#define lowp\n"
						  "#define mediump\n"
						  "#define highp\n"
						);

				bx::write(&writer, code, codeLen);
				bx::write(&writer, '\0');
			}
			else if (BX_ENABLED(BGFX_CONFIG_RENDERER_OPENGL >= 31)
				 ||  BX_ENABLED(BGFX_CONFIG_RENDERER_OPENGLES >= 30) )
			{
				if (BX_ENABLED(BGFX_CONFIG_RENDERER_OPENGLES >= 30) )
				{
					writeString(&writer
						, "#version 300 es\n"
						  "precision mediump float;\n"
						);
				}
				else
				{
					writeString(&writer, "#version 140\n");
				}

				if (m_type == GL_FRAGMENT_SHADER)
				{
					writeString(&writer, "#define varying in\n");
					writeString(&writer, "#define texture2D texture\n");
					writeString(&writer, "#define texture2DLod textureLod\n");
					writeString(&writer, "#define texture2DProj textureProj\n");

					if (BX_ENABLED(BGFX_CONFIG_RENDERER_OPENGL) )
					{
						writeString(&writer, "#define shadow2D(_sampler, _coord) vec2(textureProj(_sampler, vec4(_coord, 1.0) ) )\n");
						writeString(&writer, "#define shadow2DProj(_sampler, _coord) vec2(textureProj(_sampler, _coord) ) )\n");
					}
					else
					{
						writeString(&writer, "#define shadow2D(_sampler, _coord) (textureProj(_sampler, vec4(_coord, 1.0) ) )\n");
						writeString(&writer, "#define shadow2DProj(_sampler, _coord) (textureProj(_sampler, _coord) ) )\n");
					}

					writeString(&writer, "#define texture3D texture\n");
					writeString(&writer, "#define texture3DLod textureLod\n");
					writeString(&writer, "#define textureCube texture\n");
					writeString(&writer, "#define textureCubeLod textureLod\n");

					uint32_t fragData = 0;

					if (!!bx::findIdentifierMatch(code, "gl_FragData") )
					{
						using namespace bx;
						fragData = uint32_max(fragData, NULL == strstr(code, "gl_FragData[0]") ? 0 : 1);
						fragData = uint32_max(fragData, NULL == strstr(code, "gl_FragData[1]") ? 0 : 2);
						fragData = uint32_max(fragData, NULL == strstr(code, "gl_FragData[2]") ? 0 : 3);
						fragData = uint32_max(fragData, NULL == strstr(code, "gl_FragData[3]") ? 0 : 4);

						BGFX_FATAL(0 != fragData, Fatal::InvalidShader, "Unable to find and patch gl_FragData!");
					}

					if (0 != fragData)
					{
						writeStringf(&writer, "out vec4 bgfx_FragData[%d];\n", fragData);
						writeString(&writer, "#define gl_FragData bgfx_FragData\n");
					}
					else
					{
						writeString(&writer, "out vec4 bgfx_FragColor;\n");
						writeString(&writer, "#define gl_FragColor bgfx_FragColor\n");
					}
				}
				else
				{
					writeString(&writer, "#define attribute in\n");
					writeString(&writer, "#define varying out\n");
				}

				if (!BX_ENABLED(BGFX_CONFIG_RENDERER_OPENGLES >= 30) )
				{
					writeString(&writer
							, "#define lowp\n"
							  "#define mediump\n"
							  "#define highp\n"
							);
				}

				bx::write(&writer, code, codeLen);
				bx::write(&writer, '\0');
			}

			code = temp;

			GL_CHECK(glShaderSource(m_id, 1, (const GLchar**)&code, NULL) );
			GL_CHECK(glCompileShader(m_id) );

			GLint compiled = 0;
			GL_CHECK(glGetShaderiv(m_id, GL_COMPILE_STATUS, &compiled) );

			if (0 == compiled)
			{
				BX_TRACE("\n####\n%s\n####", code);

				GLsizei len;
				char log[1024];
				GL_CHECK(glGetShaderInfoLog(m_id, sizeof(log), &len, log) );
				BX_TRACE("Failed to compile shader. %d: %s", compiled, log);

				GL_CHECK(glDeleteShader(m_id) );
				BGFX_FATAL(false, bgfx::Fatal::InvalidShader, "Failed to compile shader.");
			}
			else if (BX_ENABLED(BGFX_CONFIG_DEBUG)
				 &&  s_extension[Extension::ANGLE_translated_shader_source].m_supported)
			{
				GLsizei len;
				GL_CHECK(glGetShaderiv(m_id, GL_TRANSLATED_SHADER_SOURCE_LENGTH_ANGLE, &len) );

				char* source = (char*)alloca(len);
				GL_CHECK(glGetTranslatedShaderSourceANGLE(m_id, len, &len, source) );

				BX_TRACE("ANGLE source (len: %d):\n%s\n####", len, source);
			}
		}
	}

	void Shader::destroy()
	{
		if (0 != m_id)
		{
			GL_CHECK(glDeleteShader(m_id) );
			m_id = 0;
		}
	}

	static void frameBufferValidate()
	{
		GLenum complete = glCheckFramebufferStatus(GL_FRAMEBUFFER);
		BX_CHECK(GL_FRAMEBUFFER_COMPLETE == complete
			, "glCheckFramebufferStatus failed 0x%08x: %s"
			, complete
			, glEnumName(complete)
		);
		BX_UNUSED(complete);
	}

	void FrameBuffer::create(uint8_t _num, const TextureHandle* _handles)
	{
		GL_CHECK(glGenFramebuffers(1, &m_fbo[0]) );
		GL_CHECK(glBindFramebuffer(GL_FRAMEBUFFER, m_fbo[0]) );

		bool needResolve = false;

		GLenum buffers[BGFX_CONFIG_MAX_FRAME_BUFFER_ATTACHMENTS];

		uint32_t colorIdx = 0;
		for (uint32_t ii = 0; ii < _num; ++ii)
		{
			TextureHandle handle = _handles[ii];
			if (isValid(handle) )
			{
				const Texture& texture = s_renderCtx->m_textures[handle.idx];

				if (0 == colorIdx)
				{
					m_width  = texture.m_width;
					m_height = texture.m_height;
				}

				GLenum attachment = GL_COLOR_ATTACHMENT0 + colorIdx;
				if (isDepth( (TextureFormat::Enum)texture.m_textureFormat) )
				{
					attachment = GL_DEPTH_ATTACHMENT;
				}
				else
				{
					buffers[colorIdx] = attachment;
					++colorIdx;
				}

				if (0 != texture.m_rbo)
				{
					GL_CHECK(glFramebufferRenderbuffer(GL_FRAMEBUFFER
						, attachment
						, GL_RENDERBUFFER
						, texture.m_rbo
						) );
				}
				else
				{
					GL_CHECK(glFramebufferTexture2D(GL_FRAMEBUFFER
						, attachment
						, texture.m_target
						, texture.m_id
						, 0
						) );
				}
				
				needResolve |= (0 != texture.m_rbo) && (0 != texture.m_id);
			}
		}

		m_num = colorIdx;

		if (BX_ENABLED(BGFX_CONFIG_RENDERER_OPENGL) )
		{
			if (0 == colorIdx)
			{
				// When only depth is attached disable draw buffer to avoid
				// GL_FRAMEBUFFER_INCOMPLETE_DRAW_BUFFER.
				GL_CHECK(glDrawBuffer(GL_NONE) );
			}
			else
			{
				GL_CHECK(glDrawBuffers(colorIdx, buffers) );
			}

			// Disable read buffer to avoid GL_FRAMEBUFFER_INCOMPLETE_READ_BUFFER.
			GL_CHECK(glReadBuffer(GL_NONE) );
		}

		frameBufferValidate();

		if (needResolve)
		{
			GL_CHECK(glGenFramebuffers(1, &m_fbo[1]) );
			GL_CHECK(glBindFramebuffer(GL_FRAMEBUFFER, m_fbo[1]) );

			for (uint32_t ii = 0, colorIdx = 0; ii < _num; ++ii)
			{
				TextureHandle handle = _handles[ii];
				if (isValid(handle) )
				{
					const Texture& texture = s_renderCtx->m_textures[handle.idx];

					if (0 != texture.m_id)
					{
						GLenum attachment = GL_COLOR_ATTACHMENT0 + colorIdx;
						if (!isDepth( (TextureFormat::Enum)texture.m_textureFormat) )
						{
							++colorIdx;
							GL_CHECK(glFramebufferTexture2D(GL_FRAMEBUFFER
								, attachment
								, texture.m_target
								, texture.m_id
								, 0
								) );
						}
					}
				}
			}

			frameBufferValidate();
		}

		GL_CHECK(glBindFramebuffer(GL_FRAMEBUFFER, s_renderCtx->m_msaaBackBufferFbo) );
	}

	void FrameBuffer::destroy()
	{
		GL_CHECK(glDeleteFramebuffers(0 == m_fbo[1] ? 1 : 2, m_fbo) );
		memset(m_fbo, 0, sizeof(m_fbo) );
		m_num = 0;
	}

	void FrameBuffer::resolve()
	{
		if (0 != m_fbo[1])
		{
			GL_CHECK(glBindFramebuffer(GL_READ_FRAMEBUFFER, m_fbo[0]) );
			GL_CHECK(glReadBuffer(GL_COLOR_ATTACHMENT0) );
			GL_CHECK(glBindFramebuffer(GL_DRAW_FRAMEBUFFER, m_fbo[1]) );
			GL_CHECK(glBlitFramebuffer(0
				, 0
				, m_width
				, m_height
				, 0
				, 0
				, m_width
				, m_height
				, GL_COLOR_BUFFER_BIT
				, GL_LINEAR
				) );
			GL_CHECK(glBindFramebuffer(GL_READ_FRAMEBUFFER, m_fbo[0]) );
			GL_CHECK(glReadBuffer(GL_NONE) );
			GL_CHECK(glBindFramebuffer(GL_FRAMEBUFFER, s_renderCtx->m_msaaBackBufferFbo) );
		}
	}

	void ConstantBuffer::commit()
	{
		reset();

		do
		{
			uint32_t opcode = read();

			if (UniformType::End == opcode)
			{
				break;
			}

			UniformType::Enum type;
			uint16_t ignore;
			uint16_t num;
			uint16_t copy;
			decodeOpcode(opcode, type, ignore, num, copy);

			const char* data;
			if (copy)
			{
				data = read(g_uniformTypeSize[type]*num);
			}
			else
			{
				UniformHandle handle;
				memcpy(&handle, read(sizeof(UniformHandle) ), sizeof(UniformHandle) );
				data = (const char*)s_renderCtx->m_uniforms[handle.idx];
//				memcpy(&data, read(sizeof(void*) ), sizeof(void*) );
			}

			uint32_t loc = read();

#define CASE_IMPLEMENT_UNIFORM(_uniform, _glsuffix, _dxsuffix, _type) \
		case UniformType::_uniform: \
			{ \
				_type* value = (_type*)data; \
				GL_CHECK(glUniform##_glsuffix(loc, num, value) ); \
			} \
			break;

#define CASE_IMPLEMENT_UNIFORM_T(_uniform, _glsuffix, _dxsuffix, _type) \
		case UniformType::_uniform: \
			{ \
				_type* value = (_type*)data; \
				GL_CHECK(glUniform##_glsuffix(loc, num, GL_FALSE, value) ); \
			} \
			break;

			switch (type)
			{
// 			case ConstantType::Uniform1iv:
// 				{
// 					int* value = (int*)data;
// 					BX_TRACE("Uniform1iv sampler %d, loc %d (num %d, copy %d)", *value, loc, num, copy);
// 					GL_CHECK(glUniform1iv(loc, num, value) );
// 				}
// 				break;

			CASE_IMPLEMENT_UNIFORM(Uniform1i, 1iv, I, int);
			CASE_IMPLEMENT_UNIFORM(Uniform1f, 1fv, F, float);
			CASE_IMPLEMENT_UNIFORM(Uniform1iv, 1iv, I, int);
			CASE_IMPLEMENT_UNIFORM(Uniform1fv, 1fv, F, float);
			CASE_IMPLEMENT_UNIFORM(Uniform2fv, 2fv, F, float);
			CASE_IMPLEMENT_UNIFORM(Uniform3fv, 3fv, F, float);
			CASE_IMPLEMENT_UNIFORM(Uniform4fv, 4fv, F, float);
			CASE_IMPLEMENT_UNIFORM_T(Uniform3x3fv, Matrix3fv, F, float);
			CASE_IMPLEMENT_UNIFORM_T(Uniform4x4fv, Matrix4fv, F, float);

			case UniformType::End:
				break;

			default:
				BX_TRACE("%4d: INVALID 0x%08x, t %d, l %d, n %d, c %d", m_pos, opcode, type, loc, num, copy);
				break;
			}

#undef CASE_IMPLEMENT_UNIFORM
#undef CASE_IMPLEMENT_UNIFORM_T

		} while (true);
	}

	void TextVideoMemBlitter::setup()
	{
		if (0 != s_renderCtx->m_vao)
		{
			GL_CHECK(glBindVertexArray(s_renderCtx->m_vao) );
		}

		uint32_t width = s_renderCtx->m_resolution.m_width;
		uint32_t height = s_renderCtx->m_resolution.m_height;

		GL_CHECK(glBindFramebuffer(GL_FRAMEBUFFER, s_renderCtx->m_backBufferFbo) );
		GL_CHECK(glViewport(0, 0, width, height) );

		GL_CHECK(glDisable(GL_SCISSOR_TEST) );
		GL_CHECK(glDisable(GL_STENCIL_TEST) );
		GL_CHECK(glDisable(GL_DEPTH_TEST) );
		GL_CHECK(glDepthFunc(GL_ALWAYS) );
		GL_CHECK(glDisable(GL_CULL_FACE) );
		GL_CHECK(glDisable(GL_BLEND) );
		GL_CHECK(glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE) );

		Program& program = s_renderCtx->m_program[m_program.idx];
		GL_CHECK(glUseProgram(program.m_id) );
		GL_CHECK(glUniform1i(program.m_sampler[0], 0) );

		float proj[16];
		mtxOrtho(proj, 0.0f, (float)width, (float)height, 0.0f, 0.0f, 1000.0f);

		GL_CHECK(glUniformMatrix4fv(program.m_predefined[0].m_loc
			, 1
			, GL_FALSE
			, proj
			) );

		GL_CHECK(glActiveTexture(GL_TEXTURE0) );
		GL_CHECK(glBindTexture(GL_TEXTURE_2D, s_renderCtx->m_textures[m_texture.idx].m_id) );
	}

	void TextVideoMemBlitter::render(uint32_t _numIndices)
	{
		uint32_t numVertices = _numIndices*4/6;
		s_renderCtx->m_indexBuffers[m_ib->handle.idx].update(0, _numIndices*2, m_ib->data);
		s_renderCtx->m_vertexBuffers[m_vb->handle.idx].update(0, numVertices*m_decl.m_stride, m_vb->data);

		VertexBuffer& vb = s_renderCtx->m_vertexBuffers[m_vb->handle.idx];
		GL_CHECK(glBindBuffer(GL_ARRAY_BUFFER, vb.m_id) );

		IndexBuffer& ib = s_renderCtx->m_indexBuffers[m_ib->handle.idx];
		GL_CHECK(glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ib.m_id) );

		Program& program = s_renderCtx->m_program[m_program.idx];
		program.bindAttributes(m_decl, 0);

		GL_CHECK(glDrawElements(GL_TRIANGLES
			, _numIndices
			, GL_UNSIGNED_SHORT
			, (void*)0
			) );
	}

	void ClearQuad::clear(const Rect& _rect, const Clear& _clear, uint32_t _height)
	{
		if (BX_ENABLED(BGFX_CONFIG_CLEAR_QUAD)
		&&  s_renderCtx->m_useClearQuad)
		{
			const GLuint defaultVao = s_renderCtx->m_vao;
			if (0 != defaultVao)
			{
				GL_CHECK(glBindVertexArray(defaultVao) );
			}

			GL_CHECK(glDisable(GL_SCISSOR_TEST) );
			GL_CHECK(glDisable(GL_CULL_FACE) );
			GL_CHECK(glDisable(GL_BLEND) );

			GLboolean colorMask = !!(BGFX_CLEAR_COLOR_BIT & _clear.m_flags);
			GL_CHECK(glColorMask(colorMask, colorMask, colorMask, colorMask) );

			if (BGFX_CLEAR_DEPTH_BIT & _clear.m_flags)
			{
				GL_CHECK(glEnable(GL_DEPTH_TEST) );
				GL_CHECK(glDepthFunc(GL_ALWAYS) );
				GL_CHECK(glDepthMask(GL_TRUE) );
			}
			else
			{
				GL_CHECK(glDisable(GL_DEPTH_TEST) );
			}

			if (BGFX_CLEAR_STENCIL_BIT & _clear.m_flags)
			{
				GL_CHECK(glEnable(GL_STENCIL_TEST) );
				GL_CHECK(glStencilFuncSeparate(GL_FRONT_AND_BACK, GL_ALWAYS, _clear.m_stencil,  0xff) );
				GL_CHECK(glStencilOpSeparate(GL_FRONT_AND_BACK, GL_REPLACE, GL_REPLACE, GL_REPLACE) );
			}
			else
			{
				GL_CHECK(glDisable(GL_STENCIL_TEST) );
			}

			VertexBuffer& vb = s_renderCtx->m_vertexBuffers[m_vb->handle.idx];
			VertexDecl& vertexDecl = s_renderCtx->m_vertexDecls[m_vb->decl.idx];

			{
				struct Vertex
				{
					float m_x;
					float m_y;
					float m_z;
					uint32_t m_abgr;
				} * vertex = (Vertex*)m_vb->data;
				BX_CHECK(vertexDecl.m_stride == sizeof(Vertex), "Stride/Vertex mismatch (stride %d, sizeof(Vertex) %d)", vertexDecl.m_stride, sizeof(Vertex) );

				const uint32_t abgr = bx::endianSwap(_clear.m_rgba);
				const float depth = _clear.m_depth;

				vertex->m_x = -1.0f;
				vertex->m_y = -1.0f;
				vertex->m_z = depth;
				vertex->m_abgr = abgr;
				vertex++;
				vertex->m_x =  1.0f;
				vertex->m_y = -1.0f;
				vertex->m_z = depth;
				vertex->m_abgr = abgr;
				vertex++;
				vertex->m_x =  1.0f;
				vertex->m_y =  1.0f;
				vertex->m_z = depth;
				vertex->m_abgr = abgr;
				vertex++;
				vertex->m_x = -1.0f;
				vertex->m_y =  1.0f;
				vertex->m_z = depth;
				vertex->m_abgr = abgr;
			}

			s_renderCtx->m_vertexBuffers[m_vb->handle.idx].update(0, 4*m_decl.m_stride, m_vb->data);

			GL_CHECK(glBindBuffer(GL_ARRAY_BUFFER, vb.m_id) );

			IndexBuffer& ib = s_renderCtx->m_indexBuffers[m_ib.idx];
			GL_CHECK(glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ib.m_id) );

			uint32_t numMrt = 0;
			FrameBufferHandle fbh = s_renderCtx->m_fbh;
			if (isValid(fbh) )
			{
				const FrameBuffer& fb = s_renderCtx->m_frameBuffers[fbh.idx];
				numMrt = bx::uint32_max(1, fb.m_num)-1;
			}

			Program& program = s_renderCtx->m_program[m_program[numMrt].idx];
			GL_CHECK(glUseProgram(program.m_id) );
			program.bindAttributes(vertexDecl, 0);

			GL_CHECK(glDrawElements(GL_TRIANGLES
				, 6
				, GL_UNSIGNED_SHORT
				, (void*)0
				) );
		}
		else
		{
			GLuint flags = 0;
			if (BGFX_CLEAR_COLOR_BIT & _clear.m_flags)
			{
				flags |= GL_COLOR_BUFFER_BIT;
				uint32_t rgba = _clear.m_rgba;
				float rr = (rgba>>24)/255.0f;
				float gg = ( (rgba>>16)&0xff)/255.0f;
				float bb = ( (rgba>>8)&0xff)/255.0f;
				float aa = (rgba&0xff)/255.0f;
				GL_CHECK(glClearColor(rr, gg, bb, aa) );
				GL_CHECK(glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE) );
			}

			if (BGFX_CLEAR_DEPTH_BIT & _clear.m_flags)
			{
				flags |= GL_DEPTH_BUFFER_BIT;
				GL_CHECK(glClearDepth(_clear.m_depth) );
				GL_CHECK(glDepthMask(GL_TRUE) );
			}

			if (BGFX_CLEAR_STENCIL_BIT & _clear.m_flags)
			{
				flags |= GL_STENCIL_BUFFER_BIT;
				GL_CHECK(glClearStencil(_clear.m_stencil) );
			}

			if (0 != flags)
			{
				GL_CHECK(glEnable(GL_SCISSOR_TEST) );
				GL_CHECK(glScissor(_rect.m_x, _height-_rect.m_height-_rect.m_y, _rect.m_width, _rect.m_height) );
				GL_CHECK(glClear(flags) );
				GL_CHECK(glDisable(GL_SCISSOR_TEST) );
			}
		}
	}

	void Context::rendererFlip()
	{
		if (NULL != s_renderCtx)
		{
			s_renderCtx->flip();
		}
	}

	void Context::rendererInit()
	{
		s_renderCtx = BX_NEW(g_allocator, RendererContext);
		s_renderCtx->init();
	}

	void Context::rendererShutdown()
	{
		if (s_renderCtx->m_vaoSupport)
		{
			GL_CHECK(glBindVertexArray(0) );
			GL_CHECK(glDeleteVertexArrays(1, &s_renderCtx->m_vao) );
			s_renderCtx->m_vao = 0;
		}

		s_renderCtx->shutdown();

		BX_DELETE(g_allocator, s_renderCtx);
		s_renderCtx = NULL;
	}

	void Context::rendererCreateIndexBuffer(IndexBufferHandle _handle, Memory* _mem)
	{
		s_renderCtx->m_indexBuffers[_handle.idx].create(_mem->size, _mem->data);
	}

	void Context::rendererDestroyIndexBuffer(IndexBufferHandle _handle)
	{
		s_renderCtx->m_indexBuffers[_handle.idx].destroy();
	}

	void Context::rendererCreateVertexDecl(VertexDeclHandle _handle, const VertexDecl& _decl)
	{
		VertexDecl& decl = s_renderCtx->m_vertexDecls[_handle.idx];
		memcpy(&decl, &_decl, sizeof(VertexDecl) );
		dump(decl);
	}

	void Context::rendererDestroyVertexDecl(VertexDeclHandle /*_handle*/)
	{
	}

	void Context::rendererCreateVertexBuffer(VertexBufferHandle _handle, Memory* _mem, VertexDeclHandle _declHandle)
	{
		s_renderCtx->m_vertexBuffers[_handle.idx].create(_mem->size, _mem->data, _declHandle);
	}

	void Context::rendererDestroyVertexBuffer(VertexBufferHandle _handle)
	{
		s_renderCtx->m_vertexBuffers[_handle.idx].destroy();
	}

	void Context::rendererCreateDynamicIndexBuffer(IndexBufferHandle _handle, uint32_t _size)
	{
		s_renderCtx->m_indexBuffers[_handle.idx].create(_size, NULL);
	}

	void Context::rendererUpdateDynamicIndexBuffer(IndexBufferHandle _handle, uint32_t _offset, uint32_t _size, Memory* _mem)
	{
		s_renderCtx->m_indexBuffers[_handle.idx].update(_offset, bx::uint32_min(_size, _mem->size), _mem->data);
	}

	void Context::rendererDestroyDynamicIndexBuffer(IndexBufferHandle _handle)
	{
		s_renderCtx->m_indexBuffers[_handle.idx].destroy();
	}

	void Context::rendererCreateDynamicVertexBuffer(VertexBufferHandle _handle, uint32_t _size)
	{
		VertexDeclHandle decl = BGFX_INVALID_HANDLE;
		s_renderCtx->m_vertexBuffers[_handle.idx].create(_size, NULL, decl);
	}

	void Context::rendererUpdateDynamicVertexBuffer(VertexBufferHandle _handle, uint32_t _offset, uint32_t _size, Memory* _mem)
	{
		s_renderCtx->m_vertexBuffers[_handle.idx].update(_offset, bx::uint32_min(_size, _mem->size), _mem->data);
	}

	void Context::rendererDestroyDynamicVertexBuffer(VertexBufferHandle _handle)
	{
		s_renderCtx->m_vertexBuffers[_handle.idx].destroy();
	}

	void Context::rendererCreateShader(ShaderHandle _handle, Memory* _mem)
	{
		s_renderCtx->m_shaders[_handle.idx].create(_mem);
	}

	void Context::rendererDestroyShader(ShaderHandle _handle)
	{
		s_renderCtx->m_shaders[_handle.idx].destroy();
	}

	void Context::rendererCreateProgram(ProgramHandle _handle, ShaderHandle _vsh, ShaderHandle _fsh)
	{
		Shader dummyFragmentShader;
		s_renderCtx->m_program[_handle.idx].create(s_renderCtx->m_shaders[_vsh.idx], isValid(_fsh) ? s_renderCtx->m_shaders[_fsh.idx] : dummyFragmentShader);
	}

	void Context::rendererDestroyProgram(ProgramHandle _handle)
	{
		s_renderCtx->m_program[_handle.idx].destroy();
	}

	void Context::rendererCreateTexture(TextureHandle _handle, Memory* _mem, uint32_t _flags, uint8_t _skip)
	{
		s_renderCtx->m_textures[_handle.idx].create(_mem, _flags, _skip);
	}

	void Context::rendererUpdateTextureBegin(TextureHandle /*_handle*/, uint8_t /*_side*/, uint8_t /*_mip*/)
	{
	}

	void Context::rendererUpdateTexture(TextureHandle _handle, uint8_t _side, uint8_t _mip, const Rect& _rect, uint16_t _z, uint16_t _depth, uint16_t _pitch, const Memory* _mem)
	{
		s_renderCtx->m_textures[_handle.idx].update(_side, _mip, _rect, _z, _depth, _pitch, _mem);
	}

	void Context::rendererUpdateTextureEnd()
	{
	}

	void Context::rendererDestroyTexture(TextureHandle _handle)
	{
		s_renderCtx->m_textures[_handle.idx].destroy();
	}

	void Context::rendererCreateFrameBuffer(FrameBufferHandle _handle, uint8_t _num, const TextureHandle* _textureHandles)
	{
		s_renderCtx->m_frameBuffers[_handle.idx].create(_num, _textureHandles);
	}

	void Context::rendererDestroyFrameBuffer(FrameBufferHandle _handle)
	{
		s_renderCtx->m_frameBuffers[_handle.idx].destroy();
	}

	void Context::rendererCreateUniform(UniformHandle _handle, UniformType::Enum _type, uint16_t _num, const char* _name)
	{
		if (NULL != s_renderCtx->m_uniforms[_handle.idx])
		{
			BX_FREE(g_allocator, s_renderCtx->m_uniforms[_handle.idx]);
		}

		uint32_t size = g_uniformTypeSize[_type]*_num;
		void* data = BX_ALLOC(g_allocator, size);
		memset(data, 0, size);
		s_renderCtx->m_uniforms[_handle.idx] = data;
		s_renderCtx->m_uniformReg.add(_handle, _name, s_renderCtx->m_uniforms[_handle.idx]);
	}

	void Context::rendererDestroyUniform(UniformHandle _handle)
	{
		BX_FREE(g_allocator, s_renderCtx->m_uniforms[_handle.idx]);
		s_renderCtx->m_uniforms[_handle.idx] = NULL;
	}

	void Context::rendererSaveScreenShot(const char* _filePath)
	{
		s_renderCtx->saveScreenShot(_filePath);
	}

	void Context::rendererUpdateViewName(uint8_t _id, const char* _name)
	{
		bx::strlcpy(&s_viewName[_id][0], _name, BX_COUNTOF(s_viewName[0]) );
	}

	void Context::rendererUpdateUniform(uint16_t _loc, const void* _data, uint32_t _size)
	{
		memcpy(s_renderCtx->m_uniforms[_loc], _data, _size);
	}

	void Context::rendererSetMarker(const char* _marker, uint32_t _size)
	{
		GL_CHECK(glInsertEventMarker(_size, _marker) );
	}

	void Context::rendererSubmit()
	{
		const GLuint defaultVao = s_renderCtx->m_vao;
		if (0 != defaultVao)
		{
			GL_CHECK(glBindVertexArray(defaultVao) );
		}

		GL_CHECK(glBindFramebuffer(GL_FRAMEBUFFER, s_renderCtx->m_backBufferFbo) );

		s_renderCtx->updateResolution(m_render->m_resolution);

		int64_t elapsed = -bx::getHPCounter();
		int64_t captureElapsed = 0;

		if (BX_ENABLED(BGFX_CONFIG_RENDERER_OPENGL)
		&& (m_render->m_debug & (BGFX_DEBUG_IFH|BGFX_DEBUG_STATS) ) )
		{
			s_renderCtx->m_queries.begin(0, GL_TIME_ELAPSED);
		}

		if (0 < m_render->m_iboffset)
		{
			TransientIndexBuffer* ib = m_render->m_transientIb;
			s_renderCtx->m_indexBuffers[ib->handle.idx].update(0, m_render->m_iboffset, ib->data);
		}

		if (0 < m_render->m_vboffset)
		{
			TransientVertexBuffer* vb = m_render->m_transientVb;
			s_renderCtx->m_vertexBuffers[vb->handle.idx].update(0, m_render->m_vboffset, vb->data);
		}

		m_render->sort();

		RenderState currentState;
		currentState.reset();
		currentState.m_flags = BGFX_STATE_NONE;
		currentState.m_stencil = packStencil(BGFX_STENCIL_NONE, BGFX_STENCIL_NONE);

		Matrix4 viewProj[BGFX_CONFIG_MAX_VIEWS];
		for (uint32_t ii = 0; ii < BGFX_CONFIG_MAX_VIEWS; ++ii)
		{
			float4x4_mul(&viewProj[ii].un.f4x4, &m_render->m_view[ii].un.f4x4, &m_render->m_proj[ii].un.f4x4);
		}

		uint16_t programIdx = invalidHandle;
		SortKey key;
		uint8_t view = 0xff;
		FrameBufferHandle fbh = BGFX_INVALID_HANDLE;
		int32_t height = m_render->m_resolution.m_height;
		float alphaRef = 0.0f;
		uint32_t blendFactor = 0;
		GLenum primType = m_render->m_debug&BGFX_DEBUG_WIREFRAME ? GL_LINES : GL_TRIANGLES;
		uint32_t primNumVerts = 3;
		uint32_t baseVertex = 0;
		GLuint currentVao = 0;
		bool viewHasScissor = false;
		Rect viewScissorRect;
		viewScissorRect.clear();

		const bool blendIndependentSupported = s_extension[Extension::ARB_draw_buffers_blend].m_supported;

		uint32_t statsNumPrimsSubmitted = 0;
		uint32_t statsNumIndices = 0;
		uint32_t statsNumInstances = 0;
		uint32_t statsNumPrimsRendered = 0;

		if (0 == (m_render->m_debug&BGFX_DEBUG_IFH) )
		{
			GL_CHECK(glBindFramebuffer(GL_FRAMEBUFFER, s_renderCtx->m_msaaBackBufferFbo) );

			for (uint32_t item = 0, numItems = m_render->m_num; item < numItems; ++item)
			{
				key.decode(m_render->m_sortKeys[item]);
				const RenderState& state = m_render->m_renderState[m_render->m_sortValues[item] ];

				const uint64_t newFlags = state.m_flags;
				uint64_t changedFlags = currentState.m_flags ^ state.m_flags;
				currentState.m_flags = newFlags;

				const uint64_t newStencil = state.m_stencil;
				uint64_t changedStencil = currentState.m_stencil ^ state.m_stencil;
				currentState.m_stencil = newStencil;

				if (key.m_view != view)
				{
					currentState.clear();
					currentState.m_scissor = !state.m_scissor;
					changedFlags = BGFX_STATE_MASK;
					changedStencil = packStencil(BGFX_STENCIL_MASK, BGFX_STENCIL_MASK);
					currentState.m_flags = newFlags;
					currentState.m_stencil = newStencil;

					GL_CHECK(glInsertEventMarker(0, s_viewName[key.m_view]) );

					view = key.m_view;
					programIdx = invalidHandle;

					if (m_render->m_fb[view].idx != fbh.idx)
					{
						fbh = m_render->m_fb[view];
						height = s_renderCtx->setFrameBuffer(fbh, m_render->m_resolution.m_height);
					}

					const Rect& rect = m_render->m_rect[view];
					const Rect& scissorRect = m_render->m_scissor[view];
					viewHasScissor = !scissorRect.isZero();
					viewScissorRect = viewHasScissor ? scissorRect : rect;

					GL_CHECK(glViewport(rect.m_x, height-rect.m_height-rect.m_y, rect.m_width, rect.m_height) );

					Clear& clear = m_render->m_clear[view];

					if (BGFX_CLEAR_NONE != clear.m_flags)
					{
						m_clearQuad.clear(rect, clear, height);
					}

					GL_CHECK(glDisable(GL_STENCIL_TEST) );
					GL_CHECK(glEnable(GL_DEPTH_TEST) );
					GL_CHECK(glDepthFunc(GL_LESS) );
					GL_CHECK(glEnable(GL_CULL_FACE) );
					GL_CHECK(glDisable(GL_BLEND) );
				}

				uint16_t scissor = state.m_scissor;
				if (currentState.m_scissor != scissor)
				{
					currentState.m_scissor = scissor;

					if (UINT16_MAX == scissor)
					{
						if (viewHasScissor)
						{
							GL_CHECK(glEnable(GL_SCISSOR_TEST) );
							GL_CHECK(glScissor(viewScissorRect.m_x, height-viewScissorRect.m_height-viewScissorRect.m_y, viewScissorRect.m_width, viewScissorRect.m_height) );
						}
						else
						{
							GL_CHECK(glDisable(GL_SCISSOR_TEST) );
						}
					}
					else
					{
						Rect scissorRect;
						scissorRect.intersect(viewScissorRect, m_render->m_rectCache.m_cache[scissor]);
						GL_CHECK(glEnable(GL_SCISSOR_TEST) );
						GL_CHECK(glScissor(scissorRect.m_x, height-scissorRect.m_height-scissorRect.m_y, scissorRect.m_width, scissorRect.m_height) );
					}
				}

				if (0 != changedStencil)
				{
					if (0 != newStencil)
					{
						GL_CHECK(glEnable(GL_STENCIL_TEST) );

						uint32_t bstencil = unpackStencil(1, newStencil);
						uint32_t frontAndBack = bstencil != BGFX_STENCIL_NONE && bstencil != unpackStencil(0, newStencil);

// 						uint32_t bchanged = unpackStencil(1, changedStencil);
// 						if (BGFX_STENCIL_FUNC_RMASK_MASK & bchanged)
// 						{
// 							uint32_t wmask = (bstencil&BGFX_STENCIL_FUNC_RMASK_MASK)>>BGFX_STENCIL_FUNC_RMASK_SHIFT;
// 							GL_CHECK(glStencilMask(wmask) );
// 						}

						for (uint32_t ii = 0, num = frontAndBack+1; ii < num; ++ii)
						{
							uint32_t stencil = unpackStencil(ii, newStencil);
							uint32_t changed = unpackStencil(ii, changedStencil);
							GLenum face = s_stencilFace[frontAndBack+ii];

							if ( (BGFX_STENCIL_TEST_MASK|BGFX_STENCIL_FUNC_REF_MASK|BGFX_STENCIL_FUNC_RMASK_MASK) & changed)
							{
								GLint ref = (stencil&BGFX_STENCIL_FUNC_REF_MASK)>>BGFX_STENCIL_FUNC_REF_SHIFT;
								GLint mask = (stencil&BGFX_STENCIL_FUNC_RMASK_MASK)>>BGFX_STENCIL_FUNC_RMASK_SHIFT;
								uint32_t func = (stencil&BGFX_STENCIL_TEST_MASK)>>BGFX_STENCIL_TEST_SHIFT;
								GL_CHECK(glStencilFuncSeparate(face, s_cmpFunc[func], ref, mask));
							}

							if ( (BGFX_STENCIL_OP_FAIL_S_MASK|BGFX_STENCIL_OP_FAIL_Z_MASK|BGFX_STENCIL_OP_PASS_Z_MASK) & changed)
							{
								uint32_t sfail = (stencil&BGFX_STENCIL_OP_FAIL_S_MASK)>>BGFX_STENCIL_OP_FAIL_S_SHIFT;
								uint32_t zfail = (stencil&BGFX_STENCIL_OP_FAIL_Z_MASK)>>BGFX_STENCIL_OP_FAIL_Z_SHIFT;
								uint32_t zpass = (stencil&BGFX_STENCIL_OP_PASS_Z_MASK)>>BGFX_STENCIL_OP_PASS_Z_SHIFT;
								GL_CHECK(glStencilOpSeparate(face, s_stencilOp[sfail], s_stencilOp[zfail], s_stencilOp[zpass]) );
							}
						}
					}
					else
					{
						GL_CHECK(glDisable(GL_STENCIL_TEST) );
					}
				}

				if ( (0
					 | BGFX_STATE_CULL_MASK
					 | BGFX_STATE_DEPTH_WRITE
					 | BGFX_STATE_DEPTH_TEST_MASK
					 | BGFX_STATE_RGB_WRITE
					 | BGFX_STATE_ALPHA_WRITE
					 | BGFX_STATE_BLEND_MASK
					 | BGFX_STATE_BLEND_EQUATION_MASK
					 | BGFX_STATE_ALPHA_REF_MASK
					 | BGFX_STATE_PT_MASK
					 | BGFX_STATE_POINT_SIZE_MASK
					 | BGFX_STATE_MSAA
					 ) & changedFlags)
				{
					if (BGFX_STATE_CULL_MASK & changedFlags)
					{
						if (BGFX_STATE_CULL_CW & newFlags)
						{
							GL_CHECK(glEnable(GL_CULL_FACE) );
							GL_CHECK(glCullFace(GL_BACK) );
						}
						else if (BGFX_STATE_CULL_CCW & newFlags)
						{
							GL_CHECK(glEnable(GL_CULL_FACE) );
							GL_CHECK(glCullFace(GL_FRONT) );
						}
						else
						{
							GL_CHECK(glDisable(GL_CULL_FACE) );
						}
					}

					if (BGFX_STATE_DEPTH_WRITE & changedFlags)
					{
						GL_CHECK(glDepthMask(!!(BGFX_STATE_DEPTH_WRITE & newFlags) ) );
					}

					if (BGFX_STATE_DEPTH_TEST_MASK & changedFlags)
					{
						uint32_t func = (newFlags&BGFX_STATE_DEPTH_TEST_MASK)>>BGFX_STATE_DEPTH_TEST_SHIFT;

						if (0 != func)
						{
							GL_CHECK(glEnable(GL_DEPTH_TEST) );
							GL_CHECK(glDepthFunc(s_cmpFunc[func]) );
						}
						else
						{
							GL_CHECK(glDisable(GL_DEPTH_TEST) );
						}
					}

					if (BGFX_STATE_ALPHA_REF_MASK & changedFlags)
					{
						uint32_t ref = (newFlags&BGFX_STATE_ALPHA_REF_MASK)>>BGFX_STATE_ALPHA_REF_SHIFT;
						alphaRef = ref/255.0f;
					}

#if BGFX_CONFIG_RENDERER_OPENGL
					if ( (BGFX_STATE_PT_POINTS|BGFX_STATE_POINT_SIZE_MASK) & changedFlags)
					{
						float pointSize = (float)(bx::uint32_max(1, (newFlags&BGFX_STATE_POINT_SIZE_MASK)>>BGFX_STATE_POINT_SIZE_SHIFT) );
						GL_CHECK(glPointSize(pointSize) );
					}

					if (BGFX_STATE_MSAA & changedFlags)
					{
						if (BGFX_STATE_MSAA & newFlags)
						{
							GL_CHECK(glEnable(GL_MULTISAMPLE) );
						}
						else
						{
							GL_CHECK(glDisable(GL_MULTISAMPLE) );
						}
					}
#endif // BGFX_CONFIG_RENDERER_OPENGL

					if ( (BGFX_STATE_ALPHA_WRITE|BGFX_STATE_RGB_WRITE) & changedFlags)
					{
						GLboolean alpha = !!(newFlags&BGFX_STATE_ALPHA_WRITE);
						GLboolean rgb = !!(newFlags&BGFX_STATE_RGB_WRITE);
						GL_CHECK(glColorMask(rgb, rgb, rgb, alpha) );
					}

					if ( (BGFX_STATE_BLEND_MASK|BGFX_STATE_BLEND_EQUATION_MASK|BGFX_STATE_BLEND_INDEPENDENT) & changedFlags
					||  blendFactor != state.m_rgba)
					{
						if ( (BGFX_STATE_BLEND_MASK|BGFX_STATE_BLEND_EQUATION_MASK|BGFX_STATE_BLEND_INDEPENDENT) & newFlags
						||  blendFactor != state.m_rgba)
						{
							const bool enabled = !!(BGFX_STATE_BLEND_MASK & newFlags);
							const bool independent = !!(BGFX_STATE_BLEND_INDEPENDENT & newFlags)
								&& blendIndependentSupported
								;

							const uint32_t blend    = uint32_t( (newFlags&BGFX_STATE_BLEND_MASK)>>BGFX_STATE_BLEND_SHIFT);
							const uint32_t equation = uint32_t( (newFlags&BGFX_STATE_BLEND_EQUATION_MASK)>>BGFX_STATE_BLEND_EQUATION_SHIFT);

							const uint32_t srcRGB  = (blend    )&0xf;
							const uint32_t dstRGB  = (blend>> 4)&0xf;
							const uint32_t srcA    = (blend>> 8)&0xf;
							const uint32_t dstA    = (blend>>12)&0xf;

							const uint32_t equRGB = (equation   )&0x7;
							const uint32_t equA   = (equation>>3)&0x7;

							const uint32_t numRt = s_renderCtx->getNumRt();

							if (!BX_ENABLED(BGFX_CONFIG_RENDERER_OPENGL)
							||  1 >= numRt
							||  !independent)
							{
								if (enabled)
								{
									GL_CHECK(glEnable(GL_BLEND) );
									GL_CHECK(glBlendFuncSeparate(s_blendFactor[srcRGB].m_src
										, s_blendFactor[dstRGB].m_dst
										, s_blendFactor[srcA].m_src
										, s_blendFactor[dstA].m_dst
										) );
									GL_CHECK(glBlendEquationSeparate(s_blendEquation[equRGB], s_blendEquation[equA]) );

									if ( (s_blendFactor[srcRGB].m_factor || s_blendFactor[dstRGB].m_factor)
									&&  blendFactor != state.m_rgba)
									{
										const uint32_t rgba = state.m_rgba;
										GLclampf rr = ( (rgba>>24)     )/255.0f;
										GLclampf gg = ( (rgba>>16)&0xff)/255.0f;
										GLclampf bb = ( (rgba>> 8)&0xff)/255.0f;
										GLclampf aa = ( (rgba    )&0xff)/255.0f;

										GL_CHECK(glBlendColor(rr, gg, bb, aa) );
									}
								}
								else
								{
									GL_CHECK(glDisable(GL_BLEND) );
								}
							}
							else
							{
								if (enabled)
								{
									GL_CHECK(glEnablei(GL_BLEND, 0) );
									GL_CHECK(glBlendFuncSeparatei(0
										, s_blendFactor[srcRGB].m_src
										, s_blendFactor[dstRGB].m_dst
										, s_blendFactor[srcA].m_src
										, s_blendFactor[dstA].m_dst
										) );
									GL_CHECK(glBlendEquationSeparatei(0
										, s_blendEquation[equRGB]
										, s_blendEquation[equA]
										) );
								}
								else
								{
									GL_CHECK(glDisablei(GL_BLEND, 0) );
								}

								for (uint32_t ii = 1, rgba = state.m_rgba; ii < numRt; ++ii, rgba >>= 11)
								{
									if (0 != (rgba&0x7ff) )
									{
										const uint32_t src      = (rgba   )&0xf;
										const uint32_t dst      = (rgba>>4)&0xf;
										const uint32_t equation = (rgba>>8)&0x7;
										GL_CHECK(glEnablei(GL_BLEND, ii) );
										GL_CHECK(glBlendFunci(ii, s_blendFactor[src].m_src, s_blendFactor[dst].m_dst) );
										GL_CHECK(glBlendEquationi(ii, s_blendEquation[equation]) );
									}
									else
									{
										GL_CHECK(glDisablei(GL_BLEND, ii) );
									}
								}
							}
						}
						else
						{
							GL_CHECK(glDisable(GL_BLEND) );
						}

						blendFactor = state.m_rgba;
					}

					uint8_t primIndex = uint8_t( (newFlags&BGFX_STATE_PT_MASK)>>BGFX_STATE_PT_SHIFT);
					primType = m_render->m_debug&BGFX_DEBUG_WIREFRAME ? GL_LINES : s_primType[primIndex];
					primNumVerts = 3-primIndex;
				}

				bool programChanged = false;
				bool constantsChanged = state.m_constBegin < state.m_constEnd;
				bool bindAttribs = false;
				rendererUpdateUniforms(m_render->m_constantBuffer, state.m_constBegin, state.m_constEnd);

				if (key.m_program != programIdx)
				{
					programIdx = key.m_program;
					GLuint id = invalidHandle == programIdx ? 0 : s_renderCtx->m_program[programIdx].m_id;
					GL_CHECK(glUseProgram(id) );
					programChanged =
						constantsChanged =
						bindAttribs = true;
				}

				if (invalidHandle != programIdx)
				{
					Program& program = s_renderCtx->m_program[programIdx];

					if (constantsChanged)
					{
						program.commit();
					}

					for (uint32_t ii = 0, num = program.m_numPredefined; ii < num; ++ii)
					{
						PredefinedUniform& predefined = program.m_predefined[ii];
						switch (predefined.m_type)
						{
						case PredefinedUniform::ViewRect:
							{
								float rect[4];
								rect[0] = m_render->m_rect[view].m_x;
								rect[1] = m_render->m_rect[view].m_y;
								rect[2] = m_render->m_rect[view].m_width;
								rect[3] = m_render->m_rect[view].m_height;

								GL_CHECK(glUniform4fv(predefined.m_loc
									, 1
									, &rect[0]
								) );
							}
							break;

						case PredefinedUniform::ViewTexel:
							{
								float rect[4];
								rect[0] = 1.0f/float(m_render->m_rect[view].m_width);
								rect[1] = 1.0f/float(m_render->m_rect[view].m_height);

								GL_CHECK(glUniform4fv(predefined.m_loc
									, 1
									, &rect[0]
								) );
							}
							break;

						case PredefinedUniform::View:
							{
								GL_CHECK(glUniformMatrix4fv(predefined.m_loc
									, 1
									, GL_FALSE
									, m_render->m_view[view].un.val
									) );
							}
							break;

						case PredefinedUniform::ViewProj:
							{
								GL_CHECK(glUniformMatrix4fv(predefined.m_loc
									, 1
									, GL_FALSE
									, viewProj[view].un.val
									) );
							}
							break;

						case PredefinedUniform::Model:
							{
								const Matrix4& model = m_render->m_matrixCache.m_cache[state.m_matrix];
								GL_CHECK(glUniformMatrix4fv(predefined.m_loc
									, bx::uint32_min(predefined.m_count, state.m_num)
									, GL_FALSE
									, model.un.val
									) );
							}
							break;

						case PredefinedUniform::ModelView:
							{
								Matrix4 modelView;
								const Matrix4& model = m_render->m_matrixCache.m_cache[state.m_matrix];
								bx::float4x4_mul(&modelView.un.f4x4, &model.un.f4x4, &m_render->m_view[view].un.f4x4);

								GL_CHECK(glUniformMatrix4fv(predefined.m_loc
									, 1
									, GL_FALSE
									, modelView.un.val
									) );
							}
							break;

						case PredefinedUniform::ModelViewProj:
							{
								Matrix4 modelViewProj;
								const Matrix4& model = m_render->m_matrixCache.m_cache[state.m_matrix];
								bx::float4x4_mul(&modelViewProj.un.f4x4, &model.un.f4x4, &viewProj[view].un.f4x4);

								GL_CHECK(glUniformMatrix4fv(predefined.m_loc
									, 1
									, GL_FALSE
									, modelViewProj.un.val
									) );
							}
							break;

						case PredefinedUniform::ModelViewProjX:
							{
								const Matrix4& model = m_render->m_matrixCache.m_cache[state.m_matrix];

								uint8_t other = m_render->m_other[view];
								Matrix4 viewProjBias;
								bx::float4x4_mul(&viewProjBias.un.f4x4, &viewProj[other].un.f4x4, &s_bias.un.f4x4);

								Matrix4 modelViewProj;
								bx::float4x4_mul(&modelViewProj.un.f4x4, &model.un.f4x4, &viewProjBias.un.f4x4);

								GL_CHECK(glUniformMatrix4fv(predefined.m_loc
									, 1
									, GL_FALSE
									, modelViewProj.un.val
									) );
							}
							break;

						case PredefinedUniform::ViewProjX:
							{
								uint8_t other = m_render->m_other[view];
								Matrix4 viewProjBias;
								bx::float4x4_mul(&viewProjBias.un.f4x4, &viewProj[other].un.f4x4, &s_bias.un.f4x4);

								GL_CHECK(glUniformMatrix4fv(predefined.m_loc
									, 1
									, GL_FALSE
									, viewProjBias.un.val
									) );
							}
							break;

						case PredefinedUniform::AlphaRef:
							{
								GL_CHECK(glUniform1f(predefined.m_loc, alphaRef) );
							}
							break;

						case PredefinedUniform::Count:
							break;
						}
					}

//						if (BGFX_STATE_TEX_MASK & changedFlags)
					{
						uint64_t flag = BGFX_STATE_TEX0;
						for (uint32_t stage = 0; stage < BGFX_STATE_TEX_COUNT; ++stage)
						{
							const Sampler& sampler = state.m_sampler[stage];
							Sampler& current = currentState.m_sampler[stage];
							if (current.m_idx != sampler.m_idx
							||  current.m_flags != sampler.m_flags
							||  programChanged)
							{
								if (invalidHandle != sampler.m_idx)
								{
									Texture& texture = s_renderCtx->m_textures[sampler.m_idx];
									texture.commit(stage, sampler.m_flags);
								}
							}

							current = sampler;
							flag <<= 1;
						}
					}

					if (0 != defaultVao
					&&  0 == state.m_startVertex
					&&  0 == state.m_instanceDataOffset)
					{
						if (programChanged
						||  currentState.m_vertexBuffer.idx != state.m_vertexBuffer.idx
						||  currentState.m_indexBuffer.idx != state.m_indexBuffer.idx
						||  currentState.m_instanceDataBuffer.idx != state.m_instanceDataBuffer.idx
						||  currentState.m_instanceDataOffset != state.m_instanceDataOffset
						||  currentState.m_instanceDataStride != state.m_instanceDataStride)
						{
							bx::HashMurmur2A murmur;
							murmur.begin();
							murmur.add(state.m_vertexBuffer.idx);
							murmur.add(state.m_indexBuffer.idx);
							murmur.add(state.m_instanceDataBuffer.idx);
							murmur.add(state.m_instanceDataOffset);
							murmur.add(state.m_instanceDataStride);
							murmur.add(programIdx);
							uint32_t hash = murmur.end();

							currentState.m_vertexBuffer = state.m_vertexBuffer;
							currentState.m_indexBuffer = state.m_indexBuffer;
							currentState.m_instanceDataOffset = state.m_instanceDataOffset;
							currentState.m_instanceDataStride = state.m_instanceDataStride;
							baseVertex = state.m_startVertex;

							GLuint id = s_renderCtx->m_vaoStateCache.find(hash);
							if (UINT32_MAX != id)
							{
								currentVao = id;
								GL_CHECK(glBindVertexArray(id) );
							}
							else
							{
								id = s_renderCtx->m_vaoStateCache.add(hash);
								currentVao = id;
								GL_CHECK(glBindVertexArray(id) );

								Program& program = s_renderCtx->m_program[programIdx];
								program.add(hash);

								if (isValid(state.m_vertexBuffer) )
								{
									VertexBuffer& vb = s_renderCtx->m_vertexBuffers[state.m_vertexBuffer.idx];
									vb.add(hash);
									GL_CHECK(glBindBuffer(GL_ARRAY_BUFFER, vb.m_id) );

									uint16_t decl = !isValid(vb.m_decl) ? state.m_vertexDecl.idx : vb.m_decl.idx;
									program.bindAttributes(s_renderCtx->m_vertexDecls[decl], state.m_startVertex);

									if (isValid(state.m_instanceDataBuffer) )
									{
										VertexBuffer& instanceVb = s_renderCtx->m_vertexBuffers[state.m_instanceDataBuffer.idx];
										instanceVb.add(hash);
										GL_CHECK(glBindBuffer(GL_ARRAY_BUFFER, instanceVb.m_id) );
										program.bindInstanceData(state.m_instanceDataStride, state.m_instanceDataOffset);
									}
								}
								else
								{
									GL_CHECK(glBindBuffer(GL_ARRAY_BUFFER, 0) );
								}

								if (isValid(state.m_indexBuffer) )
								{
									IndexBuffer& ib = s_renderCtx->m_indexBuffers[state.m_indexBuffer.idx];
									ib.add(hash);
									GL_CHECK(glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ib.m_id) );
								}
								else
								{
									GL_CHECK(glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0) );
								}
							}
						}
					}
					else
					{
						if (0 != defaultVao
						&&  0 != currentVao)
						{
							GL_CHECK(glBindVertexArray(defaultVao) );
							currentState.m_vertexBuffer.idx = invalidHandle;
							currentState.m_indexBuffer.idx = invalidHandle;
							bindAttribs = true;
							currentVao = 0;
						}

						if (programChanged
						||  currentState.m_vertexBuffer.idx != state.m_vertexBuffer.idx
						||  currentState.m_instanceDataBuffer.idx != state.m_instanceDataBuffer.idx
						||  currentState.m_instanceDataOffset != state.m_instanceDataOffset
						||  currentState.m_instanceDataStride != state.m_instanceDataStride)
						{
							currentState.m_vertexBuffer = state.m_vertexBuffer;
							currentState.m_instanceDataBuffer.idx = state.m_instanceDataBuffer.idx;
							currentState.m_instanceDataOffset = state.m_instanceDataOffset;
							currentState.m_instanceDataStride = state.m_instanceDataStride;

							uint16_t handle = state.m_vertexBuffer.idx;
							if (invalidHandle != handle)
							{
								VertexBuffer& vb = s_renderCtx->m_vertexBuffers[handle];
								GL_CHECK(glBindBuffer(GL_ARRAY_BUFFER, vb.m_id) );
								bindAttribs = true;
							}
							else
							{
								GL_CHECK(glBindBuffer(GL_ARRAY_BUFFER, 0) );
							}
						}

						if (currentState.m_indexBuffer.idx != state.m_indexBuffer.idx)
						{
							currentState.m_indexBuffer = state.m_indexBuffer;

							uint16_t handle = state.m_indexBuffer.idx;
							if (invalidHandle != handle)
							{
								IndexBuffer& ib = s_renderCtx->m_indexBuffers[handle];
								GL_CHECK(glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ib.m_id) );
							}
							else
							{
								GL_CHECK(glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0) );
							}
						}

						if (isValid(currentState.m_vertexBuffer) )
						{
							if (baseVertex != state.m_startVertex
							||  bindAttribs)
							{
								baseVertex = state.m_startVertex;
								const VertexBuffer& vb = s_renderCtx->m_vertexBuffers[state.m_vertexBuffer.idx];
								uint16_t decl = !isValid(vb.m_decl) ? state.m_vertexDecl.idx : vb.m_decl.idx;
								const Program& program = s_renderCtx->m_program[programIdx];
								program.bindAttributes(s_renderCtx->m_vertexDecls[decl], state.m_startVertex);

								if (isValid(state.m_instanceDataBuffer) )
								{
									GL_CHECK(glBindBuffer(GL_ARRAY_BUFFER, s_renderCtx->m_vertexBuffers[state.m_instanceDataBuffer.idx].m_id) );
									program.bindInstanceData(state.m_instanceDataStride, state.m_instanceDataOffset);
								}
							}
						}
					}

					if (isValid(currentState.m_vertexBuffer) )
					{
						uint32_t numVertices = state.m_numVertices;
						if (UINT32_MAX == numVertices)
						{
							const VertexBuffer& vb = s_renderCtx->m_vertexBuffers[currentState.m_vertexBuffer.idx];
							uint16_t decl = !isValid(vb.m_decl) ? state.m_vertexDecl.idx : vb.m_decl.idx;
							const VertexDecl& vertexDecl = s_renderCtx->m_vertexDecls[decl];
							numVertices = vb.m_size/vertexDecl.m_stride;
						}

						uint32_t numIndices = 0;
						uint32_t numPrimsSubmitted = 0;
						uint32_t numInstances = 0;
						uint32_t numPrimsRendered = 0;

						if (isValid(state.m_indexBuffer) )
						{
							if (UINT32_MAX == state.m_numIndices)
							{
								numIndices = s_renderCtx->m_indexBuffers[state.m_indexBuffer.idx].m_size/2;
								numPrimsSubmitted = numIndices/primNumVerts;
								numInstances = state.m_numInstances;
								numPrimsRendered = numPrimsSubmitted*state.m_numInstances;

								GL_CHECK(glDrawElementsInstanced(primType
									, numIndices
									, GL_UNSIGNED_SHORT
									, (void*)0
									, state.m_numInstances
									) );
							}
							else if (primNumVerts <= state.m_numIndices)
							{
								numIndices = state.m_numIndices;
								numPrimsSubmitted = numIndices/primNumVerts;
								numInstances = state.m_numInstances;
								numPrimsRendered = numPrimsSubmitted*state.m_numInstances;

								GL_CHECK(glDrawElementsInstanced(primType
									, numIndices
									, GL_UNSIGNED_SHORT
									, (void*)(uintptr_t)(state.m_startIndex*2)
									, state.m_numInstances
									) );
							}
						}
						else
						{
							numPrimsSubmitted = numVertices/primNumVerts;
							numInstances = state.m_numInstances;
							numPrimsRendered = numPrimsSubmitted*state.m_numInstances;

							GL_CHECK(glDrawArraysInstanced(primType
								, 0
								, numVertices
								, state.m_numInstances
								) );
						}

						statsNumPrimsSubmitted += numPrimsSubmitted;
						statsNumIndices += numIndices;
						statsNumInstances += numInstances;
						statsNumPrimsRendered += numPrimsRendered;
					}
				}
			}

			s_renderCtx->blitMsaaFbo();

			if (0 < m_render->m_num)
			{
				captureElapsed = -bx::getHPCounter();
				s_renderCtx->capture();
				captureElapsed += bx::getHPCounter();
			}
		}

		int64_t now = bx::getHPCounter();
		elapsed += now;

		static int64_t last = now;
		int64_t frameTime = now - last;
		last = now;

		static int64_t min = frameTime;
		static int64_t max = frameTime;
		min = min > frameTime ? frameTime : min;
		max = max < frameTime ? frameTime : max;

		if (m_render->m_debug & (BGFX_DEBUG_IFH|BGFX_DEBUG_STATS) )
		{
			double elapsedGpuMs = 0.0;
#if BGFX_CONFIG_RENDERER_OPENGL
			s_renderCtx->m_queries.end(GL_TIME_ELAPSED);
			uint64_t elapsedGl = s_renderCtx->m_queries.getResult(0);
			elapsedGpuMs = double(elapsedGl)/1e6;
#endif // BGFX_CONFIG_RENDERER_OPENGL

			TextVideoMem& tvm = s_renderCtx->m_textVideoMem;

			static int64_t next = now;

			if (now >= next)
			{
				next = now + bx::getHPFrequency();
				double freq = double(bx::getHPFrequency() );
				double toMs = 1000.0/freq;

				tvm.clear();
				uint16_t pos = 0;
				tvm.printf(0, pos++, BGFX_CONFIG_DEBUG ? 0x89 : 0x8f, " " BGFX_RENDERER_NAME " / " BX_COMPILER_NAME " / " BX_CPU_NAME " / " BX_ARCH_NAME " / " BX_PLATFORM_NAME " ");
				tvm.printf(0, pos++, 0x0f, "      Vendor: %s", s_renderCtx->m_vendor);
				tvm.printf(0, pos++, 0x0f, "    Renderer: %s", s_renderCtx->m_renderer);
				tvm.printf(0, pos++, 0x0f, "     Version: %s", s_renderCtx->m_version);
				tvm.printf(0, pos++, 0x0f, "GLSL version: %s", s_renderCtx->m_glslVersion);

				pos = 10;
				tvm.printf(10, pos++, 0x8e, "      Frame CPU: %7.3f, % 7.3f \x1f, % 7.3f \x1e [ms] / % 6.2f FPS "
					, double(frameTime)*toMs
					, double(min)*toMs
					, double(max)*toMs
					, freq/frameTime
					);

				const uint32_t msaa = (m_resolution.m_flags&BGFX_RESET_MSAA_MASK)>>BGFX_RESET_MSAA_SHIFT;
				tvm.printf(10, pos++, 0x8e, "    Reset flags: [%c] vsync, [%c] MSAAx%d "
					, !!(m_resolution.m_flags&BGFX_RESET_VSYNC) ? '\xfe' : ' '
					, 0 != msaa ? '\xfe' : ' '
					, 1<<msaa
					);

				double elapsedCpuMs = double(elapsed)*toMs;
				tvm.printf(10, pos++, 0x8e, " Draw calls: %4d / CPU %3.4f [ms] %c GPU %3.4f [ms]"
					, m_render->m_num
					, elapsedCpuMs
					, elapsedCpuMs > elapsedGpuMs ? '>' : '<'
					, elapsedGpuMs
					);
				tvm.printf(10, pos++, 0x8e, "      Prims: %7d (#inst: %5d), submitted: %7d"
					, statsNumPrimsRendered
					, statsNumInstances
					, statsNumPrimsSubmitted
					);

				double captureMs = double(captureElapsed)*toMs;
				tvm.printf(10, pos++, 0x8e, "    Capture: %3.4f [ms]", captureMs);

				tvm.printf(10, pos++, 0x8e, "    Indices: %7d", statsNumIndices);
				tvm.printf(10, pos++, 0x8e, "   DVB size: %7d", m_render->m_vboffset);
				tvm.printf(10, pos++, 0x8e, "   DIB size: %7d", m_render->m_iboffset);

#if BGFX_CONFIG_RENDERER_OPENGL
				if (s_extension[Extension::ATI_meminfo].m_supported)
				{
					GLint vboFree[4];
					GL_CHECK(glGetIntegerv(GL_VBO_FREE_MEMORY_ATI, vboFree) );

					GLint texFree[4];
					GL_CHECK(glGetIntegerv(GL_TEXTURE_FREE_MEMORY_ATI, texFree) );

					GLint rbfFree[4];
					GL_CHECK(glGetIntegerv(GL_RENDERBUFFER_FREE_MEMORY_ATI, rbfFree) );

					pos++;
					tvm.printf(10, pos++, 0x8c, " -------------|    free|  free b|     aux|  aux fb");
					tvm.printf(10, pos++, 0x8e, "           VBO: %7d, %7d, %7d, %7d", vboFree[0], vboFree[1], vboFree[2], vboFree[3]);
					tvm.printf(10, pos++, 0x8e, "       Texture: %7d, %7d, %7d, %7d", texFree[0], texFree[1], texFree[2], texFree[3]);
					tvm.printf(10, pos++, 0x8e, " Render Buffer: %7d, %7d, %7d, %7d", rbfFree[0], rbfFree[1], rbfFree[2], rbfFree[3]);
				}
				else if (s_extension[Extension::NVX_gpu_memory_info].m_supported)
				{
					GLint dedicated;
					GL_CHECK(glGetIntegerv(GL_GPU_MEMORY_INFO_DEDICATED_VIDMEM_NVX, &dedicated) );

					GLint totalAvail;
					GL_CHECK(glGetIntegerv(GL_GPU_MEMORY_INFO_TOTAL_AVAILABLE_MEMORY_NVX, &totalAvail) );

					GLint currAvail;
					GL_CHECK(glGetIntegerv(GL_GPU_MEMORY_INFO_CURRENT_AVAILABLE_VIDMEM_NVX, &currAvail) );

					GLint evictedCount;
					GL_CHECK(glGetIntegerv(GL_GPU_MEMORY_INFO_EVICTION_COUNT_NVX, &evictedCount) );

					GLint evictedMemory;
					GL_CHECK(glGetIntegerv(GL_GPU_MEMORY_INFO_EVICTED_MEMORY_NVX, &evictedMemory) );

					pos++;
					tvm.printf(10, pos++, 0x8c, "----------|");
					tvm.printf(10, pos++, 0x8e, " Dedicated: %7d", dedicated);
					tvm.printf(10, pos++, 0x8e, " Available: %7d (%7d)", currAvail, totalAvail);
					tvm.printf(10, pos++, 0x8e, "  Eviction: %7d / %7d", evictedCount, evictedMemory);
				}
#endif // BGFX_CONFIG_RENDERER_OPENGL

				uint8_t attr[2] = { 0x89, 0x8a };
				uint8_t attrIndex = m_render->m_waitSubmit < m_render->m_waitRender;

				pos++;
				tvm.printf(10, pos++, attr[attrIndex&1], "Submit wait: %3.4f [ms]", double(m_render->m_waitSubmit)*toMs);
				tvm.printf(10, pos++, attr[(attrIndex+1)&1], "Render wait: %3.4f [ms]", double(m_render->m_waitRender)*toMs);

				min = frameTime;
				max = frameTime;
			}

			m_textVideoMemBlitter.blit(tvm);
		}
		else if (m_render->m_debug & BGFX_DEBUG_TEXT)
		{
			m_textVideoMemBlitter.blit(m_render->m_textVideoMem);
		}

		GL_CHECK(glFrameTerminatorGREMEDY() );
	}
}

#endif // (BGFX_CONFIG_RENDERER_OPENGLES|BGFX_CONFIG_RENDERER_OPENGL)
