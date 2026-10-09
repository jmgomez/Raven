#version 450

/*
 * Raven - reference vertex shader.
 *
 * Every primitive is one instance (RDrawInstance, 80 bytes). No vertex
 * buffer is bound per vertex: the four corners of a quad are generated from
 * gl_VertexIndex (triangle strip) and the quad is sized to cover the
 * primitive. The fragment shader computes the exact shape analytically.
 */

const uint PRIM_RECT     = 0u;
const uint PRIM_GLYPH    = 1u;
const uint PRIM_SHADOW   = 2u;
const uint PRIM_LINE     = 3u;
const uint PRIM_TRIANGLE = 4u;
const uint PRIM_GLASS    = 5u;
const uint PRIM_MASK     = 15u;

layout(push_constant) uniform PushConstants {
	/* pixels -> clip space */
	vec2 scale;
	vec2 translate;
} pc;

layout(location = 0) in vec4 in_rect;
layout(location = 1) in vec4 in_uv;
layout(location = 2) in vec4 in_clip;
layout(location = 3) in vec4 in_radii;
layout(location = 4) in vec4 in_color;
layout(location = 5) in vec4 in_border_color;
layout(location = 6) in float in_border_width;
layout(location = 7) in uint in_flags;

layout(location = 0) out vec2 v_uv;
layout(location = 1) flat out vec4 v_rect;
layout(location = 2) flat out vec4 v_clip;
layout(location = 3) flat out vec4 v_radii;
/* premultiplied */
layout(location = 4) flat out vec4 v_color;
layout(location = 5) flat out vec4 v_border_color;
layout(location = 6) flat out float v_border_width;
layout(location = 7) flat out uint v_flags;
/* the four values of in_uv as they are: the parameters of a glass primitive */
layout(location = 8) flat out vec4 v_params;
/* 1 / framebuffer size: pixels -> texture coordinates of the backdrop */
layout(location = 9) flat out vec2 v_inv_size;

void main() {
	uint kind = in_flags & PRIM_MASK;
	vec2 corner = vec2(float(gl_VertexIndex & 1), float(gl_VertexIndex >> 1));
	vec2 lo = in_rect.xy;
	vec2 hi = in_rect.zw;

	if (kind == PRIM_RECT || kind == PRIM_GLASS) {
		/* half a pixel more for the anti-aliased edge of unsnapped rectangles */
		lo -= 0.5;
		hi += 0.5;
	} else if (kind == PRIM_SHADOW) {
		float grow = in_border_width + 1.0;
		lo -= grow;
		hi += grow;
	} else if (kind == PRIM_LINE) {
		float grow = in_radii.x * 0.5 + 1.0;
		lo = min(in_rect.xy, in_rect.zw) - grow;
		hi = max(in_rect.xy, in_rect.zw) + grow;
	} else if (kind == PRIM_TRIANGLE) {
		lo = min(min(in_rect.xy, in_rect.zw), in_radii.xy) - 1.0;
		hi = max(max(in_rect.xy, in_rect.zw), in_radii.xy) + 1.0;
	}

	vec2 position = mix(lo, hi, corner);

	/* texture coordinates follow the rectangle, also outside of it */
	vec2 size = max(in_rect.zw - in_rect.xy, vec2(1e-6));
	v_uv = mix(in_uv.xy, in_uv.zw, (position - in_rect.xy) / size);

	v_rect = in_rect;
	v_clip = in_clip;
	v_radii = in_radii;
	v_color = vec4(in_color.rgb * in_color.a, in_color.a);
	v_border_color = vec4(in_border_color.rgb * in_border_color.a, in_border_color.a);
	v_border_width = in_border_width;
	v_flags = in_flags;
	v_params = in_uv;
	v_inv_size = pc.scale * 0.5;

	gl_Position = vec4(position * pc.scale + pc.translate, 0.0, 1.0);
}
