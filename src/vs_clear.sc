$input a_position, a_color0
$output v_color0

/*
 * Copyright 2011-2014 Branimir Karadzic. All rights reserved.
 * License: http://www.opensource.org/licenses/BSD-2-Clause
 */

#include "bgfx_shader.sh"

void main()
{
	gl_Position = vec4(a_position, 1.0);
	v_color0 = a_color0;
}
