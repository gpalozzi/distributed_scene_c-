#ifdef _WIN32
#pragma warning (disable: 4224)
#define GLEW_STATIC
#include "GL/glew.h"
#include "GLFW/glfw3.h"
#endif

#ifdef __APPLE__
//#include <GLUT/GLUT.h>
#include <OpenGL/gl.h>
#include "GLFW/glfw3.h"
#endif

#include "scene_distributed.h"
#include "obj_parser.h"
#include "serialization.hpp"
#include "image.h"
#include "id_reference.h"
#include "server.h"
#include <boost/asio.hpp>
#include <thread>
#include <boost/thread/thread.hpp>

#include <iostream>

#include <boost/archive/tmpdir.hpp>

#include <boost/archive/text_iarchive.hpp>
#include <boost/archive/text_oarchive.hpp>

#include <boost/serialization/base_object.hpp>
#include <boost/serialization/utility.hpp>
#include <boost/serialization/vector.hpp>
#include <boost/serialization/map.hpp>
#include <boost/serialization/assume_abstract.hpp>


#include <cstdio>
#include <sstream>

#define PICOJSON_USE_INT64

using boost::asio::ip::tcp;


int gl_program_id = 0;          // OpenGL program handle
int gl_vertex_shader_id = 0;    // OpenGL vertex shader handle
int gl_fragment_shader_id = 0;  // OpenGL fragment shader handle
map<image3f*,int> gl_texture_id;// OpenGL texture handles


//map<timestamp_t,Mesh*> mesh_history;

// check if an OpenGL error
void error_if_glerror() {
	auto error = glGetError();
	error_if_not(error == GL_NO_ERROR, "gl error");
	
}

// check if an OpenGL shader is compiled
void error_if_shader_not_valid(int shader_id) {
	int iscompiled;
	glGetShaderiv(shader_id, GL_COMPILE_STATUS, &iscompiled);
	if(not iscompiled) {
		char buf[10000];
		glGetShaderInfoLog(shader_id, 10000, 0, buf);
		error("shader not compiled\n\n%s\n\n",buf);
	}
}

// check if an OpenGL program is valid
void error_if_program_not_valid(int prog_id) {
	int islinked;
	glGetProgramiv(prog_id, GL_LINK_STATUS, &islinked);
	if(not islinked) {
		char buf[10000];
		glGetProgramInfoLog(prog_id, 10000, 0, buf);
		error("program not linked\n\n%s\n\n",buf);
	}
	
	int isvalid;
	glValidateProgram(prog_id);
	glGetProgramiv(prog_id, GL_VALIDATE_STATUS, &isvalid);
	if(not isvalid) {
		char buf[10000];
		glGetProgramInfoLog(prog_id, 10000, 0, buf);
		error("program not validated\n\n%s\n\n",buf);
	}
}

// initialize the shaders
void init_shaders() {
	// load shader code from files
	auto vertex_shader_code = load_text_file("../src/vertex.glsl");
	auto fragment_shader_code = load_text_file("../src/fragment.glsl");
	auto vertex_shader_codes = (char *)vertex_shader_code.c_str();
	auto fragment_shader_codes = (char *)fragment_shader_code.c_str();
	
	// create shaders
	gl_vertex_shader_id = glCreateShader(GL_VERTEX_SHADER);
	gl_fragment_shader_id = glCreateShader(GL_FRAGMENT_SHADER);
	
	// load shaders code onto the GPU
	glShaderSource(gl_vertex_shader_id,1,(const char**)&vertex_shader_codes,nullptr);
	glShaderSource(gl_fragment_shader_id,1,(const char**)&fragment_shader_codes,nullptr);
	
	// compile shaders
	glCompileShader(gl_vertex_shader_id);
	glCompileShader(gl_fragment_shader_id);
	
	// check if shaders are valid
	error_if_glerror();
	error_if_shader_not_valid(gl_vertex_shader_id);
	error_if_shader_not_valid(gl_fragment_shader_id);
	
	// create program
	gl_program_id = glCreateProgram();
	
	// attach shaders
	glAttachShader(gl_program_id,gl_vertex_shader_id);
	glAttachShader(gl_program_id,gl_fragment_shader_id);
	
	// bind vertex attributes locations
	glBindAttribLocation(gl_program_id, 0, "vertex_pos");
	glBindAttribLocation(gl_program_id, 1, "vertex_norm");
	glBindAttribLocation(gl_program_id, 2, "vertex_texcoord");
	
	// link program
	glLinkProgram(gl_program_id);
	
	// check if program is valid
	error_if_glerror();
	error_if_program_not_valid(gl_program_id);
}

