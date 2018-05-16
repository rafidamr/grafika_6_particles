// Include standard headers
#include <stdio.h>
#include <stdlib.h>
#include <vector>

// Include GLEW
#include <GL/glew.h>

// Include GLFW
#include <GLFW/glfw3.h>
GLFWwindow* window;

// Include GLUT
#include <GL/glut.h>

// Include GLM
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtx/norm.hpp>
using namespace glm;

#include <common/shader.hpp>
#include <common/texture.hpp>
#include <common/controls.hpp>
#include <common/objloader.hpp>
#include <common/vboindexer.hpp>
#include <assimp/Importer.hpp>      // C++ importer interface
#include <assimp/scene.h>           // Output data structure
#include <assimp/postprocess.h>     // Post processing flags

// CPU representation of a particle
struct Particle {
	glm::vec3 pos, speed;
	unsigned char r, g, b, a; // Color
	float size, angle, weight;
	float life; // Remaining life of the particle. if <0 : dead and unused.
	float cameradistance; // *Squared* distance to the camera. if dead : -1.0f

	bool operator<(const Particle& that) const {
		// Sort in reverse order : far particles drawn first.
		return this->cameradistance > that.cameradistance;
	}
};

const int MaxParticles = 100000;
Particle ParticlesContainer[100 * MaxParticles];
Particle RaindropsContainer[MaxParticles];
int LastUsedParticle = 0;

// Finds a Particle in ParticlesContainer which isn't used yet.
// (i.e. life < 0);
int FindUnusedParticle() {

	for (int i = LastUsedParticle; i<MaxParticles; i++) {
		if (ParticlesContainer[i].life < 0) {
			LastUsedParticle = i;
			return i;
		}
	}

	for (int i = 0; i<LastUsedParticle; i++) {
		if (ParticlesContainer[i].life < 0) {
			LastUsedParticle = i;
			return i;
		}
	}

	return 0; // All particles are taken, override the first one
}

void SortParticles() {
	std::sort(&ParticlesContainer[0], &ParticlesContainer[MaxParticles]);
}

void SortRaindrops() {
	std::sort(&RaindropsContainer[0], &RaindropsContainer[MaxParticles]);
}

