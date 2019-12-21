#include "util.h"

#include "GL/gl3w.h"
#include "GLFW/glfw3.h"

#include <assert.h>
#include <fstream>
#include <functional>
#include <iterator> 
#include <tuple> 
#include <vector> 
#include <vmath.h>

#define SEED 5370157038

using namespace std;
using namespace vmath;

void print_arr(const GLfloat *arr, int size, int row_size) {
	char str[64];

	OutputDebugString("\nPRINTING ARR:\n");

	for (int i = 0; i < size; i++) {
		memset(str, '\0', 64);
		if (arr[i] >= 0) {
			str[0] = ' ';
			sprintf(str + 1, "%.2f ", arr[i]);
		}
		else {
			sprintf(str, "%.2f ", arr[i]);
		}
		OutputDebugString(str);

		if (((i + 1) % row_size) == 0) {
			OutputDebugString("\n");
		}
	}

	OutputDebugString("\nDONE\n");
}

// Temporary workaround to inability to package shader sources with executable.
namespace {
	// Fill these with actual sources before running.
	const char vshader_src[] = R"(#version 450 core

#define CHUNK_WIDTH 16
#define CHUNK_DEPTH 16
#define CHUNK_HEIGHT 256
#define CHUNK_SIZE (CHUNK_WIDTH * CHUNK_DEPTH * CHUNK_HEIGHT)

layout (location = 0) in vec4 position;
layout (location = 1) in uint block_type; // fed in via instance array!

// Quad input
layout (location = 2) in uint q_block_type;
layout (location = 3) in ivec3 q_corner1;
layout (location = 4) in ivec3 q_corner2;
layout (location = 5) in ivec3 q_face;

out vec4 vs_color;
out flat uint vs_block_type;
out vec2 vs_tex_coords; // texture coords in [0.0, 1.0]
out flat uint horizontal;
out flat ivec3 vs_face;


layout (std140, binding = 0) uniform UNI_IN
{
	// member			base alignment			base offset		size	aligned offset	extra info
	mat4 mv_matrix;		// 16 (same as vec4)	0				64		0 				(mv_matrix[0])
						//						0						16				(mv_matrix[1])
						//						0						32				(mv_matrix[2])
						//						0						48				(mv_matrix[3])
	mat4 proj_matrix;	// 16 (same as vec4)	64				64		64				(proj_matrix[0])
						//						80						80				(proj_matrix[1])
						//						96						96				(proj_matrix[2])
						//						112						112				(proj_matrix[3])
	ivec3 base_coords;	// 16 (same as vec4)	128				12		128
} uni;

float rand(float seed) {
	return fract(1.610612741 * seed);
}

void main(void)
{
	/* GENERATE CORNER BASED ON VERTEX ID & OUR 2 OPPOSITE CORNERS */

	vec4 offset_in_chunk = vec4(q_corner1, 0);
	ivec3 diffs = q_corner2 - q_corner1;

	// figure out which index stays the same and which indices change
	int zero_idx = diffs[0] == 0 ? 0 : diffs[1] == 0 ? 1 : 2;
	int working_idx_1 = zero_idx == 0 ? 1 : 0;
	int working_idx_2 = zero_idx == 2 ? 1 : 2;

	// if working along x axis, rotate texture 90 degrees to correct rotation
	if (diffs[0] == 0) {
		working_idx_1 ^= working_idx_2;
		working_idx_2 ^= working_idx_1;
		working_idx_1 ^= working_idx_2;
	}

	horizontal = zero_idx == 1 ? 1 : 0;

	vec4 diffs1 = vec4(0);
	vec4 diffs2 = vec4(0);
	diffs1[working_idx_1] = diffs[working_idx_1];
	diffs2[working_idx_2] = diffs[working_idx_2];

	// upside down coordinates to correct texture rotation
	vec2 possible_tex_coords[6];
	possible_tex_coords[0] = vec2(diffs[working_idx_1], diffs[working_idx_2]);
	possible_tex_coords[1] = vec2(0, diffs[working_idx_2]);
	possible_tex_coords[2] = vec2(diffs[working_idx_1], 0);
	possible_tex_coords[3] = vec2(diffs[working_idx_1], 0);
	possible_tex_coords[4] = vec2(0, diffs[working_idx_2]);
	possible_tex_coords[5] = vec2(0,0);
	
	vec4 possible_offsets[6];
	possible_offsets[0] = vec4(q_corner1, 0);
	possible_offsets[1] = vec4(q_corner1, 0) + diffs1;
	possible_offsets[2] = vec4(q_corner1, 0) + diffs2;
	possible_offsets[3] = vec4(q_corner1, 0) + diffs2;
	possible_offsets[4] = vec4(q_corner1, 0) + diffs1;
	possible_offsets[5] = vec4(q_corner2, 0);

	offset_in_chunk = possible_offsets[gl_VertexID];
	vs_tex_coords = possible_tex_coords[gl_VertexID];

	/* CREATE OUR OFFSET VARIABLE */

	vec4 chunk_base = vec4(uni.base_coords.x * 16, uni.base_coords.y, uni.base_coords.z * 16, 0);
	vec4 instance_offset = chunk_base + offset_in_chunk;
	instance_offset[3] = 1;

	/* ADD IT TO VERTEX */
	gl_Position = uni.proj_matrix * uni.mv_matrix * instance_offset;

	// set color
	int seed = int(instance_offset[0]) ^ int(instance_offset[1]) ^ int(instance_offset[2]);

	vec4 block_type_to_color[255];
	block_type_to_color[0] = vec4(0.8 + rand(seed)*0.2, 0.0, 0.0, 1.0); // Air
	block_type_to_color[1] = vec4(0.4, 0.4, 0.4, 1.0) + vec4(rand(seed), rand(seed), rand(seed), 0)*0.2; // Stone
	block_type_to_color[2] = vec4(0.2, 0.8 + rand(seed) * 0.2, 0.0, 1.0); // Grass
	block_type_to_color[9] = vec4(32/255.0, 58/255.0, 230/255.0, 0.7) + vec4(rand(seed), rand(seed), rand(seed), 0)*0.2; // Water
	block_type_to_color[17] = vec4(87/255.0, 60/255.0, 10/255.0, 1.0) + vec4(rand(seed), rand(seed), rand(seed), 0)*0.2; // Oak Log
	block_type_to_color[18] = vec4(96/255.0, 148/255.0, 98/255.0, 1.0) + vec4(rand(seed), rand(seed), rand(seed), 0)*0.2; // Oak Leaves
	block_type_to_color[100] = vec4(0.0, 0.0, 0.0, 1.0);

	vs_color = block_type_to_color[q_block_type];

    vs_block_type = q_block_type;
	vs_face = q_face;
}

// shader starts executing here
void main_cubes(void)
{
	vs_block_type = block_type;

	// TODO: Add chunk offset

	// Given gl_InstanceID, calculate 3D coordinate relative to chunk origin
	int remainder = gl_InstanceID;
	int y = remainder / CHUNK_HEIGHT;
	remainder -= y * CHUNK_WIDTH * CHUNK_DEPTH; // TODO: I think this should be y * CHUNK_HEIGHT
	int z = remainder / CHUNK_DEPTH;
	remainder -= z * CHUNK_WIDTH; // TODO: I think this should be z * CHUNK_DEPTH
	int x = remainder;

	/* CREATE OUR OFFSET VARIABLE */

	vec4 chunk_base = vec4(uni.base_coords.x * 16, uni.base_coords.y, uni.base_coords.z * 16, 0);
	vec4 offset_in_chunk = vec4(x, y, z, 0);
	vec4 instance_offset = chunk_base + offset_in_chunk;

	/* ADD IT TO VERTEX */

	gl_Position = uni.proj_matrix * uni.mv_matrix * (position + instance_offset);
	vs_color = position * 2.0 + vec4(0.5, 0.5, 0.5, 1.0);

	int seed = gl_VertexID * gl_InstanceID;
	switch(block_type) {
		case 0: // air (just has a color for debugging purposes)
			vs_color = vec4(0.7, 0.7, 0.7, 1.0);
			break;
		case 1: // grass
			vs_color = vec4(0.2, 0.8 + rand(seed) * 0.2, 0.0, 1.0); // green
			break;
		case 2: // stone
			vs_color = vec4(0.4, 0.4, 0.4, 1.0) + vec4(rand(seed), rand(seed), rand(seed), rand(seed))*0.2; // grey
			break;
		default:
			vs_color = vec4(1.0, 0.0, 1.0, 1.0); // SUPER NOTICEABLE (for debugging)
			break;
	}

	// if top corner, make it darker!
	if (position.y > 0) {
		vs_color /= 2;
	}
}
)";
	const char fshader_src[] = R"(#version 450 core