// initialize the textures
void init_textures(Scene* scene) {
	// grab textures from scene
	auto textures = get_textures(scene);
	// foreach texture
	for(auto texture : textures) {
		// if already in the gl_texture_id map, skip
		if(gl_texture_id.find(texture) != gl_texture_id.end()) continue;
		// gen texture id
		unsigned int id = 0;
		glGenTextures(1, &id);
		// set id to the gl_texture_id map for later use
		gl_texture_id[texture] = id;
		// bind texture
		glBindTexture(GL_TEXTURE_2D, id);
		// set texture filtering parameters
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
		glTexParameteri(GL_TEXTURE_2D, GL_GENERATE_MIPMAP, GL_TRUE);
		// load texture data
		glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA,
					 texture->width(), texture->height(),
					 0, GL_RGB, GL_FLOAT, texture->data());
	}
}

// utility to bind texture parameters for shaders
// uses texture name, texture_on name, texture pointer and texture unit position
void _bind_texture(string name_map, string name_on, image3f* txt, int pos) {
	// if txt is not null
	if(txt) {
		// set texture on boolean parameter to true
		glUniform1i(glGetUniformLocation(gl_program_id,name_on.c_str()),GL_TRUE);
		// activate a texture unit at position pos
		glActiveTexture(GL_TEXTURE0+pos);
		// bind texture object to it from gl_texture_id map
		glBindTexture(GL_TEXTURE_2D, gl_texture_id[txt]);
		// set texture parameter to the position pos
		glUniform1i(glGetUniformLocation(gl_program_id, name_map.c_str()), pos);
	} else {
		// set texture on boolean parameter to false
		glUniform1i(glGetUniformLocation(gl_program_id,name_on.c_str()),GL_FALSE);
		// activate a texture unit at position pos
		glActiveTexture(GL_TEXTURE0+pos);
		// set zero as the texture id
		glBindTexture(GL_TEXTURE_2D, 0);
	}
}

