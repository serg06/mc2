#version 450 core
#define DEFAULT_DISTANCE 250.0
#define PARTICLE_DISTANCE_FROM_VIEWER -10000.0f

layout( local_size_x = 16, local_size_y = 16 ) in; 

uniform float Gravity1 = 1000.0; 
uniform vec3 BlackHolePos1 = vec3(DEFAULT_DISTANCE, DEFAULT_DISTANCE, PARTICLE_DISTANCE_FROM_VIEWER); 
uniform float Gravity2 = 1000.0; 
uniform vec3 BlackHolePos2 = vec3(-DEFAULT_DISTANCE, -DEFAULT_DISTANCE, PARTICLE_DISTANCE_FROM_VIEWER); 
 
uniform float ParticleInvMass = 1.0 / 0.1; 
uniform float DeltaT = 0.05; 

// minichunk input (TODO: support multiple minichunks)
layout(std430, binding=0) buffer MINI { 
	int mini[]; 
};

// minichunk output (TODO: support multiple minichunks)
layout(std430, binding=0) buffer LAYERS { 
	int layers[]; 
};
 
void main() { 
	// index of the chunk we're working on
	uint chunk_idx = gl_WorkGroupID.x;

	// index of the face we're working on
	uint face_idx = gl_WorkGroupID.y;

	// index of the layer we're working on, relative to face
	// NOTE: this goes from 0 to 14 (inclusive) because 1 layer in each direction cannot see its face (not in mini)
	uint layer_idx = gl_WorkGroupID.z;

	// index of element in current layer
	uint layer_x = gl_LocalInvocationID.x;
	uint layer_y = gl_LocalInvocationID.y;

	// get working axes
	// ...

	// just plug in my usual gen_layer algorithm!
	

// 
//  vec3 p = Position[idx].xyz; 
//  vec3 v = Velocity[idx].xyz; 
// 
//  // Force from black hole #1 
//  vec3 d = BlackHolePos1 - p; 
//  vec3 force = (Gravity1 / length(d)) * normalize(d); 
//   
//  // Force from black hole #2 
//  d = BlackHolePos2 - p; 
//  force += (Gravity2 / length(d)) * normalize(d); 
// 
//  // Apply simple Euler integrator 
//  vec3 a = force * ParticleInvMass; 
//  Position[idx] = vec4( 
//        p + v * DeltaT + 0.5 * a * DeltaT * DeltaT, 1.0); 
//  Velocity[idx] = vec4( v + a * DeltaT, 0.0); 
}