in vec4 vs_color;
in flat uint vs_block_type;
in vec2 vs_tex_coords; // texture coords in [0.0, 1.0]
in flat uint horizontal; // 0 = quad is vertical, 1 = quad is horizontal (TODO: Remove this once we start inputting quad face to vertex shader.)
in flat ivec3 vs_face;

out vec4 color;

// our grass texture!
layout (binding = 0) uniform sampler2D grass_top;
layout (binding = 1) uniform sampler2D grass_side;

// texture arrays
layout (binding = 2) uniform sampler2DArray top_textures;
layout (binding = 3) uniform sampler2DArray side_textures;
layout (binding = 4) uniform sampler2DArray bottom_textures;

void main(void)
{
	// TODO: Remove branching.
	if (vs_face[1] > 0) {
		color = texture(top_textures,  vec3(vs_tex_coords, vs_block_type));
	} else if (vs_face[1] < 0) {
		color = texture(bottom_textures,  vec3(vs_tex_coords, vs_block_type));
	} else {
		color = texture(side_textures,  vec3(vs_tex_coords, vs_block_type));
	}

	if (color.a < 0.5) {
		discard;
	}
}
)";
	const char gshader_src[] = R"(#version 450 core

layout (triangles) in;
layout (triangle_strip, max_vertices = 3) out;