// render the scene with OpenGL
void shade(Scene* scene, bool wireframe) {
	// enable depth test
	glEnable(GL_DEPTH_TEST);
	glDepthFunc(GL_LEQUAL);
	// disable culling face
	glDisable(GL_CULL_FACE);
	// let the shader control the points
	glEnable(GL_POINT_SPRITE);
	
	// set up the viewport from the scene image size
	glViewport(0, 0, scene->image_width, scene->image_height);
	
	// clear the screen (both color and depth) - set cleared color to background
	glClearColor(scene->background.x, scene->background.y, scene->background.z, 1);
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
	
	// enable program
	glUseProgram(gl_program_id);
	
	// bind camera's position, inverse of frame and projection
	// use frame_to_matrix_inverse and frustum_matrix
	glUniform3fv(glGetUniformLocation(gl_program_id,"camera_pos"),
				 1, &scene->camera->frame.o.x);
	glUniformMatrix4fv(glGetUniformLocation(gl_program_id,"camera_frame_inverse"),
					   1, true, &frame_to_matrix_inverse(scene->camera->frame)[0][0]);
	glUniformMatrix4fv(glGetUniformLocation(gl_program_id,"camera_projection"),
					   1, true, &frustum_matrix(-scene->camera->dist*scene->camera->width/2, scene->camera->dist*scene->camera->width/2,
												-scene->camera->dist*scene->camera->height/2, scene->camera->dist*scene->camera->height/2,
												scene->camera->dist,10000)[0][0]);
	
	// bind ambient and number of lights
	glUniform3fv(glGetUniformLocation(gl_program_id,"ambient"),1,&scene->ambient.x);
	glUniform1i(glGetUniformLocation(gl_program_id,"lights_num"),scene->lights.size());
	
	// foreach light
	auto count = 0;
	for(auto light : scene->lights) {
		// bind light position and internsity (create param name with tostring)
		glUniform3fv(glGetUniformLocation(gl_program_id,tostring("light_pos[%d]",count).c_str()),
					 1, &light->frame.o.x);
		glUniform3fv(glGetUniformLocation(gl_program_id,tostring("light_intensity[%d]",count).c_str()),
					 1, &light->intensity.x);
		count++;
	}
	
	// foreach mesh
	for(auto mesh : scene->meshes) {
		// bind material kd, ks, n
		glUniform3fv(glGetUniformLocation(gl_program_id,"material_kd"),
					 1,&mesh->mat->kd.x);
		glUniform3fv(glGetUniformLocation(gl_program_id,"material_ks"),
					 1,&mesh->mat->ks.x);
		glUniform1f(glGetUniformLocation(gl_program_id,"material_n"),
					mesh->mat->n);
		// bind texture params (txt_on, sampler)
		_bind_texture("material_kd_txt", "material_kd_txt_on", mesh->mat->kd_txt, 0);
		_bind_texture("material_ks_txt", "material_ks_txt_on", mesh->mat->ks_txt, 1);
		_bind_texture("material_norm_txt", "material_norm_txt_on", mesh->mat->norm_txt, 2);
		
		// bind mesh frame - use frame_to_matrix
		glUniformMatrix4fv(glGetUniformLocation(gl_program_id,"mesh_frame"),
						   1,true,&frame_to_matrix(mesh->frame)[0][0]);
		
		// enable vertex attributes arrays and set up pointers to the mesh data
		auto vertex_pos_location = glGetAttribLocation(gl_program_id, "vertex_pos");
		auto vertex_norm_location = glGetAttribLocation(gl_program_id, "vertex_norm");
		auto vertex_texcoord_location = glGetAttribLocation(gl_program_id, "vertex_texcoord");
		glEnableVertexAttribArray(vertex_pos_location);
		glVertexAttribPointer(vertex_pos_location, 3, GL_FLOAT, GL_FALSE, 0, &mesh->pos[0].x);
		glEnableVertexAttribArray(vertex_norm_location);
		glVertexAttribPointer(vertex_norm_location, 3, GL_FLOAT, GL_FALSE, 0, &mesh->norm[0].x);
		if(not mesh->texcoord.empty()) {
			glEnableVertexAttribArray(vertex_texcoord_location);
			glVertexAttribPointer(vertex_texcoord_location, 2, GL_FLOAT, GL_FALSE, 0, &mesh->texcoord[0].x);
		}
		else glVertexAttrib2f(vertex_texcoord_location, 0, 0);
		
		// draw triangles and quads
		if(not wireframe) {
            //            if(mesh->triangle.size()) glDrawElements(GL_TRIANGLES, mesh->triangle_index.size()*3, GL_UNSIGNED_INT, &mesh->triangle_index[0].x);
            //            if(mesh->quad.size()) glDrawElements(GL_QUADS, mesh->quad_index.size()*4, GL_UNSIGNED_INT, &mesh->quad_index[0].x);
            if(mesh->triangle.size()) glDrawRangeElements(GL_TRIANGLES, 0, mesh->pos.size() - 1,  mesh->triangle_index.size()*3, GL_UNSIGNED_INT, &mesh->triangle_index[0].x);
            if(mesh->quad.size()) glDrawRangeElements(GL_QUADS, 0, mesh->pos.size() - 1, mesh->quad_index.size()*4, GL_UNSIGNED_INT, &mesh->quad_index[0].x);
		} else {
			//auto edges = EdgeMap(mesh->triangle, mesh->quad).edges();
            //if(mesh->edge.size()) glDrawElements(GL_LINES, mesh->edge_index.size()*2, GL_UNSIGNED_INT, &mesh->edge_index[0].x);
            if(mesh->edge.size()) glDrawRangeElements(GL_LINES,  0, mesh->pos.size() - 1, mesh->edge_index.size()*2, GL_UNSIGNED_INT, &mesh->edge_index[0].x);
		}
		
		// draw line sets
		if(not mesh->line.empty()) glDrawElements(GL_LINES, mesh->line.size()*2, GL_UNSIGNED_INT, mesh->line.data());
		if (not mesh->spline.empty()) {
			for(auto segment : mesh->spline) glDrawElements(GL_LINE_STRIP, 4, GL_UNSIGNED_INT, &segment);
		}
		
		// disable vertex attribute arrays
		glDisableVertexAttribArray(vertex_pos_location);
		glDisableVertexAttribArray(vertex_norm_location);
		if(not mesh->texcoord.empty()) glDisableVertexAttribArray(vertex_texcoord_location);
	}
}

