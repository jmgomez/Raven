#version 450

/*
 * HUI - reference fragment shader.
 *
 * Shapes are evaluated as signed distances in pixels, which gives exact
 * anti-aliasing at any size without tessellation. The output color is
 * premultiplied: blend with (ONE, ONE_MINUS_SRC_ALPHA).
 */

const uint PRIM_RECT     = 0u;
const uint PRIM_GLYPH    = 1u;
const uint PRIM_SHADOW   = 2u;
const uint PRIM_LINE     = 3u;
const uint PRIM_TRIANGLE = 4u;
const uint PRIM_MASK     = 15u;
const uint FLAG_TEXTURED = 16u;

layout(set = 0, binding = 0) uniform sampler2D u_texture;

layout(location = 0) in vec2 v_uv;
layout(location = 1) flat in vec4 v_rect;
layout(location = 2) flat in vec4 v_clip;
layout(location = 3) flat in vec4 v_radii;
layout(location = 4) flat in vec4 v_color;
layout(location = 5) flat in vec4 v_border_color;
layout(location = 6) flat in float v_border_width;
layout(location = 7) flat in uint v_flags;

layout(location = 0) out vec4 out_color;

/* Rounded box centered on the origin; radii = top-left, top-right, bottom-right, bottom-left. */
float sd_round_box(vec2 p, vec2 half_size, vec4 radii) {
	float radius = p.x < 0.0 ? (p.y < 0.0 ? radii.x : radii.w) : (p.y < 0.0 ? radii.y : radii.z);
	radius = min(radius, min(half_size.x, half_size.y));
	vec2 q = abs(p) - half_size + radius;
	return min(max(q.x, q.y), 0.0) + length(max(q, 0.0)) - radius;
}

float sd_segment(vec2 p, vec2 a, vec2 b) {
	vec2 pa = p - a;
	vec2 ba = b - a;
	float h = clamp(dot(pa, ba) / max(dot(ba, ba), 1e-6), 0.0, 1.0);
	return length(pa - ba * h);
}

/* Works for both windings. */
float sd_triangle(vec2 p, vec2 p0, vec2 p1, vec2 p2) {
	vec2 e0 = p1 - p0, e1 = p2 - p1, e2 = p0 - p2;
	vec2 v0 = p - p0, v1 = p - p1, v2 = p - p2;
	vec2 pq0 = v0 - e0 * clamp(dot(v0, e0) / max(dot(e0, e0), 1e-6), 0.0, 1.0);
	vec2 pq1 = v1 - e1 * clamp(dot(v1, e1) / max(dot(e1, e1), 1e-6), 0.0, 1.0);
	vec2 pq2 = v2 - e2 * clamp(dot(v2, e2) / max(dot(e2, e2), 1e-6), 0.0, 1.0);
	float s = sign(e0.x * e2.y - e0.y * e2.x);
	vec2 d = min(min(
		vec2(dot(pq0, pq0), s * (v0.x * e0.y - v0.y * e0.x)),
		vec2(dot(pq1, pq1), s * (v1.x * e1.y - v1.y * e1.x))),
		vec2(dot(pq2, pq2), s * (v2.x * e2.y - v2.y * e2.x)));
	return -sqrt(d.x) * sign(d.y);
}

void main() {
	vec2 p = gl_FragCoord.xy;
	if (p.x < v_clip.x || p.y < v_clip.y || p.x >= v_clip.z || p.y >= v_clip.w) discard;

	uint kind = v_flags & PRIM_MASK;
	vec4 color = v_color;
	if ((v_flags & FLAG_TEXTURED) != 0u) {
		vec4 texel = textureLod(u_texture, v_uv, 0.0);
		/* textures are straight alpha, v_color is already premultiplied */
		color *= vec4(texel.rgb * texel.a, texel.a);
	}

	float coverage = 1.0;
	if (kind == PRIM_RECT) {
		vec2 half_size = (v_rect.zw - v_rect.xy) * 0.5;
		float d = sd_round_box(p - (v_rect.xy + half_size), half_size, v_radii);
		coverage = clamp(0.5 - d, 0.0, 1.0);
		if (v_border_width > 0.0) {
			float inside = clamp(0.5 - (d + v_border_width), 0.0, 1.0);
			color = mix(v_border_color, color, inside);
		}
	} else if (kind == PRIM_SHADOW) {
		vec2 half_size = (v_rect.zw - v_rect.xy) * 0.5;
		float d = sd_round_box(p - (v_rect.xy + half_size), half_size, v_radii);
		float blur = max(v_border_width, 0.5);
		coverage = 1.0 - smoothstep(-blur, blur, d);
	} else if (kind == PRIM_LINE) {
		float d = sd_segment(p, v_rect.xy, v_rect.zw) - v_radii.x * 0.5;
		coverage = clamp(0.5 - d, 0.0, 1.0);
	} else if (kind == PRIM_TRIANGLE) {
		float d = sd_triangle(p, v_rect.xy, v_rect.zw, v_radii.xy);
		coverage = clamp(0.5 - d, 0.0, 1.0);
	}

	out_color = color * coverage;
}