in uint vs_block_type[];
in vec4 vs_color[];

out vec4 gs_color;

void main(void)
{
	// // don't draw air
	// if (vs_block_type[0] == 0) {
	// 	return; 
	// }

	for (int i = 0; i < 3; i++) {		
		gl_Position = gl_in[i].gl_Position;
		gs_color = vs_color[i];
		EmitVertex();
	}
}
)";
	const char tcsshader_src[] = R"()";
	const char tesshader_src[] = R"()";

}

GLuint link_program(GLuint program) {
	// link program w/ error-checking

	glLinkProgram(program); // link together all attached shaders

	// CHECK IF LINKING SUCCESSFUL
	GLint status = GL_TRUE;
	glGetProgramiv(program, GL_LINK_STATUS, &status);
	if (status == GL_FALSE)
	{
		GLint logLen;
		glGetProgramiv(program, GL_INFO_LOG_LENGTH, &logLen);
		std::vector <char> log(logLen);
		GLsizei written;
		glGetProgramInfoLog(program, logLen, &written, log.data());

		OutputDebugString("linking error with program:\n\n");
		OutputDebugString(log.data());
		OutputDebugString("\n");
		exit(1);
	}

	return program;
}

GLuint compile_shaders(std::vector <std::tuple<std::string, GLenum>> shader_fnames) {
	GLuint program;
	std::vector <GLuint> shaders; // store compiled shaders

	// for each input shader
	for (const auto&[fname, shadertype] : shader_fnames)
	{
		// load shader src
		std::ifstream shader_file(fname);

		if (!shader_file.is_open()) {
			char buf[1024];
			sprintf(buf, "could not open shader file: %s\n", fname.c_str());
			WindowsException(buf);
			exit(1);
		}

		const std::string shader_src((std::istreambuf_iterator<char>(shader_file)), std::istreambuf_iterator<char>());
		const GLchar * shader_src_ptr = shader_src.c_str();

		// Create and compile shader
		const GLuint shader = glCreateShader(shadertype); // create empty shader
		glShaderSource(shader, 1, &shader_src_ptr, NULL); // set shader source code
		glCompileShader(shader); // compile shader

	// CHECK IF COMPILATION SUCCESSFUL
		GLint status = GL_TRUE;
		glGetShaderiv(shader, GL_COMPILE_STATUS, &status);
		if (status == GL_FALSE)
		{
			GLint logLen;
			glGetShaderiv(shader, GL_INFO_LOG_LENGTH, &logLen);
			std::vector <char> log(logLen);
			GLsizei written;
			glGetShaderInfoLog(shader, logLen, &written, log.data());

			OutputDebugString("compilation error with shader ");
			OutputDebugString(fname.c_str());
			OutputDebugString(":\n\n");
			OutputDebugString(log.data());
			OutputDebugString("\n");
			exit(1);
		}

		// Close file, save shader for later
		shader_file.close();
		shaders.push_back(shader);
	}

	// Create program, attach shaders to it, and link it
	program = glCreateProgram(); // create (empty?) program

	// attach shaders
	for (const GLuint &shader : shaders) {
		glAttachShader(program, shader);
	}

	// link program
	program = link_program(program);

	// Delete the shaders as the program has them now
	for (const GLuint &shader : shaders) {
		glDeleteShader(shader);
	}

	return program;
}