string scene_filename;          // scene filename
string image_filename;          // image filename
Scene* scene;                   // scene arrays
Scene* scene2;
bool save = false;              // whether to start the save loop
bool wireframe = false;         // display as wireframe
bool mesh_mat = false;
bool mesh_ver = false;
bool mesh_rot = false;
bool check = false;
bool restore_old = false;
bool write_log    = false;
bool sync_scene = false;
int c = 1;

// glfw callback for character input
void character_callback(GLFWwindow* window, unsigned int key) {
	switch (key) {
		case 's':
			save = true;
			break;
		case 'w':
			wireframe = not wireframe;
			break;
			//        case 'm':
			//            mesh_mat = true;
			//            break;
			//        case 'v':
			//            mesh_ver = true;
			//            break;
			//        case 'r':
			//            mesh_rot = true;
			//            break;
		case 'c':
			check = true;
			break;
		case 'r':
			restore_old = true;
			break;
        case 'l':
            write_log = true;
            break;
        case 'a':
            sync_scene = true;
            break;

	}
}


void save_mesh(const Mesh &m, const char * filename){
    // make an archive
    std::ofstream ofs(filename);
    boost::archive::text_oarchive oa(ofs);
    oa << m;
}

void restore_mesh(Mesh &m, const char * filename)
{
    // open the archive
    std::ifstream ifs(filename);
    boost::archive::text_iarchive ia(ifs);
    
    // restore the schedule from the archive
    ia >> m;
}

void save_meshdiff(const MeshDiff &m, const char * filename){
    // make an archive
    std::ofstream ofs(filename);
    boost::archive::text_oarchive oa(ofs);
    oa << m;
}

void restore_meshdiff(MeshDiff &m, const char * filename)
{
    // open the archive
    std::ifstream ifs(filename);
    boost::archive::text_iarchive ia(ifs);
    
    // restore the schedule from the archive
    ia >> m;
}

// uiloop
void uiloop(editor_server* server) {
	auto ok = glfwInit();
	error_if_not(ok, "glfw init error");
	
	glfwWindowHint(GLFW_SAMPLES, scene->image_samples);
	
	auto window = glfwCreateWindow(scene->image_width,
								   scene->image_height,
								   "distributed scene | server monitor", NULL, NULL);
	error_if_not(window, "glfw window error");
	
	glfwMakeContextCurrent(window);
	
	glfwSetCharCallback(window, character_callback);
	glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_NORMAL);
	
#ifdef _WIN32
	auto ok1 = glewInit();
	error_if_not(GLEW_OK == ok1, "glew init error");
