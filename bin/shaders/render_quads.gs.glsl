#version 450 core

layout (points) in;
layout (triangle_strip, max_vertices = 4) out;

layout (std140, binding = 0) uniform UNI_IN
{
	// member			base alignment			base offset		size	aligned offset	extra info
	mat4 mv_matrix;		// 16 (same as vec4)	0				64		16 				(mv_matrix[0])
						//						16						32				(mv_matrix[1])
						//						32						48				(mv_matrix[2])
						//						48						64				(mv_matrix[3])
	mat4 proj_matrix;	// 16 (same as vec4)	64				64		80				(proj_matrix[0])
						//						80						96				(proj_matrix[1])
						//						96						112				(proj_matrix[2])
						//						112						128				(proj_matrix[3])
	uint in_water;      // 4                    128             4       132
} uni;

in uint vs_block_type[];
in ivec3 vs_face[];
in ivec3 vs_corner1[];
in ivec3 vs_corner2[];
in ivec3 vs_base_coords[];

out vec2 gs_tex_coords;
out flat uint gs_block_type;
out flat ivec3 gs_face;

// TODO: improve texture coord logic using my realization that (0,0) is bottom-left corner, not top-left.
void main(void)
{
	// data passthrough
	gs_block_type = vs_block_type[0];
	gs_face = vs_face[0];

	ivec3 corner1 = vs_corner1[0];
	ivec3 corner2 = vs_corner2[0];
	ivec3 diffs = corner2 - corner1;

	// figure out which index stays the same and which indices change
	// TODO: pass in face as int, then get zero_idx by face % 3, and regenerate face vec easily
	int zero_idx = diffs[0] == 0 ? 0 : diffs[1] == 0 ? 1 : 2;
	int working_idx_1 = zero_idx == 0 ? 1 : 0;
	int working_idx_2 = zero_idx == 2 ? 1 : 2;

	// if working along x axis, rotate texture 90 degrees to correct rotation
	if (diffs[0] == 0) {
		working_idx_1 ^= working_idx_2;
		working_idx_2 ^= working_idx_1;
		working_idx_1 ^= working_idx_2;
	}

	vec4 diffs1 = vec4(0);
	vec4 diffs2 = vec4(0);
	diffs1[working_idx_1] = diffs[working_idx_1];
	diffs2[working_idx_2] = diffs[working_idx_2];

	vec4 chunk_base = vec4(vs_base_coords[0].x * 16, vs_base_coords[0].y, vs_base_coords[0].z * 16, 0);

	gl_Position = uni.proj_matrix * uni.mv_matrix * (vec4(chunk_base.xyz + corner1, 1)); gs_tex_coords = vec2(diffs[working_idx_1], diffs[working_idx_2]); EmitVertex();
	gl_Position = uni.proj_matrix * uni.mv_matrix * (vec4(chunk_base.xyz + corner1, 1) + diffs1); gs_tex_coords = vec2(0, diffs[working_idx_2]); EmitVertex();
	gl_Position = uni.proj_matrix * uni.mv_matrix * (vec4(chunk_base.xyz + corner1, 1) + diffs2); gs_tex_coords = vec2(diffs[working_idx_1], 0); EmitVertex();
	gl_Position = uni.proj_matrix * uni.mv_matrix * (vec4(chunk_base.xyz + corner2, 1)); gs_tex_coords = vec2(0,0); EmitVertex();
}