// create a (deterministic) 2D random gradient
vec2 rand_grad(int seed, int x, int y) {
	vec2 result;
	auto h = hash<int>();
	srand(h(seed) ^ h(x) ^ h(y));

	for (int i = 0; i < result.size(); i++) {
		result[i] = ((float)rand() / (float)RAND_MAX);
	}

	result = normalize(result);

	return result;
}

// apply ease curve to p, which should be a number btwn 0 and 1
// p -> 3p^2 - 2p^3
double ease(double p) {
	assert(0 <= p && p <= 1 && "Invalid p value.");
	return 3 * pow(p, 2) - 2 * pow(p, 3);
}

// calculate weighted avg
double weighted_avg(double point, double left, double right) {
	assert(0 <= point && point <= 1 && "Invalid p value 2.");

	return left + (right - left)*point;
}

// perlin function gives us y value at (x,z).
// for now call it (x,y) for math-clarity
double noise2d(float x, float y) {
	// get coordinates
	int xMin = (int)floorf(x);
	int xMax = (int)ceilf(x);
	int yMin = (int)floorf(y);
	int yMax = (int)ceilf(y);

	ivec2 coords[4] = {
		{ xMin, yMin },
		{ xMax, yMin },
		{ xMin, yMax },
		{ xMax, yMax },
	};

	int len = sizeof(coords) / sizeof(coords[0]);

	// get "random" gradient for each coordinate
	vec2 grads[4];

	for (int i = 0; i < len; i++) {
		grads[i] = rand_grad(SEED, coords[i][0], coords[i][1]);
	}

	// generate vectors from coordinates to (x,y)
	vec2 vecs_to_xy[4];

	for (int i = 0; i < len; i++) {
		vecs_to_xy[i] = vec2(x, y) - vec2((float)coords[i][0], (float)coords[i][1]);
	}

	// dot product vec_to_xy with grad
	float dots[4];

	for (int i = 0; i < len; i++) {
		dots[i] = dot(grads[i], vecs_to_xy[i]);
	}

	// get x-dimension weight, Sx = ease(x-x0)
	double Sx = ease(x - xMin);

	// apply weighted avg along top corners (where both have y=yMin), and along bottom corners (where both have y=yMax)
	double a = weighted_avg(Sx, dots[0], dots[1]);
	double b = weighted_avg(Sx, dots[2], dots[3]);

	// get y-dimension weight, Sy = ease(y-y0)
	double Sy = ease(y - yMin);

	// apply weighted avg using our two previous weighted avgs
	double z = weighted_avg(Sy, a, b);

	z = (z + 1.0) / 2.0;
	assert(z >= 0.0 && z <= 1.0);

	return z;
}