#endif
	
	init_shaders();
	init_textures(scene);
	
	auto mouse_last_x = -1.0;
	auto mouse_last_y = -1.0;
	
	while(not glfwWindowShouldClose(window)) {
		glfwGetFramebufferSize(window, &scene->image_width, &scene->image_height);
		scene->camera->width = (scene->camera->height * scene->image_width) / scene->image_height;
		
		shade(scene,wireframe);
		
		if(glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_LEFT)) {
			double x, y;
			glfwGetCursorPos(window, &x, &y);
			if (mouse_last_x < 0 or mouse_last_y < 0) { mouse_last_x = x; mouse_last_y = y; }
			auto delta_x = x - mouse_last_x, delta_y = y - mouse_last_y;
			
			set_view_turntable(scene->camera, delta_x*0.01, -delta_y*0.01, 0, 0, 0);
			
			mouse_last_x = x;
			mouse_last_y = y;
		} else { mouse_last_x = -1; mouse_last_y = -1; }
		
		if(save) {
			auto image = image3f(scene->image_width,scene->image_height);
			glReadPixels(0, 0, scene->image_width, scene->image_height, GL_RGB, GL_FLOAT, &image.at(0,0));
			write_png(image_filename, image, true);
			save = false;
		}
		
		if(mesh_mat){
			auto mat = scene->ids_map[5].as_material();
			auto rnd = random()%10;
			auto rnd2 = random()%10;
			if (rnd > rnd2) {
				mat->kd = vec3f((float) (1/++rnd),(float) (1/++rnd2),1);
			}
			else
				mat->kd = vec3f(1,(float) (1/++rnd),(float) (1/++rnd2));
			server->remove_front_op();
			mesh_mat = false;
		}
		if(mesh_ver){
			auto mesh = scene->ids_map[7].as_mesh();
			mesh->pos[214] += transform_vector(mesh->frame, z3f);
			server->remove_front_op();
			mesh_ver = false;
		}
		if(mesh_rot){
			auto mesh = scene->ids_map[7].as_mesh();
			auto m = translation_matrix(mesh->frame.o) * rotation_matrix(x3f.x, mesh->frame.x) * rotation_matrix(x3f.y, mesh->frame.y) * rotation_matrix(x3f.z, mesh->frame.z) * translation_matrix(- mesh->frame.o);
			mesh->frame = transform_frame(m, mesh->frame);
			server->remove_front_op();
			mesh_rot = false;
		}