int main(void)
{
	// Initialise GLFW
	if (!glfwInit())
	{
		fprintf(stderr, "Failed to initialize GLFW\n");
		getchar();
		return -1;
	}

	glfwWindowHint(GLFW_SAMPLES, 4);
	glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
	glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
	glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE); // To make MacOS happy; should not be needed
	glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

	// Open a window and create its OpenGL context
	window = glfwCreateWindow(1024, 768, "Tutorial 09 - Loading with AssImp", NULL, NULL);
	if (window == NULL) {
		fprintf(stderr, "Failed to open GLFW window. If you have an Intel GPU, they are not 3.3 compatible. Try the 2.1 version of the tutorials.\n");
		getchar();
		glfwTerminate();
		return -1;
	}
	glfwMakeContextCurrent(window);

	// Initialize GLEW
	glewExperimental = true; // Needed for core profile
	if (glewInit() != GLEW_OK) {
		fprintf(stderr, "Failed to initialize GLEW\n");
		getchar();
		glfwTerminate();
		return -1;
	}

	// Ensure we can capture the escape key being pressed below
	glfwSetInputMode(window, GLFW_STICKY_KEYS, GL_TRUE);
	// Hide the mouse and enable unlimited mouvement
	glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);

	// Set the mouse at the center of the screen
	glfwPollEvents();
	glfwSetCursorPos(window, 1024 / 2, 768 / 2);

	// Dark blue background
	glClearColor(0.502f, 0.847f, 1.0f, 0.0f);

	// Enable depth test
	glEnable(GL_DEPTH_TEST);
	// Accept fragment if it closer to the camera than the former one
	glDepthFunc(GL_LESS);

	// Cull triangles which normal is not towards the camera
	glEnable(GL_CULL_FACE);

	GLuint VertexArrayID;
	glGenVertexArrays(1, &VertexArrayID);
	glBindVertexArray(VertexArrayID);


	// Create and compile our GLSL program from the shaders
	GLuint programIDCar = LoadShaders("StandardShading.vertexshader", "StandardShading.fragmentshader");

	// Get a handle for our "MVP" uniform
	GLuint MatrixID = glGetUniformLocation(programIDCar, "MVP");
	GLuint ViewMatrixID = glGetUniformLocation(programIDCar, "V");
	GLuint ModelMatrixID = glGetUniformLocation(programIDCar, "M");

	// Load the texture
	GLuint TextureCar = loadDDS("uvmap.DDS");

	// Get a handle for our "myTextureSampler" uniform
	GLuint TextureIDCar = glGetUniformLocation(programIDCar, "myTextureSampler");

	// Read our .obj file
	std::vector<unsigned short> indices;
	std::vector<glm::vec3> indexed_vertices;
	std::vector<glm::vec2> indexed_uvs;
	std::vector<glm::vec3> indexed_normals;
	bool res = loadAssImp("humvee.obj", indices, indexed_vertices, indexed_uvs, indexed_normals);

	// Load it into a VBO

	GLuint vertexbuffer;
	glGenBuffers(1, &vertexbuffer);
	glBindBuffer(GL_ARRAY_BUFFER, vertexbuffer);
	glBufferData(GL_ARRAY_BUFFER, indexed_vertices.size() * sizeof(glm::vec3), &indexed_vertices[0], GL_STATIC_DRAW);

	GLuint uvbuffer;
	glGenBuffers(1, &uvbuffer);
	glBindBuffer(GL_ARRAY_BUFFER, uvbuffer);
	glBufferData(GL_ARRAY_BUFFER, indexed_uvs.size() * sizeof(glm::vec2), &indexed_uvs[0], GL_STATIC_DRAW);

	GLuint normalbuffer;
	glGenBuffers(1, &normalbuffer);
	glBindBuffer(GL_ARRAY_BUFFER, normalbuffer);
	glBufferData(GL_ARRAY_BUFFER, indexed_normals.size() * sizeof(glm::vec3), &indexed_normals[0], GL_STATIC_DRAW);

	// Generate a buffer for the indices as well
	GLuint elementbuffer;
	glGenBuffers(1, &elementbuffer);
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, elementbuffer);
	glBufferData(GL_ELEMENT_ARRAY_BUFFER, indices.size() * sizeof(unsigned short), &indices[0], GL_STATIC_DRAW);

	// Get a handle for our "LightPosition" uniform
	glUseProgram(programIDCar);
	GLuint LightID = glGetUniformLocation(programIDCar, "LightPosition_worldspace");

	// For speed computation
	double lastTime = glfwGetTime();
	int nbFrames = 0;


	//================================================ SMOKE PARTICLES =============================================

	// Create and compile our GLSL program from the shaders
	GLuint programID = LoadShaders("Particle.vertexshader", "Particle.fragmentshader");

	// Vertex shader
	GLuint CameraRight_worldspace_ID = glGetUniformLocation(programID, "CameraRight_worldspace");
	GLuint CameraUp_worldspace_ID = glGetUniformLocation(programID, "CameraUp_worldspace");
	GLuint ViewProjMatrixID = glGetUniformLocation(programID, "VP");

	// fragment shader
	GLuint TextureID = glGetUniformLocation(programID, "myTextureSampler");


	static GLfloat* g_particule_position_size_data = new GLfloat[MaxParticles * 4];
	static GLubyte* g_particule_color_data = new GLubyte[MaxParticles * 4];

	for (int i = 0; i<MaxParticles; i++) {
		ParticlesContainer[i].life = -1.0f;
		ParticlesContainer[i].cameradistance = -1.0f;
	}

	GLuint Texture = loadDDS("particle.DDS");

	// The VBO containing the 4 vertices of the particles.
	// Thanks to instancing, they will be shared by all particles.
	static const GLfloat g_vertex_buffer_data[] = {
		-0.5f, -0.5f, 0.0f,
		0.5f, -0.5f, 0.0f,
		-0.5f,  0.5f, 0.0f,
		0.5f,  0.5f, 0.0f,
	};
	GLuint billboard_vertex_buffer;
	glGenBuffers(1, &billboard_vertex_buffer);
	glBindBuffer(GL_ARRAY_BUFFER, billboard_vertex_buffer);
	glBufferData(GL_ARRAY_BUFFER, sizeof(g_vertex_buffer_data), g_vertex_buffer_data, GL_STATIC_DRAW);

	// The VBO containing the positions and sizes of the particles
	GLuint particles_position_buffer;
	glGenBuffers(1, &particles_position_buffer);
	glBindBuffer(GL_ARRAY_BUFFER, particles_position_buffer);
	// Initialize with empty (NULL) buffer : it will be updated later, each frame.
	glBufferData(GL_ARRAY_BUFFER, MaxParticles * 4 * sizeof(GLfloat), NULL, GL_STREAM_DRAW);

	// The VBO containing the colors of the particles
	GLuint particles_color_buffer;
	glGenBuffers(1, &particles_color_buffer);
	glBindBuffer(GL_ARRAY_BUFFER, particles_color_buffer);
	// Initialize with empty (NULL) buffer : it will be updated later, each frame.
	glBufferData(GL_ARRAY_BUFFER, MaxParticles * 4 * sizeof(GLubyte), NULL, GL_STREAM_DRAW);



	//============================================ END SMOKE PARTICLES =============================================


	//================================================ RAIN PARTICLES =============================================

	// Create and compile our GLSL program from the shaders
	GLuint programIDRain = LoadShaders("ParticleRain.vertexshader", "ParticleRain.fragmentshader");

	// Vertex shader
	GLuint CameraRight_worldspace_ID_rain = glGetUniformLocation(programIDRain, "CameraRight_worldspace");
	GLuint CameraUp_worldspace_ID_rain = glGetUniformLocation(programIDRain, "CameraUp_worldspace");
	GLuint ViewProjMatrixID_rain = glGetUniformLocation(programIDRain, "VP");

	// fragment shader
	GLuint TextureIDRain = glGetUniformLocation(programIDRain, "myTextureSampler");

	static GLfloat* g_particule_position_size_data_rain = new GLfloat[MaxParticles * 4];
	static GLubyte* g_particule_color_data_rain = new GLubyte[MaxParticles * 4];

	for (int i = 0; i<MaxParticles; i++) {
		RaindropsContainer[i].life = -1.0f;
		RaindropsContainer[i].cameradistance = -1.0f;
	}

	GLuint Texture_rain = loadDDS("raindrop.DDS");

	// The VBO containing the 4 vertices of the particles.
	// Thanks to instancing, they will be shared by all particles.
	static const GLfloat g_vertex_buffer_data_rain[] = {
		-0.05f, 0.6f, 0.2f,
		0.05f, 0.6f, 0.2f,
		0.1f,  0.5f, 0.2f,
		0.1f, -0.5f, 0.5f,
		0.05f, -0.6f, 0.5f,
		-0.05f, -0.6f, 0.5f,
		-0.1f, -0.5f, 0.5f,
		-0.1f,  0.5f, 0.2f,
	};
	GLuint billboard_vertex_buffer_rain;
	glGenBuffers(1, &billboard_vertex_buffer_rain);
	glBindBuffer(GL_ARRAY_BUFFER, billboard_vertex_buffer_rain);
	glBufferData(GL_ARRAY_BUFFER, sizeof(g_vertex_buffer_data_rain), g_vertex_buffer_data_rain, GL_STATIC_DRAW);

	// The VBO containing the positions and sizes of the particles
	GLuint particles_position_buffer_rain;
	glGenBuffers(1, &particles_position_buffer_rain);
	glBindBuffer(GL_ARRAY_BUFFER, particles_position_buffer_rain);
	// Initialize with empty (NULL) buffer : it will be updated later, each frame.
	glBufferData(GL_ARRAY_BUFFER, MaxParticles * 4 * sizeof(GLfloat), NULL, GL_STREAM_DRAW);

	// The VBO containing the colors of the particles
	GLuint particles_color_buffer_rain;
	glGenBuffers(1, &particles_color_buffer_rain);
	glBindBuffer(GL_ARRAY_BUFFER, particles_color_buffer_rain);
	// Initialize with empty (NULL) buffer : it will be updated later, each frame.
	glBufferData(GL_ARRAY_BUFFER, MaxParticles * 4 * sizeof(GLubyte), NULL, GL_STREAM_DRAW);



	//============================================ END RAIN PARTICLES =============================================

	float lastTimeCheck = lastTime;

	do {

		// Measure speed
		double currentTime = glfwGetTime();
		nbFrames++;
		if (currentTime - lastTime >= 1.0) { // If last prinf() was more than 1sec ago
											 // printf and reset
			lastTime += 1.0;
		}

		if (currentTime - lastTimeCheck >= 1.0) { // If last prinf() was more than 1sec ago
												  // printf and reset
			printf("%f frame/s\n", double(nbFrames));
			nbFrames = 0;
			lastTimeCheck += 1.0;
		}

		// Clear the screen
		glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

		// Use our shader
		glUseProgram(programIDCar);

		// Compute the MVP matrix from keyboard and mouse input
		computeMatricesFromInputs();
		glm::mat4 ProjectionMatrix = getProjectionMatrix();
		glm::mat4 ViewMatrix = getViewMatrix();
		glm::mat4 ModelMatrix = glm::mat4(1.0);
		glm::mat4 MVP = ProjectionMatrix * ViewMatrix * ModelMatrix;

		// Compute ambience, specularity, and diffusement
		GLfloat current_ambience_factor = getAmbienceFactor();
		GLfloat current_specular_factor = getSpecularFactor();
		GLfloat current_diffuse_factor = getDiffuseFactor();

		// Send our transformation to the currently bound shader, 
		// in the "MVP" uniform
		glUniformMatrix4fv(MatrixID, 1, GL_FALSE, &MVP[0][0]);
		glUniformMatrix4fv(ModelMatrixID, 1, GL_FALSE, &ModelMatrix[0][0]);
		glUniformMatrix4fv(ViewMatrixID, 1, GL_FALSE, &ViewMatrix[0][0]);

		//Send ambience, specularity, and diffusement to shader
		GLuint ambience_id = glGetUniformLocation(programIDCar, "ambience_factor");
		GLuint specular_id = glGetUniformLocation(programIDCar, "specular_factor");
		GLuint diffuse_id = glGetUniformLocation(programIDCar, "diffuse_factor");
		glUniform1f(ambience_id, current_ambience_factor);
		glUniform1f(diffuse_id, current_diffuse_factor);
		glUniform1f(specular_id, current_specular_factor);

		glm::vec3 lightPos = glm::vec3(0, 10, 1.2);
		glUniform3f(LightID, lightPos.x, lightPos.y, lightPos.z);

		// Bind our texture in Texture Unit 0
		glActiveTexture(GL_TEXTURE0);
		glBindTexture(GL_TEXTURE_2D, TextureCar);
		// Set our "myTextureSampler" sampler to use Texture Unit 0
		glUniform1i(TextureIDCar, 0);

		// 1rst attribute buffer : vertices
		glEnableVertexAttribArray(0);
		glBindBuffer(GL_ARRAY_BUFFER, vertexbuffer);
		glVertexAttribPointer(
			0,                  // attribute
			3,                  // size
			GL_FLOAT,           // type
			GL_FALSE,           // normalized?
			0,                  // stride
			(void*)0            // array buffer offset
		);

		// 2nd attribute buffer : UVs
		glEnableVertexAttribArray(1);
		glBindBuffer(GL_ARRAY_BUFFER, uvbuffer);
		glVertexAttribPointer(
			1,                                // attribute
			2,                                // size
			GL_FLOAT,                         // type
			GL_FALSE,                         // normalized?
			0,                                // stride
			(void*)0                          // array buffer offset
		);

		// 3rd attribute buffer : normals
		glEnableVertexAttribArray(2);
		glBindBuffer(GL_ARRAY_BUFFER, normalbuffer);
		glVertexAttribPointer(
			2,                                // attribute
			3,                                // size
			GL_FLOAT,                         // type
			GL_FALSE,                         // normalized?
			0,                                // stride
			(void*)0                          // array buffer offset
		);

		// Index buffer
		glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, elementbuffer);

		// Draw the triangles !
		glDrawElements(
			GL_TRIANGLES,      // mode
			indices.size(),    // count
			GL_UNSIGNED_SHORT,   // type
			(void*)0           // element array buffer offset
		);

		glDisableVertexAttribArray(0);
		glDisableVertexAttribArray(1);
		glDisableVertexAttribArray(2);


		//============================================ SMOKE PARTICLES ==============================================

		double delta = currentTime - lastTime;
		lastTime = currentTime;

		// We will need the camera's position in order to sort the particles
		// w.r.t the camera's distance.
		// There should be a getCameraPosition() function in common/controls.cpp, 
		// but this works too.
		glm::vec3 CameraPosition(glm::inverse(ViewMatrix)[3]);

		glm::mat4 ViewProjectionMatrix = ProjectionMatrix * ViewMatrix;


		// Generate 10 new particule each millisecond,
		// but limit this to 16 ms (60 fps), or if you have 1 long frame (1sec),
		// newparticles will be huge and the next frame even longer.
		int newparticles = (int)(delta*10000.0);
		if (newparticles > (int)(0.016f*10000.0))
			newparticles = (int)(0.016f*10000.0);

		for (int i = 0; i<newparticles; i++) {
			int particleIndex = FindUnusedParticle();
			ParticlesContainer[particleIndex].life = 1.0f; // This particle will live 5 seconds.
			ParticlesContainer[particleIndex].pos = glm::vec3(2, 1.5f, -7.0f);

			float spread = 2.5f;
			glm::vec3 maindir = glm::vec3(0.0f, 1.5f, -10.0f);
			// Very bad way to generate a random direction; 
			// See for instance http://stackoverflow.com/questions/5408276/python-uniform-spherical-distribution instead,
			// combined with some user-controlled parameters (main direction, spread, etc)
			glm::vec3 randomdir = glm::vec3(
				(rand() % 2000 - 1000.0f) / 1000.0f,
				-(rand() % 2000 - 1000.0f) / 1000.0f,
				(rand() % 2000 - 1000.0f) / 1000.0f
			);

			ParticlesContainer[particleIndex].speed = maindir + randomdir*spread;


			// Very bad way to generate a random color
			ParticlesContainer[particleIndex].r = rand() % 10 + 170;
			ParticlesContainer[particleIndex].g = rand() % 10 + 170;
			ParticlesContainer[particleIndex].b = rand() % 10 + 170;
			ParticlesContainer[particleIndex].a = (rand() % 256) / 3;

			ParticlesContainer[particleIndex].size = (rand() % 1000) / 2000.0f + 0.1f;

		}



		// Simulate all particles
		int ParticlesCount = 0;
		for (int i = 0; i<MaxParticles; i++) {

			Particle& p = ParticlesContainer[i]; // shortcut

			if (p.life > 0.0f) {

				// Decrease life
				p.life -= delta;
				if (p.life > 0.0f) {

					// Simulate simple physics : gravity only, no collisions
					p.speed += glm::vec3(0.0f, 1.0f, 0.0f) * (float)delta * 2.0f;
					p.pos += p.speed * (float)delta;
					p.cameradistance = glm::length2(p.pos - CameraPosition);
					//ParticlesContainer[i].pos += glm::vec3(0.0f,10.0f, 0.0f) * (float)delta;

					// Fill the GPU buffer
					g_particule_position_size_data[4 * ParticlesCount + 0] = p.pos.x;
					g_particule_position_size_data[4 * ParticlesCount + 1] = p.pos.y;
					g_particule_position_size_data[4 * ParticlesCount + 2] = p.pos.z;

					g_particule_position_size_data[4 * ParticlesCount + 3] = p.size;

					g_particule_color_data[4 * ParticlesCount + 0] = p.r;
					g_particule_color_data[4 * ParticlesCount + 1] = p.g;
					g_particule_color_data[4 * ParticlesCount + 2] = p.b;
					g_particule_color_data[4 * ParticlesCount + 3] = p.a;

				}
				else {
					// Particles that just died will be put at the end of the buffer in SortParticles();
					p.cameradistance = -1.0f;
				}

				ParticlesCount++;

			}
		}

		SortParticles();


		// Update the buffers that OpenGL uses for rendering.
		// There are much more sophisticated means to stream data from the CPU to the GPU, 
		// but this is outside the scope of this tutorial.
		// http://www.opengl.org/wiki/Buffer_Object_Streaming

		glBindBuffer(GL_ARRAY_BUFFER, particles_position_buffer);
		glBufferData(GL_ARRAY_BUFFER, MaxParticles * 4 * sizeof(GLfloat), NULL, GL_STREAM_DRAW); // Buffer orphaning, a common way to improve streaming perf. See above link for details.
		glBufferSubData(GL_ARRAY_BUFFER, 0, ParticlesCount * sizeof(GLfloat) * 4, g_particule_position_size_data);

		glBindBuffer(GL_ARRAY_BUFFER, particles_color_buffer);
		glBufferData(GL_ARRAY_BUFFER, MaxParticles * 4 * sizeof(GLubyte), NULL, GL_STREAM_DRAW); // Buffer orphaning, a common way to improve streaming perf. See above link for details.
		glBufferSubData(GL_ARRAY_BUFFER, 0, ParticlesCount * sizeof(GLubyte) * 4, g_particule_color_data);

		// BLEND STILL NEEDS A PIX !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
		glEnablei(4, GL_BLEND);
		glBlendFunci(4, GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
		glEnablei(5, GL_BLEND);
		glBlendFunci(5, GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
		glEnablei(6, GL_BLEND);
		glBlendFunci(6, GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

		// Use our shader
		glUseProgram(programID);

		// Bind our texture in Texture Unit 0
		glActiveTexture(GL_TEXTURE1);
		glBindTexture(GL_TEXTURE_2D, Texture);
		// Set our "myTextureSampler" sampler to use Texture Unit 0
		glUniform1i(TextureID, 1);

		// Same as the billboards tutorial
		glUniform3f(CameraRight_worldspace_ID, ViewMatrix[0][0], ViewMatrix[1][0], ViewMatrix[2][0]);
		glUniform3f(CameraUp_worldspace_ID, ViewMatrix[0][1], ViewMatrix[1][1], ViewMatrix[2][1]);

		glUniformMatrix4fv(ViewProjMatrixID, 1, GL_FALSE, &ViewProjectionMatrix[0][0]);

		// 1rst attribute buffer : vertices
		glEnableVertexAttribArray(4);
		glBindBuffer(GL_ARRAY_BUFFER, billboard_vertex_buffer);
		glVertexAttribPointer(
			4,                  // attribute. No particular reason for 0, but must match the layout in the shader.
			3,                  // size
			GL_FLOAT,           // type
			GL_FALSE,           // normalized?
			0,                  // stride
			(void*)0            // array buffer offset
		);

		// 2nd attribute buffer : positions of particles' centers
		glEnableVertexAttribArray(5);
		glBindBuffer(GL_ARRAY_BUFFER, particles_position_buffer);
		glVertexAttribPointer(
			5,                                // attribute. No particular reason for 1, but must match the layout in the shader.
			4,                                // size : x + y + z + size => 4
			GL_FLOAT,                         // type
			GL_FALSE,                         // normalized?
			0,                                // stride
			(void*)0                          // array buffer offset
		);

		// 3rd attribute buffer : particles' colors
		glEnableVertexAttribArray(6);
		glBindBuffer(GL_ARRAY_BUFFER, particles_color_buffer);
		glVertexAttribPointer(
			6,                                // attribute. No particular reason for 1, but must match the layout in the shader.
			4,                                // size : r + g + b + a => 4
			GL_UNSIGNED_BYTE,                 // type
			GL_TRUE,                          // normalized?    *** YES, this means that the unsigned char[4] will be accessible with a vec4 (floats) in the shader ***
			0,                                // stride
			(void*)0                          // array buffer offset
		);

		// These functions are specific to glDrawArrays*Instanced*.
		// The first parameter is the attribute buffer we're talking about.
		// The second parameter is the "rate at which generic vertex attributes advance when rendering multiple instances"
		// http://www.opengl.org/sdk/docs/man/xhtml/glVertexAttribDivisor.xml
		glVertexAttribDivisor(4, 0); // particles vertices : always reuse the same 4 vertices -> 0
		glVertexAttribDivisor(5, 1); // positions : one per quad (its center)                 -> 1
		glVertexAttribDivisor(6, 1); // color : one per quad                                  -> 1

									 // Draw the particules !
									 // This draws many times a small triangle_strip (which looks like a quad).
									 // This is equivalent to :
									 // for(i in ParticlesCount) : glDrawArrays(GL_TRIANGLE_STRIP, 0, 4), 
									 // but faster.
		glDrawArraysInstanced(GL_TRIANGLE_STRIP, 0, 4, ParticlesCount);

		glDisableVertexAttribArray(4);
		glDisableVertexAttribArray(5);
		glDisableVertexAttribArray(6);

		//=========================================== END SMOKE PARTICLES ==============================================

		//============================================ RAIN PARTICLES ==============================================

		// Generate 10 new particule each millisecond,
		// but limit this to 16 ms (60 fps), or if you have 1 long frame (1sec),
		// newparticles will be huge and the next frame even longer.
		int newparticles_rain = (int)(delta*1000000.0);
		if (newparticles_rain  > (int)(0.016f*1000000.0))
			newparticles_rain = (int)(0.016f*1000000.0);

		for (int i = 0; i<newparticles_rain; i++) {
			int particleIndex_rain = FindUnusedParticle();
			RaindropsContainer[particleIndex_rain].life = 6.0f; // This particle will live 5 seconds.
			int x = rand() % 10;
			int z = rand() % 24;
			RaindropsContainer[particleIndex_rain].pos = glm::vec3(x - 5, 10.0f, z - 18);

			glm::vec3 maindir = glm::vec3(0.0f, -10.0f, 1.0f);
			// Very bad way to generate a random direction; 
			// See for instance http://stackoverflow.com/questions/5408276/python-uniform-spherical-distribution instead,
			// combined with some user-controlled parameters (main direction, spread, etc)
			/*glm::vec3 randomdir = glm::vec3(
			(rand() % 2000 - 1000.0f) / 1000.0f,
			-(rand() % 2000 - 1000.0f) / 1000.0f,
			(rand() % 2000 - 1000.0f) / 1000.0f
			);*/

			RaindropsContainer[particleIndex_rain].speed = maindir;


			// Very bad way to generate a random color
			RaindropsContainer[particleIndex_rain].r = rand() % 10 + 26;
			RaindropsContainer[particleIndex_rain].g = rand() % 10 + 35;
			RaindropsContainer[particleIndex_rain].b = rand() % 10 + 126;
			RaindropsContainer[particleIndex_rain].a = 50;
			RaindropsContainer[particleIndex_rain].size = (rand() % 1000) / 2000.0f + 0.1f;

		}



		// Simulate all particles
		int RaindropsCount = 0;
		for (int i = 0; i<MaxParticles; i++) {

			Particle& r = RaindropsContainer[i]; // shortcut

			if (r.life > 0.0f) {

				// Decrease life
				r.life -= delta;
				if (r.life > 0.0f) {

					// Simulate simple physics : gravity only, no collisions
					r.speed += glm::vec3(0.0f, 0.0f, 0.0f) * (float)delta * 2.0f;
					r.pos += r.speed * (float)delta;
					r.cameradistance = glm::length2(r.pos - CameraPosition);
					//ParticlesContainer[i].pos += glm::vec3(0.0f,10.0f, 0.0f) * (float)delta;

					// Fill the GPU buffer
					g_particule_position_size_data_rain[4 * RaindropsCount + 0] = r.pos.x;
					g_particule_position_size_data_rain[4 * RaindropsCount + 1] = r.pos.y;
					g_particule_position_size_data_rain[4 * RaindropsCount + 2] = r.pos.z;

					g_particule_position_size_data_rain[4 * RaindropsCount + 3] = r.size;

					g_particule_color_data_rain[4 * RaindropsCount + 0] = r.r;
					g_particule_color_data_rain[4 * RaindropsCount + 1] = r.g;
					g_particule_color_data_rain[4 * RaindropsCount + 2] = r.b;
					g_particule_color_data_rain[4 * RaindropsCount + 3] = r.a;

				}
				else {
					// Particles that just died will be put at the end of the buffer in SortParticles();
					r.cameradistance = -1.0f;
				}

				RaindropsCount++;

			}
		}

		SortRaindrops();


		// Update the buffers that OpenGL uses for rendering.
		// There are much more sophisticated means to stream data from the CPU to the GPU, 
		// but this is outside the scope of this tutorial.
		// http://www.opengl.org/wiki/Buffer_Object_Streaming

		glBindBuffer(GL_ARRAY_BUFFER, particles_position_buffer_rain);
		glBufferData(GL_ARRAY_BUFFER, MaxParticles * 4 * sizeof(GLfloat), NULL, GL_STREAM_DRAW); // Buffer orphaning, a common way to improve streaming perf. See above link for details.
		glBufferSubData(GL_ARRAY_BUFFER, 0, RaindropsCount * sizeof(GLfloat) * 4, g_particule_position_size_data_rain);

		glBindBuffer(GL_ARRAY_BUFFER, particles_color_buffer_rain);
		glBufferData(GL_ARRAY_BUFFER, MaxParticles * 4 * sizeof(GLubyte), NULL, GL_STREAM_DRAW); // Buffer orphaning, a common way to improve streaming perf. See above link for details.
		glBufferSubData(GL_ARRAY_BUFFER, 0, RaindropsCount * sizeof(GLubyte) * 4, g_particule_color_data_rain);


		// BLEND STILL NEEDS A PIX !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
		glEnablei(7, GL_BLEND);
		glBlendFunci(7, GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
		glEnablei(8, GL_BLEND);
		glBlendFunci(8, GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
		glEnablei(9, GL_BLEND);
		glBlendFunci(9, GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

		// Use our shader
		glUseProgram(programIDRain);

		// Bind our texture in Texture Unit 0
		glActiveTexture(GL_TEXTURE2);
		glBindTexture(GL_TEXTURE_2D, Texture_rain);
		// Set our "myTextureSampler" sampler to use Texture Unit 0
		glUniform1i(TextureIDRain, 2);

		// Same as the billboards tutorial
		glUniform3f(CameraRight_worldspace_ID_rain, ViewMatrix[0][0], ViewMatrix[1][0], ViewMatrix[2][0]);
		glUniform3f(CameraUp_worldspace_ID_rain, ViewMatrix[0][1], ViewMatrix[1][1], ViewMatrix[2][1]);

		glUniformMatrix4fv(ViewProjMatrixID, 1, GL_FALSE, &ViewProjectionMatrix[0][0]);

		// 1rst attribute buffer : vertices
		glEnableVertexAttribArray(7);
		glBindBuffer(GL_ARRAY_BUFFER, billboard_vertex_buffer_rain);
		glVertexAttribPointer(
			7,                  // attribute. No particular reason for 0, but must match the layout in the shader.
			3,                  // size
			GL_FLOAT,           // type
			GL_FALSE,           // normalized?
			0,                  // stride
			(void*)0            // array buffer offset
		);

		// 2nd attribute buffer : positions of particles' centers
		glEnableVertexAttribArray(8);
		glBindBuffer(GL_ARRAY_BUFFER, particles_position_buffer_rain);
		glVertexAttribPointer(
			8,                                // attribute. No particular reason for 1, but must match the layout in the shader.
			4,                                // size : x + y + z + size => 4
			GL_FLOAT,                         // type
			GL_FALSE,                         // normalized?
			0,                                // stride
			(void*)0                          // array buffer offset
		);

		// 3rd attribute buffer : particles' colors
		glEnableVertexAttribArray(9);
		glBindBuffer(GL_ARRAY_BUFFER, particles_color_buffer_rain);
		glVertexAttribPointer(
			9,                                // attribute. No particular reason for 1, but must match the layout in the shader.
			4,                                // size : r + g + b + a => 4
			GL_UNSIGNED_BYTE,                 // type
			GL_TRUE,                          // normalized?    *** YES, this means that the unsigned char[4] will be accessible with a vec4 (floats) in the shader ***
			0,                                // stride
			(void*)0                          // array buffer offset
		);

		// These functions are specific to glDrawArrays*Instanced*.
		// The first parameter is the attribute buffer we're talking about.
		// The second parameter is the "rate at which generic vertex attributes advance when rendering multiple instances"
		// http://www.opengl.org/sdk/docs/man/xhtml/glVertexAttribDivisor.xml
		glVertexAttribDivisor(7, 0); // particles vertices : always reuse the same 4 vertices -> 0
		glVertexAttribDivisor(8, 1); // positions : one per quad (its center)                 -> 1
		glVertexAttribDivisor(9, 1); // color : one per quad                                  -> 1

									 // Draw the particules !
									 // This draws many times a small triangle_strip (which looks like a quad).
									 // This is equivalent to :
									 // for(i in ParticlesCount) : glDrawArrays(GL_TRIANGLE_STRIP, 0, 4), 
									 // but faster.
		glDrawArraysInstanced(GL_TRIANGLE_STRIP, 0, 4, RaindropsCount);

		glDisableVertexAttribArray(7);
		glDisableVertexAttribArray(8);
		glDisableVertexAttribArray(9);

		// Swap buffers
		glfwSwapBuffers(window);
		glfwPollEvents();

		//=========================================== END RAIN PARTICLES ==============================================

	} // Check if the ESC key was pressed or the window was closed
	while (glfwGetKey(window, GLFW_KEY_ESCAPE) != GLFW_PRESS &&
		glfwWindowShouldClose(window) == 0);

	// Cleanup VBO and shader
	glDeleteBuffers(1, &vertexbuffer);
	glDeleteBuffers(1, &uvbuffer);
	glDeleteBuffers(1, &normalbuffer);
	glDeleteBuffers(1, &elementbuffer);
	glDeleteProgram(programIDCar);
	glDeleteTextures(1, &TextureCar);
	glDeleteVertexArrays(1, &VertexArrayID);

	// Close OpenGL window and terminate GLFW
	glfwTerminate();

	return 0;
}