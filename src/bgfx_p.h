/*
 * Copyright 2011-2014 Branimir Karadzic. All rights reserved.
 * License: http://www.opensource.org/licenses/BSD-2-Clause
 */

#ifndef BGFX_P_H_HEADER_GUARD
#define BGFX_P_H_HEADER_GUARD

#include "bgfx.h"
#include "config.h"

#include <inttypes.h>
#include <stdarg.h> // va_list
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <alloca.h>

namespace bgfx
{
	void fatal(Fatal::Enum _code, const char* _format, ...);
	void dbgPrintf(const char* _format, ...);
}

#define _BX_TRACE(_format, ...) \
				do { \
					bgfx::dbgPrintf(BX_FILE_LINE_LITERAL "BGFX " _format "\n", ##__VA_ARGS__); \
				} while(0)

#define _BX_WARN(_condition, _format, ...) \
				do { \
					if (!(_condition) ) \
					{ \
						BX_TRACE("WARN " _format, ##__VA_ARGS__); \
					} \
				} while(0)

#define _BX_CHECK(_condition, _format, ...) \
				do { \
					if (!(_condition) ) \
					{ \
						BX_TRACE("CHECK " _format, ##__VA_ARGS__); \
						bgfx::fatal(bgfx::Fatal::DebugCheck, _format, ##__VA_ARGS__); \
					} \
				} while(0)

#if BGFX_CONFIG_DEBUG
#	define BX_TRACE _BX_TRACE
#	define BX_WARN  _BX_WARN
#	define BX_CHECK _BX_CHECK
#	define BX_CONFIG_ALLOCATOR_DEBUG 1
#endif // BGFX_CONFIG_DEBUG

#define BGFX_FATAL(_condition, _err, _format, ...) \
			do { \
				if (!(_condition) ) \
				{ \
					fatal(_err, _format, ##__VA_ARGS__); \
				} \
			} while(0)

#include <bx/bx.h>
#include <bx/debug.h>
#include <bx/float4x4_t.h>
#include <bx/blockalloc.h>
#include <bx/endian.h>
#include <bx/handlealloc.h>
#include <bx/hash.h>
#include <bx/radixsort.h>
#include <bx/ringbuffer.h>
#include <bx/uint32_t.h>
#include <bx/readerwriter.h>
#include <bx/allocator.h>
#include <bx/string.h>
#include <bx/os.h>

#include "bgfxplatform.h"
#include "image.h"

#define BGFX_CHUNK_MAGIC_FSH BX_MAKEFOURCC('F', 'S', 'H', 0x2)
#define BGFX_CHUNK_MAGIC_TEX BX_MAKEFOURCC('T', 'E', 'X', 0x0)
#define BGFX_CHUNK_MAGIC_VSH BX_MAKEFOURCC('V', 'S', 'H', 0x2)

#include <list> // mingw wants it to be before tr1/unordered_*...

#if BGFX_CONFIG_USE_TINYSTL
namespace bgfx
{
	struct TinyStlAllocator
	{
		static void* static_allocate(size_t _bytes);
		static void static_deallocate(void* _ptr, size_t /*_bytes*/);
	};
} // namespace bgfx
#	define TINYSTL_ALLOCATOR bgfx::TinyStlAllocator
#	include <tinystl/string.h>
#	include <tinystl/unordered_map.h>
#	include <tinystl/unordered_set.h>
namespace stl = tinystl;
#else
#	include <string>
#	include <unordered_map>
#	include <unordered_set>
namespace stl
{
	using namespace std;
	using namespace std::tr1;
}
#endif // BGFX_CONFIG_USE_TINYSTL

#if BX_PLATFORM_ANDROID
#	include <android/native_window.h>
#elif BX_PLATFORM_WINDOWS
#	include <windows.h>
#elif BX_PLATFORM_XBOX360
#	include <malloc.h>
#	include <xtl.h>
#endif // BX_PLATFORM_*

#include <bx/cpu.h>
#include <bx/thread.h>
#include <bx/timer.h>

#include "vertexdecl.h"

#define BGFX_DEFAULT_WIDTH  1280
#define BGFX_DEFAULT_HEIGHT 720

#define BGFX_STATE_TEX0      UINT64_C(0x0100000000000000)
#define BGFX_STATE_TEX1      UINT64_C(0x0200000000000000)
#define BGFX_STATE_TEX2      UINT64_C(0x0400000000000000)
#define BGFX_STATE_TEX3      UINT64_C(0x0800000000000000)
#define BGFX_STATE_TEX4      UINT64_C(0x1000000000000000)
#define BGFX_STATE_TEX5      UINT64_C(0x2000000000000000)
#define BGFX_STATE_TEX6      UINT64_C(0x4000000000000000)
#define BGFX_STATE_TEX7      UINT64_C(0x8000000000000000)
#define BGFX_STATE_TEX_MASK  UINT64_C(0xff00000000000000)
#define BGFX_STATE_TEX_COUNT 8

#define BGFX_SAMPLER_DEFAULT_FLAGS UINT32_C(0x10000000)

#if BGFX_CONFIG_RENDERER_DIRECT3D9
#	define BGFX_RENDERER_NAME "Direct3D 9"
#elif BGFX_CONFIG_RENDERER_DIRECT3D11
#	define BGFX_RENDERER_NAME "Direct3D 11"
#elif BGFX_CONFIG_RENDERER_OPENGL
#	if BGFX_CONFIG_RENDERER_OPENGL >= 31
#		define BGFX_RENDERER_NAME "OpenGL 3.1"
#	else
#		define BGFX_RENDERER_NAME "OpenGL 2.1"
#	endif // BGFX_CONFIG_RENDERER_OPENGL
#elif BGFX_CONFIG_RENDERER_OPENGLES
#	if BGFX_CONFIG_RENDERER_OPENGLES == 30
#		define BGFX_RENDERER_NAME "OpenGL ES 3.0"
#	elif BGFX_CONFIG_RENDERER_OPENGLES == 31
#		define BGFX_RENDERER_NAME "OpenGL ES 3.1"
#	else
#		define BGFX_RENDERER_NAME "OpenGL ES 2.0"
#	endif // BGFX_CONFIG_RENDERER_OPENGLES
#else
#	define BGFX_RENDERER_NAME "NULL"
#endif // BGFX_CONFIG_RENDERER_

namespace bgfx
{
#if BX_PLATFORM_ANDROID
	extern ::ANativeWindow* g_bgfxAndroidWindow;
#elif BX_PLATFORM_IOS
	extern void* g_bgfxEaglLayer;
#elif BX_PLATFORM_OSX
	extern void* g_bgfxNSWindow;
#elif BX_PLATFORM_WINDOWS
	extern ::HWND g_bgfxHwnd;
#endif // BX_PLATFORM_*

	struct Clear
	{
		uint32_t m_rgba;
		float m_depth;
		uint8_t m_stencil;
		uint8_t m_flags;
	};

	struct Rect
	{
		void clear()
		{
			m_x =
			m_y =
			m_width =
			m_height = 0;
		}

		bool isZero() const
		{
			uint64_t ui64 = *( (uint64_t*)this);
			return UINT64_C(0) == ui64;
		}

		void intersect(const Rect& _a, const Rect& _b)
		{
			using namespace bx;
			const uint16_t sx = uint16_max(_a.m_x, _b.m_x);
			const uint16_t sy = uint16_max(_a.m_y, _b.m_y);
			const uint16_t ex = uint16_min(_a.m_x + _a.m_width,  _b.m_x + _b.m_width );
			const uint16_t ey = uint16_min(_a.m_y + _a.m_height, _b.m_y + _b.m_height);
			m_x = sx;
			m_y = sy;
			m_width  = (uint16_t)uint32_satsub(ex, sx);
			m_height = (uint16_t)uint32_satsub(ey, sy);
		}

		uint16_t m_x;
		uint16_t m_y;
		uint16_t m_width;
		uint16_t m_height;
	};

	struct TextureCreate
	{
		uint32_t m_flags;
		uint16_t m_width;
		uint16_t m_height;
		uint16_t m_sides;
		uint16_t m_depth;
		uint8_t m_numMips;
		uint8_t m_format;
		bool m_cubeMap;
		const Memory* m_mem;
	};

	extern const uint32_t g_uniformTypeSize[UniformType::Count+1];
	extern CallbackI* g_callback;
	extern bx::ReallocatorI* g_allocator;
	extern Caps g_caps;

	void setGraphicsDebuggerPresent(bool _present);
	bool isGraphicsDebuggerPresent();
	void release(const Memory* _mem);
	const char* getAttribName(Attrib::Enum _attr);

	inline uint32_t gcd(uint32_t _a, uint32_t _b)
	{
		do
		{
			uint32_t tmp = _a % _b;
			_a = _b;
			_b = tmp;
		}
		while (_b);

		return _a;
	}

	inline uint32_t lcm(uint32_t _a, uint32_t _b)
	{
		return _a * (_b / gcd(_a, _b) );
	}

	inline uint32_t strideAlign(uint32_t _offset, uint32_t _stride)
	{
		using namespace bx;
		const uint32_t mod    = uint32_mod(_offset, _stride);
		const uint32_t add    = uint32_sub(_stride, mod);
		const uint32_t mask   = uint32_cmpeq(mod, 0);
		const uint32_t tmp    = uint32_selb(mask, 0, add);
		const uint32_t result = uint32_add(_offset, tmp);

		return result;
	}

	inline uint32_t strideAlign16(uint32_t _offset, uint32_t _stride)
	{
		uint32_t align = lcm(16, _stride);
		return _offset+align-(_offset%align);
	}

	inline uint32_t strideAlign256(uint32_t _offset, uint32_t _stride)
	{
		uint32_t align = lcm(256, _stride);
		return _offset+align-(_offset%align);
	}

	inline uint32_t castfu(float _value)
	{
		union {	float fl; uint32_t ui; } un;
		un.fl = _value;
		return un.ui;
	}

	inline uint64_t packStencil(uint32_t _fstencil, uint32_t _bstencil)
	{
		return (uint64_t(_bstencil)<<32)|uint64_t(_fstencil);
	}

	inline uint32_t unpackStencil(uint8_t _0or1, uint64_t _stencil)
	{
		return uint32_t( (_stencil >> (32*_0or1) ) );
	}

	void dump(const VertexDecl& _decl);

	struct TextVideoMem
	{
		TextVideoMem()
			: m_mem(NULL)
			, m_size(0)
			, m_width(0)
			, m_height(0)
			, m_small(false)
		{
			resize();
			clear();
		}

		~TextVideoMem()
		{
			BX_FREE(g_allocator, m_mem);
		}

		void resize(bool _small = false, uint16_t _width = BGFX_DEFAULT_WIDTH, uint16_t _height = BGFX_DEFAULT_HEIGHT)
		{
			uint32_t width = bx::uint32_max(1, _width/8);
			uint32_t height = bx::uint32_max(1, _height/(_small ? 8 : 16) );

			if (NULL == m_mem
			||  m_width != width
			||  m_height != height
			||  m_small != _small)
			{
				m_small = _small;
				m_width = (uint16_t)width;
				m_height = (uint16_t)height;

				uint32_t size = m_size;
				m_size = m_width * m_height * 2;

				m_mem = (uint8_t*)BX_REALLOC(g_allocator, m_mem, m_size);

				if (size < m_size)
				{
					memset(&m_mem[size], 0, m_size-size);
				}
			}
		}

		void clear(uint8_t _attr = 0)
		{
			uint8_t* mem = m_mem;
			for (uint32_t ii = 0, num = m_size/2; ii < num; ++ii)
			{
				mem[0] = 0;
				mem[1] = _attr;
				mem += 2;
			}
		}

		void printfVargs(uint16_t _x, uint16_t _y, uint8_t _attr, const char* _format, va_list _argList)
		{
			if (_x < m_width && _y < m_height)
			{
				char* temp = (char*)alloca(m_width);

				uint32_t num = bx::vsnprintf(temp, m_width, _format, _argList);

				uint8_t* mem = &m_mem[(_y*m_width+_x)*2];
				for (uint32_t ii = 0, xx = _x; ii < num && xx < m_width; ++ii, ++xx)
				{
					mem[0] = temp[ii];
					mem[1] = _attr;
					mem += 2;
				}
			}
		}

		void printf(uint16_t _x, uint16_t _y, uint8_t _attr, const char* _format, ...)
		{
			va_list argList;
			va_start(argList, _format);
			printfVargs(_x, _y, _attr, _format, argList);
			va_end(argList);
		}

		uint8_t* m_mem;
		uint32_t m_size;
		uint16_t m_width;
		uint16_t m_height;
		bool m_small;
	};

	struct TextVideoMemBlitter
	{
		void init();
		void shutdown();

		void blit(const TextVideoMem* _mem)
		{
			blit(*_mem);
		}

		void blit(const TextVideoMem& _mem);
		void setup();
		void render(uint32_t _numIndices);

		TextureHandle m_texture;
		TransientVertexBuffer* m_vb;
		TransientIndexBuffer* m_ib;
		VertexDecl m_decl;
		ProgramHandle m_program;
		bool m_init;
	};

	template <uint32_t maxKeys>
	struct UpdateBatchT
	{
		UpdateBatchT()
			: m_num(0)
		{
		}

		void add(uint32_t _key, uint32_t _value)
		{
			uint32_t num = m_num++;
			m_keys[num] = _key;
			m_values[num] = _value;
		}

		bool sort()
		{
			if (0 < m_num)
			{
				uint32_t* tempKeys = (uint32_t*)alloca(sizeof(m_keys) );
				uint32_t* tempValues = (uint32_t*)alloca(sizeof(m_values) );
				bx::radixSort32(m_keys, tempKeys, m_values, tempValues, m_num);
				return true;
			}

			return false;
		}

		bool isFull() const
		{
			return m_num >= maxKeys;
		}

		void reset()
		{
			m_num = 0;
		}

		uint32_t m_num;
		uint32_t m_keys[maxKeys];
		uint32_t m_values[maxKeys];
	};

	struct ClearQuad
	{
		void init();
		void shutdown();
		void clear(const Rect& _rect, const Clear& _clear, uint32_t _height = 0);

		TransientVertexBuffer* m_vb;
		IndexBufferHandle m_ib;
		VertexDecl m_decl;
		ProgramHandle m_program[BGFX_CONFIG_MAX_FRAME_BUFFER_ATTACHMENTS];
	};

	struct PredefinedUniform
	{
		enum Enum
		{
			ViewRect,
			ViewTexel,
			View,
			ViewProj,
			ViewProjX,
			Model,
			ModelView,
			ModelViewProj,
			ModelViewProjX,
			AlphaRef,
			Count
		};

		uint32_t m_loc;
		uint16_t m_count;
		uint8_t m_type;
	};

	const char* getUniformTypeName(UniformType::Enum _enum);
	UniformType::Enum nameToUniformTypeEnum(const char* _name);
	const char* getPredefinedUniformName(PredefinedUniform::Enum _enum);
	PredefinedUniform::Enum nameToPredefinedUniformEnum(const char* _name);

	struct CommandBuffer
	{
		CommandBuffer()
			: m_pos(0)
			, m_size(BGFX_CONFIG_MAX_COMMAND_BUFFER_SIZE)
		{
			finish();
		}

		enum Enum
		{
			RendererInit,
			RendererShutdownBegin,
			CreateVertexDecl,
			CreateIndexBuffer,
			CreateVertexBuffer,
			CreateDynamicIndexBuffer,
			UpdateDynamicIndexBuffer,
			CreateDynamicVertexBuffer,
			UpdateDynamicVertexBuffer,
			CreateShader,
			CreateProgram,
			CreateTexture,
			UpdateTexture,
			CreateFrameBuffer,
			CreateUniform,
			UpdateViewName,
			End,
			RendererShutdownEnd,
			DestroyVertexDecl,
			DestroyIndexBuffer,
			DestroyVertexBuffer,
			DestroyDynamicIndexBuffer,
			DestroyDynamicVertexBuffer,
			DestroyShader,
			DestroyProgram,
			DestroyTexture,
			DestroyFrameBuffer,
			DestroyUniform,
			SaveScreenShot,
		};

		void write(const void* _data, uint32_t _size)
		{
			BX_CHECK(m_size == BGFX_CONFIG_MAX_COMMAND_BUFFER_SIZE, "Called write outside start/finish?");
			BX_CHECK(m_pos < m_size, "");
			memcpy(&m_buffer[m_pos], _data, _size);
			m_pos += _size;
		}

		template<typename Type>
		void write(const Type& _in)
		{
			align(BX_ALIGNOF(Type) );
			write(reinterpret_cast<const uint8_t*>(&_in), sizeof(Type) );
		}

		void read(void* _data, uint32_t _size)
		{
			BX_CHECK(m_pos < m_size, "");
			memcpy(_data, &m_buffer[m_pos], _size);
			m_pos += _size;
		}

		template<typename Type>
		void read(Type& _in)
		{
			align(BX_ALIGNOF(Type) );
			read(reinterpret_cast<uint8_t*>(&_in), sizeof(Type) );
		}

		const uint8_t* skip(uint32_t _size)
		{
			BX_CHECK(m_pos < m_size, "");
			const uint8_t* result = &m_buffer[m_pos];
			m_pos += _size;
			return result;
		}

		template<typename Type>
		void skip()
		{
			align(BX_ALIGNOF(Type) );
			skip(sizeof(Type) );
		}

		void align(uint32_t _alignment)
		{
			const uint32_t mask = _alignment-1;
			const uint32_t pos = (m_pos+mask) & (~mask);
			m_pos = pos;
		}

		void reset()
		{
			m_pos = 0;
		}

		void start()
		{
			m_pos = 0;
			m_size = BGFX_CONFIG_MAX_COMMAND_BUFFER_SIZE;
		}

		void finish()
		{
			uint8_t cmd = End;
			write(cmd);
			m_size = m_pos;
			m_pos = 0;
		}

		uint32_t m_pos;
		uint32_t m_size;
		uint8_t m_buffer[BGFX_CONFIG_MAX_COMMAND_BUFFER_SIZE];

	private:
		CommandBuffer(const CommandBuffer&);
		void operator=(const CommandBuffer&);
	};

	struct SortKey
	{
		uint64_t encode()
		{
			// |               3               2               1               0|
			// |fedcba9876543210fedcba9876543210fedcba9876543210fedcba9876543210|
			// |             vvvvvsssssssssssttmmmmmmmmmdddddddddddddddddddddddd|
			// |                 ^          ^ ^        ^                       ^|
			// |                 |          | |        |                       ||

			const uint64_t tmp0 = m_depth;
			const uint64_t tmp1 = uint64_t(m_program)<<0x18;
			const uint64_t tmp2 = uint64_t(m_trans  )<<0x21;
			const uint64_t tmp3 = uint64_t(m_seq    )<<0x23;
			const uint64_t tmp4 = uint64_t(m_view   )<<0x2e;
			const uint64_t key  = tmp0|tmp1|tmp2|tmp3|tmp4;
			return key;
		}

		void decode(uint64_t _key)
		{
			m_depth   =  _key       & 0xffffffff;
			m_program = (_key>>0x18)&(BGFX_CONFIG_MAX_PROGRAMS-1);
			m_trans   = (_key>>0x21)& 0x3;
			m_seq     = (_key>>0x23)& 0x7ff;
			m_view    = (_key>>0x2e)&(BGFX_CONFIG_MAX_VIEWS-1);
		}

		void reset()
		{
			m_depth   = 0;
			m_program = 0;
			m_seq     = 0;
			m_view    = 0;
			m_trans   = 0;
		}

		int32_t m_depth;
		uint16_t m_program;
		uint16_t m_seq;
		uint8_t m_view;
		uint8_t m_trans;
	};

	BX_ALIGN_STRUCT_16(struct) Matrix4
	{
		union
		{
			float val[16];
			bx::float4x4_t f4x4;
		} un;

		void setIdentity()
		{
			memset(un.val, 0, sizeof(un.val) );
			un.val[0] = un.val[5] = un.val[10] = un.val[15] = 1.0f;
		}
	};

	void mtxOrtho(float* _result, float _left, float _right, float _bottom, float _top, float _near, float _far);

	struct MatrixCache
	{
		MatrixCache()
			: m_num(1)
		{
			m_cache[0].setIdentity();
		}

		void reset()
		{
			m_num = 1;
		}

		uint32_t add(const void* _mtx, uint16_t _num)
		{
			if (NULL != _mtx)
			{
				BX_CHECK(m_num+_num < BGFX_CONFIG_MAX_MATRIX_CACHE, "Matrix cache overflow. %d (max: %d)", m_num+_num, BGFX_CONFIG_MAX_MATRIX_CACHE);

				uint32_t num = bx::uint32_min(BGFX_CONFIG_MAX_MATRIX_CACHE-m_num, _num);
				uint32_t first = m_num;
				memcpy(&m_cache[m_num], _mtx, sizeof(Matrix4)*num);
				m_num += num;
				return first;
			}

			return 0;
		}

		Matrix4 m_cache[BGFX_CONFIG_MAX_MATRIX_CACHE];
		uint32_t m_num;
	};

	struct RectCache
	{
		RectCache()
			: m_num(0)
		{
		}

		void reset()
		{
			m_num = 0;
		}

		uint32_t add(uint16_t _x, uint16_t _y, uint16_t _width, uint16_t _height)
		{
			BX_CHECK(m_num+1 < BGFX_CONFIG_MAX_RECT_CACHE, "Rect cache overflow. %d (max: %d)", m_num, BGFX_CONFIG_MAX_RECT_CACHE);

			uint32_t first = m_num;
			Rect& rect = m_cache[m_num];

			rect.m_x = _x;
			rect.m_y = _y;
			rect.m_width = _width;
			rect.m_height = _height;

			m_num++;
			return first;
		}

		Rect m_cache[BGFX_CONFIG_MAX_RECT_CACHE];
		uint32_t m_num;
	};

	struct Sampler
	{
		uint32_t m_flags;
		uint16_t m_idx;
	};

#define CONSTANT_OPCODE_TYPE_SHIFT 27
#define CONSTANT_OPCODE_TYPE_MASK  UINT32_C(0xf8000000)
#define CONSTANT_OPCODE_LOC_SHIFT  11
#define CONSTANT_OPCODE_LOC_MASK   UINT32_C(0x07fff800)
#define CONSTANT_OPCODE_NUM_SHIFT  1
#define CONSTANT_OPCODE_NUM_MASK   UINT32_C(0x000007fe)
#define CONSTANT_OPCODE_COPY_SHIFT 0
#define CONSTANT_OPCODE_COPY_MASK  UINT32_C(0x00000001)

#define BGFX_UNIFORM_FRAGMENTBIT UINT8_C(0x10)

	class ConstantBuffer
	{
	public:
		static ConstantBuffer* create(uint32_t _size)
		{
			uint32_t size = BX_ALIGN_16(bx::uint32_max(_size, sizeof(ConstantBuffer) ) );
			void* data = BX_ALLOC(g_allocator, size);
			return ::new(data) ConstantBuffer(_size);
		}

		static void destroy(ConstantBuffer* _constantBuffer)
		{
			_constantBuffer->~ConstantBuffer();
			BX_FREE(g_allocator, _constantBuffer);
		}

		static uint32_t encodeOpcode(UniformType::Enum _type, uint16_t _loc, uint16_t _num, uint16_t _copy)
		{
			const uint32_t type = _type << CONSTANT_OPCODE_TYPE_SHIFT;
			const uint32_t loc  = _loc  << CONSTANT_OPCODE_LOC_SHIFT;
			const uint32_t num  = _num  << CONSTANT_OPCODE_NUM_SHIFT;
			const uint32_t copy = _copy << CONSTANT_OPCODE_COPY_SHIFT;
			return type|loc|num|copy;
		}

		static void decodeOpcode(uint32_t _opcode, UniformType::Enum& _type, uint16_t& _loc, uint16_t& _num, uint16_t& _copy)
		{
			const uint32_t type = (_opcode&CONSTANT_OPCODE_TYPE_MASK) >> CONSTANT_OPCODE_TYPE_SHIFT;
			const uint32_t loc  = (_opcode&CONSTANT_OPCODE_LOC_MASK)  >> CONSTANT_OPCODE_LOC_SHIFT;
			const uint32_t num  = (_opcode&CONSTANT_OPCODE_NUM_MASK)  >> CONSTANT_OPCODE_NUM_SHIFT;
			const uint32_t copy = (_opcode&CONSTANT_OPCODE_COPY_MASK) >> CONSTANT_OPCODE_COPY_SHIFT;

			_type = (UniformType::Enum)(type);
			_copy = (uint16_t)copy;
			_num = (uint16_t)num;
			_loc = (uint16_t)loc;
		}

		void write(const void* _data, uint32_t _size)
		{
			BX_CHECK(m_pos + _size < m_size, "Write would go out of bounds. pos %d + size %d > max size: %d).", m_pos, _size, m_size);

			if (m_pos + _size < m_size)
			{
				memcpy(&m_buffer[m_pos], _data, _size);
				m_pos += _size;
			}
		}

		void write(uint32_t _value)
		{
			write(&_value, sizeof(uint32_t) );
		}

		const char* read(uint32_t _size)
		{
			BX_CHECK(m_pos < m_size, "Out of bounds %d (size: %d).", m_pos, m_size);
			const char* result = &m_buffer[m_pos];
			m_pos += _size;
			return result;
		}

		uint32_t read()
		{
			const char* result = read(sizeof(uint32_t) );
			return *( (uint32_t*)result);
		}

		bool isEmpty() const
		{
			return 0 == m_pos;
		}

		uint32_t getPos() const
		{
			return m_pos;
		}

		void reset(uint32_t _pos = 0)
		{
			m_pos = _pos;
		}

		void finish()
		{
			write(UniformType::End);
			m_pos = 0;
		}

		void writeUniform(UniformType::Enum _type, uint16_t _loc, const void* _value, uint16_t _num = 1);
		void writeUniformHandle(UniformType::Enum _type, uint16_t _loc, UniformHandle _handle, uint16_t _num = 1);
		void writeMarker(const char* _marker);
		void commit();

	private:
		ConstantBuffer(uint32_t _size)
			: m_size(_size-sizeof(m_buffer) )
			, m_pos(0)
		{
			finish();
		}

		~ConstantBuffer()
		{
		}

		uint32_t m_size;
		uint32_t m_pos;
		char m_buffer[8];
	};

	typedef const void* (*UniformFn)(const void* _data);

	struct UniformInfo
	{
		const void* m_data;
		UniformFn m_func;
		UniformHandle m_handle;
	};

 	class UniformRegistry
 	{
	public:
		UniformRegistry()
		{
		}

		~UniformRegistry()
		{
		}

 		const UniformInfo* find(const char* _name) const
 		{
			UniformHashMap::const_iterator it = m_uniforms.find(_name);
			if (it != m_uniforms.end() )
			{
				return &it->second;
			}

 			return NULL;
 		}

		const UniformInfo& add(UniformHandle _handle, const char* _name, const void* _data, UniformFn _func = NULL)
		{
			UniformHashMap::iterator it = m_uniforms.find(_name);
			if (it == m_uniforms.end() )
			{
				UniformInfo info;
				info.m_data   = _data;
				info.m_func   = _func;
				info.m_handle = _handle;

				stl::pair<UniformHashMap::iterator, bool> result = m_uniforms.insert(UniformHashMap::value_type(_name, info) );
				return result.first->second;
			}

			UniformInfo& info = it->second;
			info.m_data   = _data;
			info.m_func   = _func;
			info.m_handle = _handle;

			return info;
		}

 	private:
 		typedef stl::unordered_map<stl::string, UniformInfo> UniformHashMap;
 		UniformHashMap m_uniforms;
 	};

	struct RenderState
	{
		void reset()
		{
			m_constEnd = 0;
			clear();
		}

		void clear()
		{
			m_constBegin = m_constEnd;
			m_flags = BGFX_STATE_DEFAULT;
			m_stencil = packStencil(BGFX_STENCIL_DEFAULT, BGFX_STENCIL_DEFAULT);
			m_rgba = 0;
			m_matrix = 0;
			m_startIndex = 0;
			m_numIndices = UINT32_MAX;
			m_startVertex = 0;
			m_numVertices = UINT32_MAX;
			m_instanceDataOffset = 0;
			m_instanceDataStride = 0;
			m_numInstances = 1;
			m_num = 1;
			m_scissor = UINT16_MAX;
			m_vertexBuffer.idx = invalidHandle;
			m_vertexDecl.idx = invalidHandle;
			m_indexBuffer.idx = invalidHandle;
			m_instanceDataBuffer.idx = invalidHandle;

			for (uint32_t ii = 0; ii < BGFX_STATE_TEX_COUNT; ++ii)
			{
				m_sampler[ii].m_idx = invalidHandle;
				m_sampler[ii].m_flags = 0;
			}
		}

		uint64_t m_flags;
		uint64_t m_stencil;
		uint32_t m_rgba;
		uint32_t m_constBegin;
		uint32_t m_constEnd;
		uint32_t m_matrix;
		uint32_t m_startIndex;
		uint32_t m_numIndices;
		uint32_t m_startVertex;
		uint32_t m_numVertices;
		uint32_t m_instanceDataOffset;
		uint16_t m_instanceDataStride;
		uint16_t m_numInstances;
		uint16_t m_num;
		uint16_t m_scissor;

		VertexBufferHandle m_vertexBuffer;
		VertexDeclHandle m_vertexDecl;
		IndexBufferHandle m_indexBuffer;
		VertexBufferHandle m_instanceDataBuffer;
		Sampler m_sampler[BGFX_STATE_TEX_COUNT];
	};

	struct Resolution
	{
		Resolution()
			: m_width(BGFX_DEFAULT_WIDTH)
			, m_height(BGFX_DEFAULT_HEIGHT)
			, m_flags(BGFX_RESET_NONE)
		{
		}

		uint32_t m_width;
		uint32_t m_height;
		uint32_t m_flags;
	};

	struct DynamicIndexBuffer
	{
		IndexBufferHandle m_handle;
		uint32_t m_offset;
		uint32_t m_size;
	};

	struct DynamicVertexBuffer
	{
		VertexBufferHandle m_handle;
		uint32_t m_offset;
		uint32_t m_size;
		uint32_t m_startVertex;
		uint32_t m_numVertices;
		uint32_t m_stride;
		VertexDeclHandle m_decl;
	};

	struct Frame
	{
		BX_CACHE_LINE_ALIGN_MARKER();

		Frame()
			: m_waitSubmit(0)
			, m_waitRender(0)
		{
		}

		~Frame()
		{
		}

		void create()
		{
			m_constantBuffer = ConstantBuffer::create(BGFX_CONFIG_MAX_CONSTANT_BUFFER_SIZE);
			reset();
			start();
			m_textVideoMem = BX_NEW(g_allocator, TextVideoMem);
		}

		void destroy()
		{
			ConstantBuffer::destroy(m_constantBuffer);
			BX_DELETE(g_allocator, m_textVideoMem);
		}

		void reset()
		{
			start();
			finish();
			resetFreeHandles();
		}

		void start()
		{
			m_flags = BGFX_STATE_NONE;
			m_state.reset();
			m_matrixCache.reset();
			m_rectCache.reset();
			m_key.reset();
			m_num = 0;
			m_numRenderStates = 0;
			m_numDropped = 0;
			m_iboffset = 0;
			m_vboffset = 0;
			m_cmdPre.start();
			m_cmdPost.start();
			m_constantBuffer->reset();
			m_discard = false;
		}

		void finish()
		{
			m_cmdPre.finish();
			m_cmdPost.finish();

			m_constantBuffer->finish();

			if (0 < m_numDropped)
			{
				BX_TRACE("Too many draw calls: %d, dropped %d (max: %d)"
					, m_num+m_numDropped
					, m_numDropped
					, BGFX_CONFIG_MAX_DRAW_CALLS
					);
			}
		}

		void setMarker(const char* _name)
		{
			m_constantBuffer->writeMarker(_name);
		}

		void setState(uint64_t _state, uint32_t _rgba)
		{
			uint8_t blend = ( (_state&BGFX_STATE_BLEND_MASK)>>BGFX_STATE_BLEND_SHIFT)&0xff;
			// transparency sort order table
			m_key.m_trans = "\x0\x1\x1\x2\x2\x1\x2\x1\x2\x1\x1\x1\x1\x1\x1\x1\x1\x1\x1"[( (blend)&0xf) + (!!blend)];
			m_state.m_flags = _state;
			m_state.m_rgba = _rgba;
		}

		void setStencil(uint32_t _fstencil, uint32_t _bstencil)
		{
			m_state.m_stencil = packStencil(_fstencil, _bstencil);
		}

		uint16_t setScissor(uint16_t _x, uint16_t _y, uint16_t _width, uint16_t _height)
		{
			uint16_t scissor = (uint16_t)m_rectCache.add(_x, _y, _width, _height);
			m_state.m_scissor = scissor;
			return scissor;
		}

		void setScissor(uint16_t _cache)
		{
			m_state.m_scissor = _cache;
		}

		uint32_t setTransform(const void* _mtx, uint16_t _num)
		{
			m_state.m_matrix = m_matrixCache.add(_mtx, _num);
			m_state.m_num = _num;

			return m_state.m_matrix;
		}

		void setTransform(uint32_t _cache, uint16_t _num)
		{
			m_state.m_matrix = _cache;
			m_state.m_num = _num;
		}

		void setIndexBuffer(IndexBufferHandle _handle, uint32_t _firstIndex, uint32_t _numIndices)
		{
			m_state.m_startIndex = _firstIndex;
			m_state.m_numIndices = _numIndices;
			m_state.m_indexBuffer = _handle;
		}

		void setIndexBuffer(const TransientIndexBuffer* _tib, uint32_t _numIndices)
		{
			m_state.m_indexBuffer = _tib->handle;
			m_state.m_startIndex = _tib->startIndex;
			m_state.m_numIndices = _numIndices;
			m_discard = 0 == _numIndices;
		}

		void setVertexBuffer(VertexBufferHandle _handle, uint32_t _startVertex, uint32_t _numVertices)
		{
			BX_CHECK(_handle.idx < BGFX_CONFIG_MAX_VERTEX_BUFFERS, "Invalid vertex buffer handle. %d (< %d)", _handle.idx, BGFX_CONFIG_MAX_VERTEX_BUFFERS);
			m_state.m_startVertex = _startVertex;
			m_state.m_numVertices = _numVertices;
			m_state.m_vertexBuffer = _handle;
		}

		void setVertexBuffer(const DynamicVertexBuffer& _dvb, uint32_t _numVertices)
		{
			m_state.m_startVertex = _dvb.m_startVertex;
			m_state.m_numVertices = bx::uint32_min(_dvb.m_numVertices, _numVertices);
			m_state.m_vertexBuffer = _dvb.m_handle;
			m_state.m_vertexDecl = _dvb.m_decl;
		}

		void setVertexBuffer(const TransientVertexBuffer* _tvb, uint32_t _numVertices)
		{
			m_state.m_startVertex = _tvb->startVertex;
			m_state.m_numVertices = bx::uint32_min(_tvb->size/_tvb->stride, _numVertices);
			m_state.m_vertexBuffer = _tvb->handle;
			m_state.m_vertexDecl = _tvb->decl;
		}

		void setInstanceDataBuffer(const InstanceDataBuffer* _idb, uint16_t _num)
		{
 			m_state.m_instanceDataOffset = _idb->offset;
			m_state.m_instanceDataStride = _idb->stride;
			m_state.m_numInstances = bx::uint16_min( (uint16_t)_idb->num, _num);
			m_state.m_instanceDataBuffer = _idb->handle;
			BX_FREE(g_allocator, const_cast<InstanceDataBuffer*>(_idb) );
		}

		void setProgram(ProgramHandle _handle)
		{
			BX_CHECK(isValid(_handle), "Can't set program with invalid handle.");
			m_key.m_program = _handle.idx;
		}

		void setTexture(uint8_t _stage, UniformHandle _sampler, TextureHandle _handle, uint32_t _flags)
		{
			m_flags |= BGFX_STATE_TEX0<<_stage;
			Sampler& sampler = m_state.m_sampler[_stage];
			sampler.m_idx = _handle.idx;
			sampler.m_flags = (_flags&BGFX_SAMPLER_DEFAULT_FLAGS) ? BGFX_SAMPLER_DEFAULT_FLAGS : _flags;

			if (isValid(_sampler)
			&& (BX_ENABLED(BGFX_CONFIG_RENDERER_OPENGL) || BX_ENABLED(BGFX_CONFIG_RENDERER_OPENGLES) ) )
			{
				uint32_t stage = _stage;
				setUniform(_sampler, &stage);
			}
		}

		void discard()
		{
			m_discard = false;
			m_state.clear();
			m_flags = BGFX_STATE_NONE;
		}

		uint32_t submit(uint8_t _id, int32_t _depth);
		uint32_t submitMask(uint32_t _viewMask, int32_t _depth);
		void sort();

		bool checkAvailTransientIndexBuffer(uint32_t _num)
		{
			uint32_t offset = m_iboffset;
			uint32_t iboffset = offset + _num*sizeof(uint16_t);
			iboffset = bx::uint32_min(iboffset, BGFX_CONFIG_TRANSIENT_INDEX_BUFFER_SIZE);
			uint32_t num = (iboffset-offset)/sizeof(uint16_t);
			return num == _num;
		}

		uint32_t allocTransientIndexBuffer(uint32_t& _num)
		{
			uint32_t offset = m_iboffset;
			m_iboffset = offset + _num*sizeof(uint16_t);
			m_iboffset = bx::uint32_min(m_iboffset, BGFX_CONFIG_TRANSIENT_INDEX_BUFFER_SIZE);
			_num = (m_iboffset-offset)/sizeof(uint16_t);
			return offset;
		}

		bool checkAvailTransientVertexBuffer(uint32_t _num, uint16_t _stride)
		{
			uint32_t offset = strideAlign(m_vboffset, _stride);
			uint32_t vboffset = offset + _num * _stride;
			vboffset = bx::uint32_min(vboffset, BGFX_CONFIG_TRANSIENT_VERTEX_BUFFER_SIZE);
			uint32_t num = (vboffset-offset)/_stride;
			return num == _num;
		}

		uint32_t allocTransientVertexBuffer(uint32_t& _num, uint16_t _stride)
		{
			uint32_t offset = strideAlign(m_vboffset, _stride);
			m_vboffset = offset + _num * _stride;
			m_vboffset = bx::uint32_min(m_vboffset, BGFX_CONFIG_TRANSIENT_VERTEX_BUFFER_SIZE);
			_num = (m_vboffset-offset)/_stride;
			return offset;
		}

		void writeUniform(UniformType::Enum _type, UniformHandle _handle, const void* _value, uint16_t _num)
		{
			m_constantBuffer->writeUniform(_type, _handle.idx, _value, _num);
		}

		void free(IndexBufferHandle _handle)
		{
			m_freeIndexBufferHandle[m_numFreeIndexBufferHandles] = _handle;
			++m_numFreeIndexBufferHandles;
		}

		void free(VertexDeclHandle _handle)
		{
			m_freeVertexDeclHandle[m_numFreeVertexDeclHandles] = _handle;
			++m_numFreeVertexDeclHandles;
		}

		void free(VertexBufferHandle _handle)
		{
			m_freeVertexBufferHandle[m_numFreeVertexBufferHandles] = _handle;
			++m_numFreeVertexBufferHandles;
		}

		void free(ShaderHandle _handle)
		{
			m_freeShaderHandle[m_numFreeShaderHandles] = _handle;
			++m_numFreeShaderHandles;
		}

		void free(ProgramHandle _handle)
		{
			m_freeProgramHandle[m_numFreeProgramHandles] = _handle;
			++m_numFreeProgramHandles;
		}

		void free(TextureHandle _handle)
		{
			m_freeTextureHandle[m_numFreeTextureHandles] = _handle;
			++m_numFreeTextureHandles;
		}

		void free(FrameBufferHandle _handle)
		{
			m_freeFrameBufferHandle[m_numFreeFrameBufferHandles] = _handle;
			++m_numFreeFrameBufferHandles;
		}

		void free(UniformHandle _handle)
		{
			m_freeUniformHandle[m_numFreeUniformHandles] = _handle;
			++m_numFreeUniformHandles;
		}

		void resetFreeHandles()
		{
			m_numFreeIndexBufferHandles = 0;
			m_numFreeVertexDeclHandles = 0;
			m_numFreeVertexBufferHandles = 0;
			m_numFreeShaderHandles = 0;
			m_numFreeProgramHandles = 0;
			m_numFreeTextureHandles = 0;
			m_numFreeFrameBufferHandles = 0;
			m_numFreeUniformHandles = 0;
		}

		SortKey m_key;

		FrameBufferHandle m_fb[BGFX_CONFIG_MAX_VIEWS];
		Clear m_clear[BGFX_CONFIG_MAX_VIEWS];
		Rect m_rect[BGFX_CONFIG_MAX_VIEWS];
		Rect m_scissor[BGFX_CONFIG_MAX_VIEWS];
		Matrix4 m_view[BGFX_CONFIG_MAX_VIEWS];
		Matrix4 m_proj[BGFX_CONFIG_MAX_VIEWS];
		uint8_t m_other[BGFX_CONFIG_MAX_VIEWS];

		uint64_t m_sortKeys[BGFX_CONFIG_MAX_DRAW_CALLS];
		uint16_t m_sortValues[BGFX_CONFIG_MAX_DRAW_CALLS];
		RenderState m_renderState[BGFX_CONFIG_MAX_DRAW_CALLS];
		RenderState m_state;
		uint64_t m_flags;

		ConstantBuffer* m_constantBuffer;

		uint16_t m_num;
		uint16_t m_numRenderStates;
		uint16_t m_numDropped;

		MatrixCache m_matrixCache;
		RectCache m_rectCache;

		uint32_t m_iboffset;
		uint32_t m_vboffset;
		TransientIndexBuffer* m_transientIb;
		TransientVertexBuffer* m_transientVb;

		Resolution m_resolution;
		uint32_t m_debug;

		CommandBuffer m_cmdPre;
		CommandBuffer m_cmdPost;

		uint16_t m_numFreeIndexBufferHandles;
		uint16_t m_numFreeVertexDeclHandles;
		uint16_t m_numFreeVertexBufferHandles;
		uint16_t m_numFreeShaderHandles;
		uint16_t m_numFreeProgramHandles;
		uint16_t m_numFreeTextureHandles;
		uint16_t m_numFreeFrameBufferHandles;
		uint16_t m_numFreeUniformHandles;

		IndexBufferHandle m_freeIndexBufferHandle[BGFX_CONFIG_MAX_INDEX_BUFFERS];
		VertexDeclHandle m_freeVertexDeclHandle[BGFX_CONFIG_MAX_VERTEX_DECLS];
		VertexBufferHandle m_freeVertexBufferHandle[BGFX_CONFIG_MAX_VERTEX_BUFFERS];
		ShaderHandle m_freeShaderHandle[BGFX_CONFIG_MAX_SHADERS];
		ProgramHandle m_freeProgramHandle[BGFX_CONFIG_MAX_PROGRAMS];
		TextureHandle m_freeTextureHandle[BGFX_CONFIG_MAX_TEXTURES];
		FrameBufferHandle m_freeFrameBufferHandle[BGFX_CONFIG_MAX_FRAME_BUFFERS];
		UniformHandle m_freeUniformHandle[BGFX_CONFIG_MAX_UNIFORMS];
		TextVideoMem* m_textVideoMem;

		int64_t m_waitSubmit;
		int64_t m_waitRender;

		bool m_discard;
	};

	struct VertexDeclRef
	{
		VertexDeclRef()
		{
		}

		void init()
		{
			memset(m_vertexDeclRef, 0, sizeof(m_vertexDeclRef) );
			memset(m_vertexBufferRef, 0xff, sizeof(m_vertexBufferRef) );
		}

		template <uint16_t MaxHandlesT>
		void shutdown(bx::HandleAllocT<MaxHandlesT>& _handleAlloc)
		{
			for (VertexDeclMap::iterator it = m_vertexDeclMap.begin(), itEnd = m_vertexDeclMap.end(); it != itEnd; ++it)
			{
				_handleAlloc.free(it->second.idx);
			}

			m_vertexDeclMap.clear();
		}

		VertexDeclHandle find(uint32_t _hash)
		{
			VertexDeclMap::const_iterator it = m_vertexDeclMap.find(_hash);
			if (it != m_vertexDeclMap.end() )
			{
				return it->second;
			}

			VertexDeclHandle result = BGFX_INVALID_HANDLE;
			return result;
		}

		void add(VertexBufferHandle _handle, VertexDeclHandle _declHandle, uint32_t _hash)
		{
			m_vertexBufferRef[_handle.idx] = _declHandle;
			m_vertexDeclRef[_declHandle.idx]++;
			m_vertexDeclMap.insert(stl::make_pair(_hash, _declHandle) );
		}

		VertexDeclHandle release(VertexBufferHandle _handle)
		{
			VertexDeclHandle declHandle = m_vertexBufferRef[_handle.idx];
			if (isValid(declHandle) )
			{
				m_vertexDeclRef[declHandle.idx]--;

				if (0 != m_vertexDeclRef[declHandle.idx])
				{
					VertexDeclHandle invalid = BGFX_INVALID_HANDLE;
					return invalid;
				}
			}

			return declHandle;
		}

		typedef stl::unordered_map<uint32_t, VertexDeclHandle> VertexDeclMap;
		VertexDeclMap m_vertexDeclMap;
		uint16_t m_vertexDeclRef[BGFX_CONFIG_MAX_VERTEX_DECLS];
		VertexDeclHandle m_vertexBufferRef[BGFX_CONFIG_MAX_VERTEX_BUFFERS];
	};

	// First-fit non-local allocator.
	class NonLocalAllocator
	{
	public:
		static const uint64_t invalidBlock = UINT64_MAX;

		NonLocalAllocator()
		{
		}

		~NonLocalAllocator()
		{
		}

		void reset()
		{
			m_free.clear();
			m_used.clear();
		}

		void add(uint64_t _ptr, uint32_t _size)
		{
			m_free.push_back(Free(_ptr, _size) );
		}

		uint64_t alloc(uint32_t _size)
		{
			for (FreeList::iterator it = m_free.begin(), itEnd = m_free.end(); it != itEnd; ++it)
			{
				if (it->m_size >= _size)
				{
					uint64_t ptr = it->m_ptr;

					m_used.insert(stl::make_pair(ptr, _size) );

					if (it->m_size != _size)
					{
						it->m_size -= _size;
						it->m_ptr += _size;
					}
					else
					{
						m_free.erase(it);
					}

					return ptr;
				}
			}

			// there is no block large enough.
			return invalidBlock;
		}

		void free(uint64_t _block)
		{
			UsedList::iterator it = m_used.find(_block);
			if (it != m_used.end() )
			{
				m_free.push_front(Free(it->first, it->second) );
				m_used.erase(it);
			}
		}

		void compact()
		{
			m_free.sort();

			for (FreeList::iterator it = m_free.begin(), next = it, itEnd = m_free.end(); next != itEnd;)
			{
				if ( (it->m_ptr + it->m_size) == next->m_ptr)
				{
					it->m_size += next->m_size;
					next = m_free.erase(next);
				}
				else
				{
					it = next;
					++next;
				}
			}
		}

	private:
		struct Free
		{
			Free(uint64_t _ptr, uint32_t _size)
				: m_ptr(_ptr)
				, m_size(_size)
			{
			}

			bool operator<(const Free& rhs) const
			{
				return m_ptr < rhs.m_ptr;
			}

			uint64_t m_ptr;
			uint32_t m_size;
		};

		typedef std::list<Free> FreeList;
		FreeList m_free;

		typedef stl::unordered_map<uint64_t, uint32_t> UsedList;
		UsedList m_used;
	};

#if BGFX_CONFIG_DEBUG
#	define BGFX_API_FUNC(_api) BX_NO_INLINE _api
#else
#	define BGFX_API_FUNC(_api) _api
#endif // BGFX_CONFIG_DEBUG

	struct Context
	{
		Context()
			: m_render(&m_frame[0])
			, m_submit(&m_frame[1])
			, m_numFreeDynamicIndexBufferHandles(0)
			, m_numFreeDynamicVertexBufferHandles(0)
			, m_instBufferCount(0)
			, m_frames(0)
			, m_debug(BGFX_DEBUG_NONE)
			, m_rendererInitialized(false)
			, m_exit(false)
		{
		}

		~Context()
		{
		}

		static int32_t renderThread(void* _userData)
		{
			BX_TRACE("render thread start");
			Context* ctx = (Context*)_userData;
			while (!ctx->renderFrame() ) {};
			BX_TRACE("render thread exit");
			return EXIT_SUCCESS;
		}

		// game thread
		void init();
		void shutdown();

		CommandBuffer& getCommandBuffer(CommandBuffer::Enum _cmd)
		{
			CommandBuffer& cmdbuf = _cmd < CommandBuffer::End ? m_submit->m_cmdPre : m_submit->m_cmdPost;
			uint8_t cmd = (uint8_t)_cmd;
			cmdbuf.write(cmd);
			return cmdbuf;
		}

		BGFX_API_FUNC(void reset(uint32_t _width, uint32_t _height, uint32_t _flags) )
		{
			BX_WARN(0 != _width && 0 != _height, "Frame buffer resolution width or height cannot be 0 (width %d, height %d).", _width, _height);
			m_resolution.m_width = bx::uint32_max(1, _width);
			m_resolution.m_height = bx::uint32_max(1, _height);
			m_resolution.m_flags = _flags;

			memset(m_fb, 0xff, sizeof(m_fb) );
		}

		BGFX_API_FUNC(void setDebug(uint32_t _debug) )
		{
			m_debug = _debug;
		}

		BGFX_API_FUNC(void dbgTextClear(uint8_t _attr, bool _small) )
		{
			m_submit->m_textVideoMem->resize(_small, (uint16_t)m_resolution.m_width, (uint16_t)m_resolution.m_height);
			m_submit->m_textVideoMem->clear(_attr);
		}

		BGFX_API_FUNC(void dbgTextPrintfVargs(uint16_t _x, uint16_t _y, uint8_t _attr, const char* _format, va_list _argList) )
		{
			m_submit->m_textVideoMem->printfVargs(_x, _y, _attr, _format, _argList);
		}

		BGFX_API_FUNC(IndexBufferHandle createIndexBuffer(const Memory* _mem) )
		{
			IndexBufferHandle handle = { m_indexBufferHandle.alloc() };

			BX_WARN(isValid(handle), "Failed to allocate index buffer handle.");
			if (isValid(handle) )
			{
				CommandBuffer& cmdbuf = getCommandBuffer(CommandBuffer::CreateIndexBuffer);
				cmdbuf.write(handle);
				cmdbuf.write(_mem);
			}

			return handle;
		}

		BGFX_API_FUNC(void destroyIndexBuffer(IndexBufferHandle _handle) )
		{
			CommandBuffer& cmdbuf = getCommandBuffer(CommandBuffer::DestroyIndexBuffer);
			cmdbuf.write(_handle);
			m_submit->free(_handle);
		}

		VertexDeclHandle findVertexDecl(const VertexDecl& _decl)
		{
			VertexDeclHandle declHandle = m_declRef.find(_decl.m_hash);

			if (!isValid(declHandle) )
			{
				VertexDeclHandle temp = { m_vertexDeclHandle.alloc() };
				declHandle = temp;
				CommandBuffer& cmdbuf = getCommandBuffer(CommandBuffer::CreateVertexDecl);
				cmdbuf.write(declHandle);
				cmdbuf.write(_decl);
			}

			return declHandle;
		}

		BGFX_API_FUNC(VertexBufferHandle createVertexBuffer(const Memory* _mem, const VertexDecl& _decl) )
		{
			VertexBufferHandle handle = { m_vertexBufferHandle.alloc() };

			BX_WARN(isValid(handle), "Failed to allocate vertex buffer handle.");
			if (isValid(handle) )
			{
				VertexDeclHandle declHandle = findVertexDecl(_decl);
				m_declRef.add(handle, declHandle, _decl.m_hash);

				CommandBuffer& cmdbuf = getCommandBuffer(CommandBuffer::CreateVertexBuffer);
				cmdbuf.write(handle);
				cmdbuf.write(_mem);
				cmdbuf.write(declHandle);
			}

			return handle;
		}

		BGFX_API_FUNC(void destroyVertexBuffer(VertexBufferHandle _handle) )
		{
			CommandBuffer& cmdbuf = getCommandBuffer(CommandBuffer::DestroyVertexBuffer);
			cmdbuf.write(_handle);
			m_submit->free(_handle);
		}

		void destroyVertexBufferInternal(VertexBufferHandle _handle)
		{
			VertexDeclHandle declHandle = m_declRef.release(_handle);
			if (isValid(declHandle) )
			{
				CommandBuffer& cmdbuf = getCommandBuffer(CommandBuffer::DestroyVertexDecl);
				cmdbuf.write(declHandle);
			}

			m_vertexBufferHandle.free(_handle.idx);
		}

		BGFX_API_FUNC(DynamicIndexBufferHandle createDynamicIndexBuffer(uint32_t _num) )
		{
			DynamicIndexBufferHandle handle = BGFX_INVALID_HANDLE;
			uint32_t size = BX_ALIGN_16(_num*2);
			uint64_t ptr = m_dynamicIndexBufferAllocator.alloc(size);
			if (ptr == NonLocalAllocator::invalidBlock)
			{
				IndexBufferHandle indexBufferHandle = { m_indexBufferHandle.alloc() };
				BX_WARN(isValid(indexBufferHandle), "Failed to allocate index buffer handle.");
				if (!isValid(indexBufferHandle) )
				{
					return handle;
				}

				CommandBuffer& cmdbuf = getCommandBuffer(CommandBuffer::CreateDynamicIndexBuffer);
				cmdbuf.write(indexBufferHandle);
				cmdbuf.write(BGFX_CONFIG_DYNAMIC_INDEX_BUFFER_SIZE);

				m_dynamicIndexBufferAllocator.add(uint64_t(indexBufferHandle.idx)<<32, BGFX_CONFIG_DYNAMIC_INDEX_BUFFER_SIZE);
				ptr = m_dynamicIndexBufferAllocator.alloc(size);
			}

			handle.idx = m_dynamicIndexBufferHandle.alloc();
			BX_WARN(isValid(handle), "Failed to allocate dynamic index buffer handle.");
			if (!isValid(handle) )
			{
				return handle;
			}

			DynamicIndexBuffer& dib = m_dynamicIndexBuffers[handle.idx];
			dib.m_handle.idx = uint16_t(ptr>>32);
			dib.m_offset = uint32_t(ptr);
			dib.m_size = size;

			return handle;
		}

		BGFX_API_FUNC(DynamicIndexBufferHandle createDynamicIndexBuffer(const Memory* _mem) )
		{
			DynamicIndexBufferHandle handle = createDynamicIndexBuffer(_mem->size/2);
			if (isValid(handle) )
			{
				updateDynamicIndexBuffer(handle, _mem);
			}
			return handle;
		}

		BGFX_API_FUNC(void updateDynamicIndexBuffer(DynamicIndexBufferHandle _handle, const Memory* _mem) )
		{
			DynamicIndexBuffer& dib = m_dynamicIndexBuffers[_handle.idx];
			CommandBuffer& cmdbuf = getCommandBuffer(CommandBuffer::UpdateDynamicIndexBuffer);
			cmdbuf.write(dib.m_handle);
			cmdbuf.write(dib.m_offset);
			cmdbuf.write(dib.m_size);
			cmdbuf.write(_mem);
		}

		BGFX_API_FUNC(void destroyDynamicIndexBuffer(DynamicIndexBufferHandle _handle) )
		{
			m_freeDynamicIndexBufferHandle[m_numFreeDynamicIndexBufferHandles++] = _handle;
		}

		void destroyDynamicIndexBufferInternal(DynamicIndexBufferHandle _handle)
		{
			DynamicIndexBuffer& dib = m_dynamicIndexBuffers[_handle.idx];
			m_dynamicIndexBufferAllocator.free(uint64_t(dib.m_handle.idx)<<32 | dib.m_offset);
			m_dynamicIndexBufferHandle.free(_handle.idx);
		}

		BGFX_API_FUNC(DynamicVertexBufferHandle createDynamicVertexBuffer(uint16_t _num, const VertexDecl& _decl) )
		{
			DynamicVertexBufferHandle handle = BGFX_INVALID_HANDLE;
			uint32_t size = strideAlign16(_num*_decl.m_stride, _decl.m_stride);
			uint64_t ptr = m_dynamicVertexBufferAllocator.alloc(size);
			if (ptr == NonLocalAllocator::invalidBlock)
			{
				VertexBufferHandle vertexBufferHandle = { m_vertexBufferHandle.alloc() };

				BX_WARN(isValid(handle), "Failed to allocate dynamic vertex buffer handle.");
				if (!isValid(vertexBufferHandle) )
				{
					return handle;
				}

				CommandBuffer& cmdbuf = getCommandBuffer(CommandBuffer::CreateDynamicVertexBuffer);
				cmdbuf.write(vertexBufferHandle);
				cmdbuf.write(BGFX_CONFIG_DYNAMIC_VERTEX_BUFFER_SIZE);

				m_dynamicVertexBufferAllocator.add(uint64_t(vertexBufferHandle.idx)<<32, BGFX_CONFIG_DYNAMIC_VERTEX_BUFFER_SIZE);
				ptr = m_dynamicVertexBufferAllocator.alloc(size);
			}

			VertexDeclHandle declHandle = findVertexDecl(_decl);

			handle.idx = m_dynamicVertexBufferHandle.alloc();
			DynamicVertexBuffer& dvb = m_dynamicVertexBuffers[handle.idx];
			dvb.m_handle.idx = uint16_t(ptr>>32);
			dvb.m_offset = uint32_t(ptr);
			dvb.m_size = size;
			dvb.m_startVertex = dvb.m_offset/_decl.m_stride;
			dvb.m_numVertices = dvb.m_size/_decl.m_stride;
			dvb.m_decl = declHandle;
			m_declRef.add(dvb.m_handle, declHandle, _decl.m_hash);

			return handle;
		}

		BGFX_API_FUNC(DynamicVertexBufferHandle createDynamicVertexBuffer(const Memory* _mem, const VertexDecl& _decl) )
		{
			DynamicVertexBufferHandle handle = createDynamicVertexBuffer(_mem->size/_decl.m_stride, _decl);
			if (isValid(handle) )
			{
				updateDynamicVertexBuffer(handle, _mem);
			}
			return handle;
		}

		BGFX_API_FUNC(void updateDynamicVertexBuffer(DynamicVertexBufferHandle _handle, const Memory* _mem) )
		{
			DynamicVertexBuffer& dvb = m_dynamicVertexBuffers[_handle.idx];
			CommandBuffer& cmdbuf = getCommandBuffer(CommandBuffer::UpdateDynamicVertexBuffer);
			cmdbuf.write(dvb.m_handle);
			cmdbuf.write(dvb.m_offset);
			cmdbuf.write(dvb.m_size);
			cmdbuf.write(_mem);
		}

		BGFX_API_FUNC(void destroyDynamicVertexBuffer(DynamicVertexBufferHandle _handle) )
		{
			m_freeDynamicVertexBufferHandle[m_numFreeDynamicVertexBufferHandles++] = _handle;
		}

		void destroyDynamicVertexBufferInternal(DynamicVertexBufferHandle _handle)
		{
			DynamicVertexBuffer& dvb = m_dynamicVertexBuffers[_handle.idx];

			VertexDeclHandle declHandle = m_declRef.release(dvb.m_handle);
			if (invalidHandle != declHandle.idx)
			{
				CommandBuffer& cmdbuf = getCommandBuffer(CommandBuffer::DestroyVertexDecl);
				cmdbuf.write(declHandle);
			}

			m_dynamicVertexBufferAllocator.free(uint64_t(dvb.m_handle.idx)<<32 | dvb.m_offset);
			m_dynamicVertexBufferHandle.free(_handle.idx);
		}

		BGFX_API_FUNC(bool checkAvailTransientIndexBuffer(uint32_t _num) const)
		{
			return m_submit->checkAvailTransientIndexBuffer(_num);
		}

		BGFX_API_FUNC(bool checkAvailTransientVertexBuffer(uint32_t _num, uint16_t _stride) const)
		{
			return m_submit->checkAvailTransientVertexBuffer(_num, _stride);
		}

		TransientIndexBuffer* createTransientIndexBuffer(uint32_t _size)
		{
			TransientIndexBuffer* ib = NULL;

			IndexBufferHandle handle = { m_indexBufferHandle.alloc() };
			BX_WARN(isValid(handle), "Failed to allocate transient index buffer handle.");
			if (isValid(handle) )
			{
				CommandBuffer& cmdbuf = getCommandBuffer(CommandBuffer::CreateDynamicIndexBuffer);
				cmdbuf.write(handle);
				cmdbuf.write(_size);

				ib = (TransientIndexBuffer*)BX_ALLOC(g_allocator, sizeof(TransientIndexBuffer)+_size);
				ib->data = (uint8_t*)&ib[1];
				ib->size = _size;
				ib->handle = handle;
			}

			return ib;
		}

		void destroyTransientIndexBuffer(TransientIndexBuffer* _ib)
		{
			CommandBuffer& cmdbuf = getCommandBuffer(CommandBuffer::DestroyDynamicIndexBuffer);
			cmdbuf.write(_ib->handle);

			m_submit->free(_ib->handle);
			BX_FREE(g_allocator, const_cast<TransientIndexBuffer*>(_ib) );
		}

		BGFX_API_FUNC(void allocTransientIndexBuffer(TransientIndexBuffer* _tib, uint32_t _num) )
		{
			uint32_t offset = m_submit->allocTransientIndexBuffer(_num);

			TransientIndexBuffer& dib = *m_submit->m_transientIb;

			_tib->data = &dib.data[offset];
			_tib->size = _num * sizeof(uint16_t);
			_tib->handle = dib.handle;
			_tib->startIndex = offset/sizeof(uint16_t);
		}

		TransientVertexBuffer* createTransientVertexBuffer(uint32_t _size, const VertexDecl* _decl = NULL)
		{
			TransientVertexBuffer* vb = NULL;

			VertexBufferHandle handle = { m_vertexBufferHandle.alloc() };

			BX_WARN(isValid(handle), "Failed to allocate transient vertex buffer handle.");
			if (isValid(handle) )
			{
				uint16_t stride = 0;
				VertexDeclHandle declHandle = BGFX_INVALID_HANDLE;

				if (NULL != _decl)
				{
					declHandle = findVertexDecl(*_decl);
					m_declRef.add(handle, declHandle, _decl->m_hash);

					stride = _decl->m_stride;
				}

				CommandBuffer& cmdbuf = getCommandBuffer(CommandBuffer::CreateDynamicVertexBuffer);
				cmdbuf.write(handle);
				cmdbuf.write(_size);

				vb = (TransientVertexBuffer*)BX_ALLOC(g_allocator, sizeof(TransientVertexBuffer)+_size);
				vb->data = (uint8_t*)&vb[1];
				vb->size = _size;
				vb->startVertex = 0;
				vb->stride = stride;
				vb->handle = handle;
				vb->decl = declHandle;
			}

			return vb;
		}

		void destroyTransientVertexBuffer(TransientVertexBuffer* _vb)
		{
			CommandBuffer& cmdbuf = getCommandBuffer(CommandBuffer::DestroyDynamicVertexBuffer);
			cmdbuf.write(_vb->handle);

			m_submit->free(_vb->handle);
			BX_FREE(g_allocator, const_cast<TransientVertexBuffer*>(_vb) );
		}

		BGFX_API_FUNC(void allocTransientVertexBuffer(TransientVertexBuffer* _tvb, uint32_t _num, const VertexDecl& _decl) )
		{
			VertexDeclHandle declHandle = m_declRef.find(_decl.m_hash);

			TransientVertexBuffer& dvb = *m_submit->m_transientVb;

			if (!isValid(declHandle) )
			{
				VertexDeclHandle temp = { m_vertexDeclHandle.alloc() };
				declHandle = temp;
				CommandBuffer& cmdbuf = getCommandBuffer(CommandBuffer::CreateVertexDecl);
				cmdbuf.write(declHandle);
				cmdbuf.write(_decl);
				m_declRef.add(dvb.handle, declHandle, _decl.m_hash);
			}

			uint32_t offset = m_submit->allocTransientVertexBuffer(_num, _decl.m_stride);

			_tvb->data = &dvb.data[offset];
			_tvb->size = _num * _decl.m_stride;
			_tvb->startVertex = offset/_decl.m_stride;
			_tvb->stride = _decl.m_stride;
			_tvb->handle = dvb.handle;
			_tvb->decl = declHandle;
		}

		BGFX_API_FUNC(const InstanceDataBuffer* allocInstanceDataBuffer(uint32_t _num, uint16_t _stride) )
		{
			++m_instBufferCount;

			uint16_t stride = BX_ALIGN_16(_stride);
			uint32_t offset = m_submit->allocTransientVertexBuffer(_num, stride);

			TransientVertexBuffer& dvb = *m_submit->m_transientVb;
			InstanceDataBuffer* idb = (InstanceDataBuffer*)BX_ALLOC(g_allocator, sizeof(InstanceDataBuffer) );
			idb->data = &dvb.data[offset];
			idb->size = _num * stride;
			idb->offset = offset;
			idb->stride = stride;
			idb->num = _num;
			idb->handle = dvb.handle;

			return idb;
		}

		BGFX_API_FUNC(ShaderHandle createShader(const Memory* _mem) )
		{
			bx::MemoryReader reader(_mem->data, _mem->size);

			uint32_t magic;
			bx::read(&reader, magic);

			if (BGFX_CHUNK_MAGIC_VSH != magic
			&&  BGFX_CHUNK_MAGIC_FSH != magic)
			{
				BX_WARN(false, "Invalid shader signature! 0x%08x.", magic);
				ShaderHandle invalid = BGFX_INVALID_HANDLE;
				return invalid;
			}

			ShaderHandle handle = { m_shaderHandle.alloc() };

			BX_WARN(isValid(handle), "Failed to allocate shader handle.");
			if (isValid(handle) )
			{
				uint32_t iohash;
				bx::read(&reader, iohash);

				uint16_t count;
				bx::read(&reader, count);

				ShaderRef& sr = m_shaderRef[handle.idx];
				sr.m_refCount = 1;
				sr.m_hash     = iohash;
				sr.m_num      = 0;
				sr.m_uniforms = NULL;

				UniformHandle* uniforms = (UniformHandle*)alloca(count*sizeof(UniformHandle) );

				for (uint32_t ii = 0; ii < count; ++ii)
				{
					uint8_t nameSize;
					bx::read(&reader, nameSize);

					char name[256];
					bx::read(&reader, &name, nameSize);
					name[nameSize] = '\0';

					uint8_t type;
					bx::read(&reader, type);
					type &= ~BGFX_UNIFORM_FRAGMENTBIT;

					uint8_t num;
					bx::read(&reader, num);

					uint16_t regIndex;
					bx::read(&reader, regIndex);

					uint16_t regCount;
					bx::read(&reader, regCount);

					PredefinedUniform::Enum predefined = nameToPredefinedUniformEnum(name);
					if (PredefinedUniform::Count == predefined)
					{
						uniforms[sr.m_num] = createUniform(name, UniformType::Enum(type), regCount);
						sr.m_num++;
					}
				}

				if (0 != sr.m_num)
				{
					uint32_t size = sr.m_num*sizeof(UniformHandle);
					sr.m_uniforms = (UniformHandle*)BX_ALLOC(g_allocator, size);
					memcpy(sr.m_uniforms, uniforms, size);
				}

				CommandBuffer& cmdbuf = getCommandBuffer(CommandBuffer::CreateShader);
				cmdbuf.write(handle);
				cmdbuf.write(_mem);
			}

			return handle;
		}

		BGFX_API_FUNC(uint16_t getShaderUniforms(ShaderHandle _handle, UniformHandle* _uniforms, uint16_t _max) )
		{
			if (!isValid(_handle) )
			{
				BX_WARN(false, "Passing invalid shader handle to bgfx::getShaderUniforms.");
				return 0;
			}

			ShaderRef& sr = m_shaderRef[_handle.idx];
			if (NULL != _uniforms)
			{
				memcpy(_uniforms, sr.m_uniforms, bx::uint16_min(_max, sr.m_num)*sizeof(UniformHandle) );
			}

			return sr.m_num;
		}

		BGFX_API_FUNC(void destroyShader(ShaderHandle _handle) )
		{
			if (!isValid(_handle) )
			{
				BX_WARN(false, "Passing invalid shader handle to bgfx::destroyShader.");
				return;
			}

			shaderDecRef(_handle);
		}

		void shaderIncRef(ShaderHandle _handle)
		{
			ShaderRef& sr = m_shaderRef[_handle.idx];
			++sr.m_refCount;
		}

		void shaderDecRef(ShaderHandle _handle)
		{
			ShaderRef& sr = m_shaderRef[_handle.idx];
			int32_t refs = --sr.m_refCount;
			if (0 == refs)
			{
				CommandBuffer& cmdbuf = getCommandBuffer(CommandBuffer::DestroyShader);
				cmdbuf.write(_handle);
				m_submit->free(_handle);

				if (0 != sr.m_num)
				{
					for (uint32_t ii = 0, num = sr.m_num; ii < num; ++ii)
					{
						destroyUniform(sr.m_uniforms[ii]);
					}

					BX_FREE(g_allocator, sr.m_uniforms);
					sr.m_uniforms = NULL;
					sr.m_num = 0;
				}
			}
		}

		BGFX_API_FUNC(ProgramHandle createProgram(ShaderHandle _vsh, ShaderHandle _fsh) )
		{
			if (!isValid(_vsh)
			||  !isValid(_fsh) )
			{
				BX_WARN(false, "Vertex/fragment shader is invalid (vsh %d, fsh %d).", _vsh.idx, _fsh.idx);
				ProgramHandle invalid = BGFX_INVALID_HANDLE;
				return invalid;
			}

			const ShaderRef& vsr = m_shaderRef[_vsh.idx];
			const ShaderRef& fsr = m_shaderRef[_fsh.idx];
			if (vsr.m_hash != fsr.m_hash)
			{
				BX_WARN(vsr.m_hash == fsr.m_hash, "Vertex shader output doesn't match fragment shader input.");
				ProgramHandle invalid = BGFX_INVALID_HANDLE;
				return invalid;
			}

			ProgramHandle handle;
 			handle.idx = m_programHandle.alloc();

			BX_WARN(isValid(handle), "Failed to allocate program handle.");
			if (isValid(handle) )
			{
				shaderIncRef(_vsh);
				shaderIncRef(_fsh);
				m_programRef[handle.idx].m_vsh = _vsh;
				m_programRef[handle.idx].m_fsh = _fsh;

				CommandBuffer& cmdbuf = getCommandBuffer(CommandBuffer::CreateProgram);
				cmdbuf.write(handle);
				cmdbuf.write(_vsh);
				cmdbuf.write(_fsh);
			}

			return handle;
		}

		BGFX_API_FUNC(void destroyProgram(ProgramHandle _handle) )
		{
			CommandBuffer& cmdbuf = getCommandBuffer(CommandBuffer::DestroyProgram);
			cmdbuf.write(_handle);
			m_submit->free(_handle);

			shaderDecRef(m_programRef[_handle.idx].m_vsh);
			shaderDecRef(m_programRef[_handle.idx].m_fsh);
		}

		BGFX_API_FUNC(TextureHandle createTexture(const Memory* _mem, uint32_t _flags, uint8_t _skip, TextureInfo* _info) )
		{
			if (NULL != _info)
			{
				ImageContainer imageContainer;
				if (imageParse(imageContainer, _mem->data, _mem->size) )
				{
					calcTextureSize(*_info
						, (uint16_t)imageContainer.m_width
						, (uint16_t)imageContainer.m_height
						, (uint16_t)imageContainer.m_depth
						, imageContainer.m_numMips
						, TextureFormat::Enum(imageContainer.m_format)
						);
				}
				else
				{
					_info->format = TextureFormat::Unknown;
					_info->storageSize = 0;
					_info->width = 0;
					_info->height = 0;
					_info->depth = 0;
					_info->numMips = 0;
					_info->bitsPerPixel = 0;
				}
			}

			TextureHandle handle = { m_textureHandle.alloc() };
			BX_WARN(isValid(handle), "Failed to allocate texture handle.");
			if (isValid(handle) )
			{
				TextureRef& ref = m_textureRef[handle.idx];
				ref.m_refCount = 1;

				CommandBuffer& cmdbuf = getCommandBuffer(CommandBuffer::CreateTexture);
				cmdbuf.write(handle);
				cmdbuf.write(_mem);
				cmdbuf.write(_flags);
				cmdbuf.write(_skip);
			}

			return handle;
		}

		BGFX_API_FUNC(void destroyTexture(TextureHandle _handle) )
		{
			if (!isValid(_handle) )
			{
				BX_WARN(false, "Passing invalid texture handle to bgfx::destroyTexture");
				return;
			}

			textureDecRef(_handle);
		}

		void textureIncRef(TextureHandle _handle)
		{
			TextureRef& ref = m_textureRef[_handle.idx];
			++ref.m_refCount;
		}

		void textureDecRef(TextureHandle _handle)
		{
			TextureRef& ref = m_textureRef[_handle.idx];
			int32_t refs = --ref.m_refCount;
			if (0 == refs)
			{
				CommandBuffer& cmdbuf = getCommandBuffer(CommandBuffer::DestroyTexture);
				cmdbuf.write(_handle);
				m_submit->free(_handle);
			}
		}

		BGFX_API_FUNC(void updateTexture(TextureHandle _handle, uint8_t _side, uint8_t _mip, uint16_t _x, uint16_t _y, uint16_t _z, uint16_t _width, uint16_t _height, uint16_t _depth, uint16_t _pitch, const Memory* _mem) )
		{
			CommandBuffer& cmdbuf = getCommandBuffer(CommandBuffer::UpdateTexture);
			cmdbuf.write(_handle);
			cmdbuf.write(_side);
			cmdbuf.write(_mip);
			Rect rect;
			rect.m_x = _x;
			rect.m_y = _y;
			rect.m_width = _width;
			rect.m_height = _height;
			cmdbuf.write(rect);
			cmdbuf.write(_z);
			cmdbuf.write(_depth);
			cmdbuf.write(_pitch);
			cmdbuf.write(_mem);
		}

		BGFX_API_FUNC(FrameBufferHandle createFrameBuffer(uint8_t _num, TextureHandle* _handles) )
		{
			FrameBufferHandle handle = { m_frameBufferHandle.alloc() };
			BX_WARN(isValid(handle), "Failed to allocate render target handle.");

			if (isValid(handle) )
			{
				CommandBuffer& cmdbuf = getCommandBuffer(CommandBuffer::CreateFrameBuffer);
				cmdbuf.write(handle);
				cmdbuf.write(_num);

				FrameBufferRef& ref = m_frameBufferRef[handle.idx];
				memset(ref.m_th, 0xff, sizeof(ref.m_th) );
				for (uint32_t ii = 0; ii < _num; ++ii)
				{
					TextureHandle handle = _handles[ii];

					cmdbuf.write(handle);

					ref.m_th[ii] = handle;
					textureIncRef(handle);
				}
			}

			return handle;
		}

		BGFX_API_FUNC(void destroyFrameBuffer(FrameBufferHandle _handle) )
		{
			CommandBuffer& cmdbuf = getCommandBuffer(CommandBuffer::DestroyFrameBuffer);
			cmdbuf.write(_handle);
			m_submit->free(_handle);

			FrameBufferRef& ref = m_frameBufferRef[_handle.idx];
			for (uint32_t ii = 0; ii < BX_COUNTOF(ref.m_th); ++ii)
			{
				TextureHandle th = ref.m_th[ii];
				if (isValid(th) )
				{
					textureDecRef(th);
				}
			}
		}

		BGFX_API_FUNC(UniformHandle createUniform(const char* _name, UniformType::Enum _type, uint16_t _num) )
		{
			BX_WARN(PredefinedUniform::Count == nameToPredefinedUniformEnum(_name), "%s is predefined uniform name.", _name);
			if (PredefinedUniform::Count != nameToPredefinedUniformEnum(_name) )
			{
				UniformHandle handle = BGFX_INVALID_HANDLE;
				return handle;
			}

			UniformHashMap::iterator it = m_uniformHashMap.find(_name);
			if (it != m_uniformHashMap.end() )
			{
				UniformHandle handle = it->second;
				UniformRef& uniform = m_uniformRef[handle.idx];

				uint32_t oldsize = g_uniformTypeSize[uniform.m_type];
				uint32_t newsize = g_uniformTypeSize[_type];

				if (oldsize < newsize
				||  uniform.m_num < _num)
				{
					uniform.m_type = oldsize < newsize ? _type : uniform.m_type;
					uniform.m_num  = bx::uint16_max(uniform.m_num, _num);

					CommandBuffer& cmdbuf = getCommandBuffer(CommandBuffer::CreateUniform);
					cmdbuf.write(handle);
					cmdbuf.write(uniform.m_type);
					cmdbuf.write(uniform.m_num);
					uint8_t len = (uint8_t)strlen(_name)+1;
					cmdbuf.write(len);
					cmdbuf.write(_name, len);
				}

				++uniform.m_refCount;
				return handle;
			}

			UniformHandle handle = { m_uniformHandle.alloc() };

			BX_WARN(isValid(handle), "Failed to allocate uniform handle.");
			if (isValid(handle) )
			{
				UniformRef& uniform = m_uniformRef[handle.idx];
				uniform.m_refCount = 1;
				uniform.m_type = _type;
				uniform.m_num  = _num;

				m_uniformHashMap.insert(stl::make_pair(stl::string(_name), handle) );

				CommandBuffer& cmdbuf = getCommandBuffer(CommandBuffer::CreateUniform);
				cmdbuf.write(handle);
				cmdbuf.write(_type);
				cmdbuf.write(_num);
				uint8_t len = (uint8_t)strlen(_name)+1;
				cmdbuf.write(len);
				cmdbuf.write(_name, len);
			}

			return handle;
		}

		BGFX_API_FUNC(void destroyUniform(UniformHandle _handle) )
		{
			UniformRef& uniform = m_uniformRef[_handle.idx];
			BX_CHECK(uniform.m_refCount > 0, "Destroying already destroyed uniform %d.", _handle.idx);
			int32_t refs = --uniform.m_refCount;

			if (0 == refs)
			{
				CommandBuffer& cmdbuf = getCommandBuffer(CommandBuffer::DestroyUniform);
				cmdbuf.write(_handle);
				m_submit->free(_handle);
			}
		}

		BGFX_API_FUNC(void saveScreenShot(const char* _filePath) )
		{
			CommandBuffer& cmdbuf = getCommandBuffer(CommandBuffer::SaveScreenShot);
			uint16_t len = (uint16_t)strlen(_filePath)+1;
			cmdbuf.write(len);
			cmdbuf.write(_filePath, len);
		}

		BGFX_API_FUNC(void setViewName(uint8_t _id, const char* _name) )
		{
			CommandBuffer& cmdbuf = getCommandBuffer(CommandBuffer::UpdateViewName);
			cmdbuf.write(_id);
			uint16_t len = (uint16_t)strlen(_name)+1;
			cmdbuf.write(len);
			cmdbuf.write(_name, len);
		}

		BGFX_API_FUNC(void setViewRect(uint8_t _id, uint16_t _x, uint16_t _y, uint16_t _width, uint16_t _height) )
		{
			Rect& rect = m_rect[_id];
			rect.m_x = _x;
			rect.m_y = _y;
			rect.m_width = bx::uint16_max(_width, 1);
			rect.m_height = bx::uint16_max(_height, 1);
		}

		BGFX_API_FUNC(void setViewRectMask(uint32_t _viewMask, uint16_t _x, uint16_t _y, uint16_t _width, uint16_t _height) )
		{
			for (uint32_t view = 0, viewMask = _viewMask, ntz = bx::uint32_cnttz(_viewMask); 0 != viewMask; viewMask >>= 1, view += 1, ntz = bx::uint32_cnttz(viewMask) )
			{
				viewMask >>= ntz;
				view += ntz;

				setViewRect( (uint8_t)view, _x, _y, _width, _height);
			}
		}

		BGFX_API_FUNC(void setViewScissor(uint8_t _id, uint16_t _x, uint16_t _y, uint16_t _width, uint16_t _height) )
		{
			Rect& scissor = m_scissor[_id];
			scissor.m_x = _x;
			scissor.m_y = _y;
			scissor.m_width = _width;
			scissor.m_height = _height;
		}

		BGFX_API_FUNC(void setViewScissorMask(uint32_t _viewMask, uint16_t _x, uint16_t _y, uint16_t _width, uint16_t _height) )
		{
			for (uint32_t view = 0, viewMask = _viewMask, ntz = bx::uint32_cnttz(_viewMask); 0 != viewMask; viewMask >>= 1, view += 1, ntz = bx::uint32_cnttz(viewMask) )
			{
				viewMask >>= ntz;
				view += ntz;

				setViewScissor( (uint8_t)view, _x, _y, _width, _height);
			}
		}

		BGFX_API_FUNC(void setViewClear(uint8_t _id, uint8_t _flags, uint32_t _rgba, float _depth, uint8_t _stencil) )
		{
			Clear& clear = m_clear[_id];
			clear.m_flags = _flags;
			clear.m_rgba = _rgba;
			clear.m_depth = _depth;
			clear.m_stencil = _stencil;
		}

		BGFX_API_FUNC(void setViewClearMask(uint32_t _viewMask, uint8_t _flags, uint32_t _rgba, float _depth, uint8_t _stencil) )
		{
			for (uint32_t view = 0, viewMask = _viewMask, ntz = bx::uint32_cnttz(_viewMask); 0 != viewMask; viewMask >>= 1, view += 1, ntz = bx::uint32_cnttz(viewMask) )
			{
				viewMask >>= ntz;
				view += ntz;

				setViewClear( (uint8_t)view, _flags, _rgba, _depth, _stencil);
			}
		}

		BGFX_API_FUNC(void setViewSeq(uint8_t _id, bool _enabled) )
		{
			m_seqMask[_id] = _enabled ? 0xffff : 0x0;
		}

		BGFX_API_FUNC(void setViewSeqMask(uint32_t _viewMask, bool _enabled) )
		{
			uint16_t mask = _enabled ? 0xffff : 0x0;
			for (uint32_t view = 0, viewMask = _viewMask, ntz = bx::uint32_cnttz(_viewMask); 0 != viewMask; viewMask >>= 1, view += 1, ntz = bx::uint32_cnttz(viewMask) )
			{
				viewMask >>= ntz;
				view += ntz;

				m_seqMask[view] = mask;
			}
		}

		BGFX_API_FUNC(void setViewFrameBuffer(uint8_t _id, FrameBufferHandle _handle) )
		{
			m_fb[_id] = _handle;
		}

		BGFX_API_FUNC(void setViewFrameBufferMask(uint32_t _viewMask, FrameBufferHandle _handle) )
		{
			for (uint32_t view = 0, viewMask = _viewMask, ntz = bx::uint32_cnttz(_viewMask); 0 != viewMask; viewMask >>= 1, view += 1, ntz = bx::uint32_cnttz(viewMask) )
			{
				viewMask >>= ntz;
				view += ntz;

				m_fb[view] = _handle;
			}
		}

		BGFX_API_FUNC(void setViewTransform(uint8_t _id, const void* _view, const void* _proj, uint8_t _other) )
		{
			if (BGFX_CONFIG_MAX_VIEWS > _other)
			{
				m_other[_id] = _other;
			}
			else
			{
				m_other[_id] = _id;
			}

			if (NULL != _view)
			{
				memcpy(m_view[_id].un.val, _view, sizeof(Matrix4) );
			}
			else
			{
				m_view[_id].setIdentity();
			}

			if (NULL != _proj)
			{
				memcpy(m_proj[_id].un.val, _proj, sizeof(Matrix4) );
			}
			else
			{
				m_proj[_id].setIdentity();
			}
		}

		BGFX_API_FUNC(void setViewTransformMask(uint32_t _viewMask, const void* _view, const void* _proj, uint8_t _other) )
		{
			for (uint32_t view = 0, viewMask = _viewMask, ntz = bx::uint32_cnttz(_viewMask); 0 != viewMask; viewMask >>= 1, view += 1, ntz = bx::uint32_cnttz(viewMask) )
			{
				viewMask >>= ntz;
				view += ntz;

				setViewTransform( (uint8_t)view, _view, _proj, _other);
			}
		}

		BGFX_API_FUNC(void setMarker(const char* _marker) )
		{
			m_submit->setMarker(_marker);
		}

		BGFX_API_FUNC(void setState(uint64_t _state, uint32_t _rgba) )
		{
			m_submit->setState(_state, _rgba);
		}

		BGFX_API_FUNC(void setStencil(uint32_t _fstencil, uint32_t _bstencil) )
		{
			m_submit->setStencil(_fstencil, _bstencil);
		}

		BGFX_API_FUNC(uint16_t setScissor(uint16_t _x, uint16_t _y, uint16_t _width, uint16_t _height) )
		{
			return m_submit->setScissor(_x, _y, _width, _height);
		}

		BGFX_API_FUNC(void setScissor(uint16_t _cache) )
		{
			m_submit->setScissor(_cache);
		}

		BGFX_API_FUNC(uint32_t setTransform(const void* _mtx, uint16_t _num) )
		{
			return m_submit->setTransform(_mtx, _num);
		}

		BGFX_API_FUNC(void setTransform(uint32_t _cache, uint16_t _num) )
		{
			m_submit->setTransform(_cache, _num);
		}

		BGFX_API_FUNC(void setUniform(UniformHandle _handle, const void* _value, uint16_t _num) )
		{
			UniformRef& uniform = m_uniformRef[_handle.idx];
			BX_CHECK(uniform.m_num >= _num, "Truncated uniform update. %d (max: %d)", _num, uniform.m_num);
			m_submit->writeUniform(uniform.m_type, _handle, _value, bx::uint16_min(uniform.m_num, _num) );
		}

		BGFX_API_FUNC(void setIndexBuffer(IndexBufferHandle _handle, uint32_t _firstIndex, uint32_t _numIndices) )
		{
			m_submit->setIndexBuffer(_handle, _firstIndex, _numIndices);
		}

		BGFX_API_FUNC(void setIndexBuffer(DynamicIndexBufferHandle _handle, uint32_t _firstIndex, uint32_t _numIndices) )
		{
			m_submit->setIndexBuffer(m_dynamicIndexBuffers[_handle.idx].m_handle, _firstIndex, _numIndices);
		}

		BGFX_API_FUNC(void setIndexBuffer(const TransientIndexBuffer* _tib, uint32_t _numIndices) )
		{
			m_submit->setIndexBuffer(_tib, _numIndices);
		}

		BGFX_API_FUNC(void setVertexBuffer(VertexBufferHandle _handle, uint32_t _numVertices, uint32_t _startVertex) )
		{
			m_submit->setVertexBuffer(_handle, _numVertices, _startVertex);
		}

		BGFX_API_FUNC(void setVertexBuffer(DynamicVertexBufferHandle _handle, uint32_t _numVertices) )
		{
			m_submit->setVertexBuffer(m_dynamicVertexBuffers[_handle.idx], _numVertices);
		}

		BGFX_API_FUNC(void setVertexBuffer(const TransientVertexBuffer* _tvb, uint32_t _numVertices) )
		{
			m_submit->setVertexBuffer(_tvb, _numVertices);
		}

		BGFX_API_FUNC(void setInstanceDataBuffer(const InstanceDataBuffer* _idb, uint16_t _num) )
		{
			--m_instBufferCount;

			m_submit->setInstanceDataBuffer(_idb, _num);
		}

		BGFX_API_FUNC(void setProgram(ProgramHandle _handle) )
		{
			m_submit->setProgram(_handle);
		}

		BGFX_API_FUNC(void setTexture(uint8_t _stage, UniformHandle _sampler, TextureHandle _handle, uint32_t _flags) )
		{
			m_submit->setTexture(_stage, _sampler, _handle, _flags);
		}

		BGFX_API_FUNC(void setTexture(uint8_t _stage, UniformHandle _sampler, FrameBufferHandle _handle, uint8_t _attachment, uint32_t _flags) )
		{
			BX_CHECK(_attachment < g_caps.maxFBAttachments, "Frame buffer attachment index %d is invalid.", _attachment);
			TextureHandle textureHandle = BGFX_INVALID_HANDLE;
			if (isValid(_handle) )
			{
				textureHandle = m_frameBufferRef[_handle.idx].m_th[_attachment];
				BX_CHECK(isValid(textureHandle), "Frame buffer texture %d is invalid.", _attachment);
			}

			m_submit->setTexture(_stage, _sampler, textureHandle, _flags);
		}

		BGFX_API_FUNC(uint32_t submit(uint8_t _id, int32_t _depth) )
		{
			return m_submit->submit(_id, _depth);
		}

		BGFX_API_FUNC(uint32_t submitMask(uint32_t _viewMask, int32_t _depth) )
		{
			return m_submit->submitMask(_viewMask, _depth);
		}

		BGFX_API_FUNC(void discard() )
		{
			m_submit->discard();
		}

		BGFX_API_FUNC(uint32_t frame() );

		void dumpViewStats();
		void freeDynamicBuffers();
		void freeAllHandles(Frame* _frame);
		void frameNoRenderWait();
		void swap();

		// render thread
		bool renderFrame();
		void rendererFlip();
		void rendererInit();
		void rendererShutdown();
		void rendererCreateIndexBuffer(IndexBufferHandle _handle, Memory* _mem);
		void rendererDestroyIndexBuffer(IndexBufferHandle _handle);
		void rendererCreateVertexBuffer(VertexBufferHandle _handle, Memory* _mem, VertexDeclHandle _declHandle);
		void rendererDestroyVertexBuffer(VertexBufferHandle _handle);
		void rendererCreateDynamicIndexBuffer(IndexBufferHandle _handle, uint32_t _size);
		void rendererUpdateDynamicIndexBuffer(IndexBufferHandle _handle, uint32_t _offset, uint32_t _size, Memory* _mem);
		void rendererDestroyDynamicIndexBuffer(IndexBufferHandle _handle);
		void rendererCreateDynamicVertexBuffer(VertexBufferHandle _handle, uint32_t _size);
		void rendererUpdateDynamicVertexBuffer(VertexBufferHandle _handle, uint32_t _offset, uint32_t _size, Memory* _mem);
		void rendererDestroyDynamicVertexBuffer(VertexBufferHandle _handle);
		void rendererCreateVertexDecl(VertexDeclHandle _handle, const VertexDecl& _decl);
		void rendererDestroyVertexDecl(VertexDeclHandle _handle);
		void rendererCreateShader(ShaderHandle _handle, Memory* _mem);
		void rendererDestroyShader(ShaderHandle _handle);
		void rendererCreateProgram(ProgramHandle _handle, ShaderHandle _vsh, ShaderHandle _fsh);
		void rendererDestroyProgram(ProgramHandle _handle);
		void rendererCreateTexture(TextureHandle _handle, Memory* _mem, uint32_t _flags, uint8_t _skip);
		void rendererUpdateTextureBegin(TextureHandle _handle, uint8_t _side, uint8_t _mip);
		void rendererUpdateTexture(TextureHandle _handle, uint8_t _side, uint8_t _mip, const Rect& _rect, uint16_t _z, uint16_t _depth, uint16_t _pitch, const Memory* _mem);
		void rendererUpdateTextureEnd();
		void rendererDestroyTexture(TextureHandle _handle);
		void rendererCreateFrameBuffer(FrameBufferHandle _handle, uint8_t _num, const TextureHandle* _textureHandles);
		void rendererDestroyFrameBuffer(FrameBufferHandle _handle);
		void rendererCreateUniform(UniformHandle _handle, UniformType::Enum _type, uint16_t _num, const char* _name);
		void rendererDestroyUniform(UniformHandle _handle);
		void rendererSaveScreenShot(const char* _filePath);
		void rendererUpdateViewName(uint8_t _id, const char* _name);
		void rendererUpdateUniform(uint16_t _loc, const void* _data, uint32_t _size);
		void rendererSetMarker(const char* _marker, uint32_t _size);
		void rendererUpdateUniforms(ConstantBuffer* _constantBuffer, uint32_t _begin, uint32_t _end);
		void flushTextureUpdateBatch(CommandBuffer& _cmdbuf);
		void rendererExecCommands(CommandBuffer& _cmdbuf);
		void rendererSubmit();

#if BGFX_CONFIG_MULTITHREADED
		void gameSemPost()
		{
			m_gameSem.post();
		}

		void gameSemWait()
		{
			int64_t start = bx::getHPCounter();
			bool ok = m_gameSem.wait();
			BX_CHECK(ok, "Semaphore wait failed."); BX_UNUSED(ok);
			m_render->m_waitSubmit = bx::getHPCounter()-start;
		}

		void renderSemPost()
		{
			m_renderSem.post();
		}

		void renderSemWait()
		{
			int64_t start = bx::getHPCounter();
			bool ok = m_renderSem.wait();
			BX_CHECK(ok, "Semaphore wait failed."); BX_UNUSED(ok);
			m_submit->m_waitRender = bx::getHPCounter() - start;
		}

		bx::Semaphore m_renderSem;
		bx::Semaphore m_gameSem;
		bx::Thread m_thread;
#else
		void gameSemPost()
		{
		}

		void gameSemWait()
		{
		}

		void renderSemPost()
		{
		}

		void renderSemWait()
		{
		}
#endif // BGFX_CONFIG_MULTITHREADED

		Frame m_frame[2];
		Frame* m_render;
		Frame* m_submit;

		uint64_t m_tempKeys[BGFX_CONFIG_MAX_DRAW_CALLS];
		uint16_t m_tempValues[BGFX_CONFIG_MAX_DRAW_CALLS];

		DynamicIndexBuffer m_dynamicIndexBuffers[BGFX_CONFIG_MAX_DYNAMIC_INDEX_BUFFERS];
		DynamicVertexBuffer m_dynamicVertexBuffers[BGFX_CONFIG_MAX_DYNAMIC_VERTEX_BUFFERS];

		uint16_t m_numFreeDynamicIndexBufferHandles;
		uint16_t m_numFreeDynamicVertexBufferHandles;
		DynamicIndexBufferHandle m_freeDynamicIndexBufferHandle[BGFX_CONFIG_MAX_DYNAMIC_INDEX_BUFFERS];
		DynamicVertexBufferHandle m_freeDynamicVertexBufferHandle[BGFX_CONFIG_MAX_DYNAMIC_VERTEX_BUFFERS];

		NonLocalAllocator m_dynamicIndexBufferAllocator;
		bx::HandleAllocT<BGFX_CONFIG_MAX_DYNAMIC_INDEX_BUFFERS> m_dynamicIndexBufferHandle;
		NonLocalAllocator m_dynamicVertexBufferAllocator;
		bx::HandleAllocT<BGFX_CONFIG_MAX_DYNAMIC_VERTEX_BUFFERS> m_dynamicVertexBufferHandle;

		bx::HandleAllocT<BGFX_CONFIG_MAX_INDEX_BUFFERS> m_indexBufferHandle;
		bx::HandleAllocT<BGFX_CONFIG_MAX_VERTEX_DECLS > m_vertexDeclHandle;

		bx::HandleAllocT<BGFX_CONFIG_MAX_VERTEX_BUFFERS> m_vertexBufferHandle;
		bx::HandleAllocT<BGFX_CONFIG_MAX_SHADERS> m_shaderHandle;
		bx::HandleAllocT<BGFX_CONFIG_MAX_PROGRAMS> m_programHandle;
		bx::HandleAllocT<BGFX_CONFIG_MAX_TEXTURES> m_textureHandle;
		bx::HandleAllocT<BGFX_CONFIG_MAX_FRAME_BUFFERS> m_frameBufferHandle;
		bx::HandleAllocT<BGFX_CONFIG_MAX_UNIFORMS> m_uniformHandle;

		struct ShaderRef
		{
			UniformHandle* m_uniforms;
			uint32_t m_hash;
			int16_t  m_refCount;
			uint16_t m_num;
		};
		
		struct ProgramRef
		{
			ShaderHandle m_vsh;
			ShaderHandle m_fsh;
		};

		struct UniformRef
		{
			UniformType::Enum m_type;
			uint16_t m_num;
			int16_t m_refCount;
		};

		struct TextureRef
		{
			int16_t m_refCount;
		};

		struct FrameBufferRef
		{
			TextureHandle m_th[BGFX_CONFIG_MAX_FRAME_BUFFER_ATTACHMENTS];
		};

		typedef stl::unordered_map<stl::string, UniformHandle> UniformHashMap;
		UniformHashMap m_uniformHashMap;
		UniformRef m_uniformRef[BGFX_CONFIG_MAX_UNIFORMS];
		ShaderRef m_shaderRef[BGFX_CONFIG_MAX_SHADERS];
		ProgramRef m_programRef[BGFX_CONFIG_MAX_PROGRAMS];
		TextureRef m_textureRef[BGFX_CONFIG_MAX_TEXTURES];
		FrameBufferRef m_frameBufferRef[BGFX_CONFIG_MAX_FRAME_BUFFERS];
		VertexDeclRef m_declRef;

		FrameBufferHandle m_fb[BGFX_CONFIG_MAX_VIEWS];
		Clear m_clear[BGFX_CONFIG_MAX_VIEWS];
		Rect m_rect[BGFX_CONFIG_MAX_VIEWS];
		Rect m_scissor[BGFX_CONFIG_MAX_VIEWS];
		Matrix4 m_view[BGFX_CONFIG_MAX_VIEWS];
		Matrix4 m_proj[BGFX_CONFIG_MAX_VIEWS];
		uint8_t m_other[BGFX_CONFIG_MAX_VIEWS];
		uint16_t m_seq[BGFX_CONFIG_MAX_VIEWS];
		uint16_t m_seqMask[BGFX_CONFIG_MAX_VIEWS];

		Resolution m_resolution;
		int32_t  m_instBufferCount;
		uint32_t m_frames;
		uint32_t m_debug;

		TextVideoMemBlitter m_textVideoMemBlitter;
		ClearQuad m_clearQuad;

		bool m_rendererInitialized;
		bool m_exit;

		BX_CACHE_LINE_ALIGN_MARKER();
		typedef UpdateBatchT<256> TextureUpdateBatch;
		TextureUpdateBatch m_textureUpdateBatch;
	};

#undef BGFX_API_FUNC

} // namespace bgfx

#endif // BGFX_P_H_HEADER_GUARD