//		  to do: implement if needed
        if(sync_scene){
            timestamp_t old = get_global_version();
            auto version = restore_version(scene);
            
            if(old != version){
                stringstream restore_message;
                restore_message << "restore_version " << version;
                server->write_all(restore_message.str().c_str());
            }
            
            sync_scene = false;
        }

		while(server->has_pending_op()){
			//prendi la prossima operazione
			auto msg = server->get_next_pending_op();
			//eseguila
			switch (msg[0]) {
				case 'm':
					mesh_mat = true;
					break;
				case 'v':
					mesh_ver = true;
					break;
				case 'r':
					mesh_rot = true;
			}
			//rimuovila
			server->remove_front_op();
		}
		
		while(server->has_pending_mesh()){
            // get next message
            auto msg = server->get_next_pending();
            // switch between message type
            switch (msg->type()) {
                case 0: // Message
                {
                    auto message_tokens = msg->as_message();
                    if(message_tokens[0] == "restore_version"){
                        restore_to_version(scene, atoll(message_tokens[1].c_str()));
                        message("scene restored to version %llu", message_tokens[1].c_str());
                    }
                }
                    break;
                case 1: // Mesh
                {
                    // get the next mesh recived
                    auto mesh = msg->as_mesh();
                    // save mesh in history and update mesh
                    swap_mesh(mesh, scene, true);
                    // remove from queue
                    server->remove_first();
                }
                    break;
                case 2: // SceneDiff
                {
                    timing("deserialize_from_msg[start]");
                    auto scenediff = msg->as_scenediff();
                    timing("deserialize_from_msg[end]");
                    // apply differences
                    timing("apply_diff[start]");
                    apply_change_reverse(scene, scenediff, scenediff->_label);
                    timing("apply_diff[end]");
                    message("actual mesh version: %d\n", scene->meshes[0]->_version);
                    // remove from queue
                    // remove from queue
                    server->remove_first();
                }
                    break;
                case 3: // MeshDiff
                {
                    // get the next mesh recived
                    timing("deserialize_from_msg[start]");
                    auto meshdiff = msg->as_meshdiff();
                    timing("deserialize_from_msg[end]");
                    // apply differences
                    timing("apply_diff[start]");
                    timing_apply("apply_diff[start]");
                    apply_mesh_change(scene->meshes[0], meshdiff, get_timestamp());
                    timing("apply_diff[end]");
                    timing_apply("apply_diff[end]");
                    message("actual mesh version: %d\n", scene->meshes[0]->_version);
                    // remove from queue
                    server->remove_first();
                }
                    break;
                case 7: // OBJ - MTL
                {
                    // parse material and scene from obj & mtl
                    auto tmp_scene = new Scene();
                    obj_parse_materials(tmp_scene, msg->as_mtl());
                    scene = obj_parse_scene(msg->as_obj(), false, tmp_scene);
                    auto light = new Light();
                    light->_id_ = 126128947120701270;
                    light->frame.o = vec3f(2.0,6.0,5.0);
                    light->intensity = vec3f(25.0,25.0,25.0);
                    scene->meshes[0]->frame.o = vec3f(-2.0,0.0,-1.0);
                    scene->lights.push_back(light);
                    scene->camera = lookat_camera(vec3f(1.0,6.0,10.0), zero3f, y3f, 1.0f, 1.0f, 1.0f,129849216921865, 0);

                    
                    message("scene reloaded\n");
                    // remove from queue
                    server->remove_first();
                }
                    break;
                default:
                    break;
            }
        }
		
        if(write_log){
            save_timing("server_timing_v" + scene_filename + to_string(scene->meshes[0]->_version));
            write_log = false;
        }
		
		if(check){
            scene2 = load_obj_scene("../scenes/fat_v"+ to_string(c++) +".obj");
            scene2->meshes[0]->frame.o = vec3f(-2.0,0.0,-1.0);

            auto diff = meshdiff_assume_ordered(scene->meshes[0], scene2->meshes[0]);
            obj_apply_mesh_change(scene->meshes[0], diff, 0);
            c = c % 5;
            check = false;
		}
		glfwSwapBuffers(window);
		glfwPollEvents();
	}
	
	glfwDestroyWindow(window);
	
	glfwTerminate();
}

// main function
int main(int argc, char** argv) {
	auto args = parse_cmdline(argc, argv,
							  { "3D viewer", "show 3d scene",
								  {  {"resolution", "r", "image resolution", typeid(int), true, jsonvalue() }  },
								  {  {"scene_filename", "", "scene filename", typeid(string), false, jsonvalue("scene.json")},
									  {"image_filename", "", "image filename", typeid(string), true, jsonvalue("")}  }
							  });
	scene_filename = args.object_element("scene_filename").as_string();
	image_filename = (args.object_element("image_filename").as_string() != "") ?
	args.object_element("image_filename").as_string() :
	scene_filename.substr(0,scene_filename.size()-5)+".png";
    scene = load_json_scene("../scenes/shuttleply_v0.json");
    scene->background = zero3f;
    
    //scene = load_json_scene("../scenes/shuttleply_v" + scene_filename + ".json");
    timing("parse_scene[end]");
	if(not args.object_element("resolution").is_null()) {
		scene->image_height = args.object_element("resolution").as_int();
		scene->image_width = scene->camera->width * scene->image_height / scene->camera->height;
	}
	
	// not needed
	//subdivide(scene);
    
	// start
	
	try {
		boost::asio::io_service io_service;
		
		tcp::endpoint endpoint(tcp::v4(), 3311);
		editor_server server(io_service, endpoint);
		std::thread t([&io_service](){ io_service.run(); });
        
		uiloop(&server);
		server.close();
		io_service.stop();
		
		t.join();
		
	}
	catch (std::exception& e){
		std::cerr << "Exception: " << e.what() << "\n";
	}
	
	return 0;
}

