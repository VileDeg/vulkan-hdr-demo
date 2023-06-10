#include "stdafx.h"

#include "resources.h"
#include "Engine.h"

#include "tinyobj/tiny_obj_loader.h"

#define STB_IMAGE_IMPLEMENTATION
#include "stb/stb_image.h"

static std::string GetBaseDir(const std::string& filepath) {
	//From https://github.com/tinyobjloader/tinyobjloader
	if (filepath.find_last_of("/\\") != std::string::npos)
		return filepath.substr(0, filepath.find_last_of("/\\"));
	return "";
}

static bool FileExists(const std::string& abs_filename) {
	//From https://github.com/tinyobjloader/tinyobjloader
	bool ret;
	FILE* fp = fopen(abs_filename.c_str(), "rb");
	if (fp) {
		ret = true;
		fclose(fp);
	} else {
		ret = false;
	}

	return ret;
}

static std::string GetFileNameNoExt(const std::string& filepath) {
	std::string subs = filepath;
	auto dotp = subs.find_last_of(".");
	auto slashp = subs.find_last_of("/\\");

	if (dotp != std::string::npos) {
		subs = subs.substr(0, dotp);
	}
	if (slashp != std::string::npos) {
		subs = subs.substr(slashp, subs.length()-1-slashp);
	}

	return subs;
}

static void computeAllSmoothingNormals(tinyobj::attrib_t& attrib,
	std::vector<tinyobj::shape_t>& shapes) 
{
	// From https://github.com/tinyobjloader/tinyobjloader/blob/release/examples/viewer/viewer.cc
	struct vec3 {
		float v[3];
		vec3() {
			v[0] = 0.0f;
			v[1] = 0.0f;
			v[2] = 0.0f;
		}
	};
	vec3 p[3];
	for (size_t s = 0, slen = shapes.size(); s < slen; ++s) {
		const tinyobj::shape_t& shape(shapes[s]);
		size_t facecount = shape.mesh.num_face_vertices.size();
		assert(shape.mesh.smoothing_group_ids.size());

		for (size_t f = 0, flen = facecount; f < flen; ++f) {
			for (unsigned int v = 0; v < 3; ++v) {
				tinyobj::index_t idx = shape.mesh.indices[3 * f + v];
				assert(idx.vertex_index != -1);
				p[v].v[0] = attrib.vertices[3 * idx.vertex_index];
				p[v].v[1] = attrib.vertices[3 * idx.vertex_index + 1];
				p[v].v[2] = attrib.vertices[3 * idx.vertex_index + 2];
			}

			// cross(p[1] - p[0], p[2] - p[0])
			float nx = (p[1].v[1] - p[0].v[1]) * (p[2].v[2] - p[0].v[2]) -
				(p[1].v[2] - p[0].v[2]) * (p[2].v[1] - p[0].v[1]);
			float ny = (p[1].v[2] - p[0].v[2]) * (p[2].v[0] - p[0].v[0]) -
				(p[1].v[0] - p[0].v[0]) * (p[2].v[2] - p[0].v[2]);
			float nz = (p[1].v[0] - p[0].v[0]) * (p[2].v[1] - p[0].v[1]) -
				(p[1].v[1] - p[0].v[1]) * (p[2].v[0] - p[0].v[0]);

			// Don't normalize here.
			for (unsigned int v = 0; v < 3; ++v) {
				tinyobj::index_t idx = shape.mesh.indices[3 * f + v];
				attrib.normals[3 * idx.normal_index] += nx;
				attrib.normals[3 * idx.normal_index + 1] += ny;
				attrib.normals[3 * idx.normal_index + 2] += nz;
			}
		}
	}

	assert(attrib.normals.size() % 3 == 0);
	for (size_t i = 0, nlen = attrib.normals.size() / 3; i < nlen; ++i) {
		tinyobj::real_t& nx = attrib.normals[3 * i];
		tinyobj::real_t& ny = attrib.normals[3 * i + 1];
		tinyobj::real_t& nz = attrib.normals[3 * i + 2];
		tinyobj::real_t len = sqrtf(nx * nx + ny * ny + nz * nz);
		tinyobj::real_t scale = len == 0 ? 0 : 1 / len;
		nx *= scale;
		ny *= scale;
		nz *= scale;
	}
}

static void computeSmoothingShape(tinyobj::attrib_t& inattrib, tinyobj::shape_t& inshape,
	std::vector<std::pair<unsigned int, unsigned int>>& sortedids,
	unsigned int idbegin, unsigned int idend,
	std::vector<tinyobj::shape_t>& outshapes,
	tinyobj::attrib_t& outattrib) 
{
	// From https://github.com/tinyobjloader/tinyobjloader/blob/release/examples/viewer/viewer.cc
	unsigned int sgroupid = sortedids[idbegin].first;
	bool hasmaterials = inshape.mesh.material_ids.size();
	// Make a new shape from the set of faces in the range [idbegin, idend).
	outshapes.emplace_back();
	tinyobj::shape_t& outshape = outshapes.back();
	outshape.name = inshape.name;
	// Skip lines and points.

	std::unordered_map<unsigned int, unsigned int> remap;
	for (unsigned int id = idbegin; id < idend; ++id) {
		unsigned int face = sortedids[id].second;

		outshape.mesh.num_face_vertices.push_back(3); // always triangles
		if (hasmaterials)
			outshape.mesh.material_ids.push_back(inshape.mesh.material_ids[face]);
		outshape.mesh.smoothing_group_ids.push_back(sgroupid);
		// Skip tags.

		for (unsigned int v = 0; v < 3; ++v) {
			tinyobj::index_t inidx = inshape.mesh.indices[3 * face + v], outidx;
			assert(inidx.vertex_index != -1);
			auto iter = remap.find(inidx.vertex_index);
			// Smooth group 0 disables smoothing so no shared vertices in that case.
			if (sgroupid && iter != remap.end()) {
				outidx.vertex_index = (*iter).second;
				outidx.normal_index = outidx.vertex_index;
				outidx.texcoord_index = (inidx.texcoord_index == -1) ? -1 : outidx.vertex_index;
			} else {
				assert(outattrib.vertices.size() % 3 == 0);
				unsigned int offset = static_cast<unsigned int>(outattrib.vertices.size() / 3);
				outidx.vertex_index = outidx.normal_index = offset;
				outidx.texcoord_index = (inidx.texcoord_index == -1) ? -1 : offset;
				outattrib.vertices.push_back(inattrib.vertices[3 * inidx.vertex_index]);
				outattrib.vertices.push_back(inattrib.vertices[3 * inidx.vertex_index + 1]);
				outattrib.vertices.push_back(inattrib.vertices[3 * inidx.vertex_index + 2]);
				outattrib.normals.push_back(0.0f);
				outattrib.normals.push_back(0.0f);
				outattrib.normals.push_back(0.0f);
				if (inidx.texcoord_index != -1) {
					outattrib.texcoords.push_back(inattrib.texcoords[2 * inidx.texcoord_index]);
					outattrib.texcoords.push_back(inattrib.texcoords[2 * inidx.texcoord_index + 1]);
				}
				remap[inidx.vertex_index] = offset;
			}
			outshape.mesh.indices.push_back(outidx);
		}
	}
}

static void computeSmoothingShapes(tinyobj::attrib_t& inattrib,
	std::vector<tinyobj::shape_t>& inshapes,
	std::vector<tinyobj::shape_t>& outshapes,
	tinyobj::attrib_t& outattrib) 
{
	// From https://github.com/tinyobjloader/tinyobjloader/blob/release/examples/viewer/viewer.cc
	for (size_t s = 0, slen = inshapes.size(); s < slen; ++s) {
		tinyobj::shape_t& inshape = inshapes[s];

		unsigned int numfaces = static_cast<unsigned int>(inshape.mesh.smoothing_group_ids.size());
		assert(numfaces);
		std::vector<std::pair<unsigned int, unsigned int>> sortedids(numfaces);
		for (unsigned int i = 0; i < numfaces; ++i)
			sortedids[i] = std::make_pair(inshape.mesh.smoothing_group_ids[i], i);
		sort(sortedids.begin(), sortedids.end());

		unsigned int activeid = sortedids[0].first;
		unsigned int id = activeid, idbegin = 0, idend = 0;
		// Faces are now bundled by smoothing group id, create shapes from these.
		while (idbegin < numfaces) {
			while (activeid == id && ++idend < numfaces)
				id = sortedids[idend].first;
			computeSmoothingShape(inattrib, inshape, sortedids, idbegin, idend,
				outshapes, outattrib);
			activeid = id;
			idbegin = idend;
		}
	}
}

void Engine::UpdateSSBOData(const std::vector<std::shared_ptr<RenderObject>>& objects)
{
	// Following is the maximum pixel value of image that is used for exposure simulation
	auto& sd = _renderContext.ssboData;

	sd.exposureON = _inp.exposureEnabled;
	sd.toneMappingON = _inp.toneMappingEnabled;

	for (int i = 0; i < objects.size(); i++) {

		sd.objects[i].modelMatrix = objects[i]->Transform();
		sd.objects[i].color = objects[i]->color;
		//sd.objects[i].lightAffected = objects[i]->model->lightAffected;
		/*for (int i = 0; i < objects[i]->model->meshes.size(); i++) {
			sd.models[i].meshes->hasTextures = (objects[i]->model->meshes[i]->p_tex != nullptr);
		}*/
	}
}

void Engine::drawObjects(VkCommandBuffer cmd, const std::vector<std::shared_ptr<RenderObject>>& objects)
{
	// Load SSBO to GPU
	{ 
		UpdateSSBOData(objects);
		getCurrentFrame().objectBuffer.runOnMemoryMap(_allocator, 
			[&](void* data) {
				char* ssboData = (char*)data;
				unsigned int* newMax = (unsigned int*)(ssboData + offsetof(GPUSSBOData, newMax));
				unsigned int* oldMax = (unsigned int*)(ssboData + offsetof(GPUSSBOData, oldMax));
				float* f_oldMax = reinterpret_cast<float*>(oldMax);

				if (_frameNumber == 0) { // Initially new MAX and old MAX are zero.
					*newMax = 0;
					*oldMax = 0;
				} else { // On every next frame, swap new MAX and old MAX.
					unsigned int tmp = *oldMax;
					std::swap(*newMax, *oldMax);
					// For optimization we assume that MAX of new frame
					// won't be more then two times lower.
					*newMax = 0.5 * tmp; 
				}
				auto& sd = _renderContext.ssboData;
				sd.newMax = *newMax;
				sd.oldMax = *oldMax;

				memcpy(data, &sd, sizeof(GPUSSBOData));
			}
		);
	}

	// Load UNIFORM BUFFER of scene parameters to GPU
	{ 
		_renderContext.sceneData.cameraPos = _inp.camera.GetPos();
		_sceneParameterBuffer.runOnMemoryMap(_allocator, 
			[&](void* data) {
				char* sceneData = (char*)data;
				sceneData += pad_uniform_buffer_size(sizeof(GPUSceneData)) * _frameInFlightNum;
				memcpy(sceneData, &_renderContext.sceneData, sizeof(GPUSceneData));
			}
		);
	}
	
	glm::mat4 viewMat = _inp.camera.GetViewMat();
	glm::mat4 projMat = _inp.camera.GetProjMat(_fovY, _windowExtent.width, _windowExtent.height);
	GPUCameraData camData{
		.view = viewMat,
		.proj = projMat,
		.viewproj = projMat * viewMat,
	};
	getCurrentFrame().cameraBuffer.runOnMemoryMap(_allocator, 
		[&](void* data) {
			memcpy(data, &camData, sizeof(GPUCameraData));
		}
	);

	Mesh* lastMesh = nullptr;
	Material* lastMaterial = nullptr;
	for (int i = 0; i < objects.size(); i++) {
        const RenderObject& obj = *objects[i];
		Model* model = obj.model;

		for (int m = 0; m < model->meshes.size(); ++m) {
			Mesh* mesh = model->meshes[m];

			ASSERT(mesh && mesh->material);
			
			

			//offset for our scene buffer
			uint32_t uniform_offset = pad_uniform_buffer_size(sizeof(GPUSceneData)) * _frameInFlightNum;

			// Always add the sceneData and SSBO descriptor
			std::vector<VkDescriptorSet> sets = { getCurrentFrame().globalDescriptor, getCurrentFrame().objectDescriptor };

			VkImageView imageView = VK_NULL_HANDLE;
			if (mesh->p_tex != nullptr) {
				imageView = mesh->p_tex->imageView;
			}
			VkDescriptorImageInfo imageBufferInfo{
				.sampler = _linearSampler,
				.imageView = imageView,
				.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
			};
			VkWriteDescriptorSet texture1 =
				vkinit::write_descriptor_image(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
					VK_NULL_HANDLE, &imageBufferInfo, 0);
			vkCmdPushDescriptorSetKHR(
				cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, mesh->material->pipelineLayout, 2, 1, &texture1);

			_renderContext.pushConstantData.hasTexture = (mesh->p_tex != nullptr);
			_renderContext.pushConstantData.lightAffected = model->lightAffected;

			vkCmdPushConstants(
				cmd, mesh->material->pipelineLayout,
				VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(GPUPushConstantData),
				&_renderContext.pushConstantData);

			// If the material is different, bind the new material
			if (mesh->material != lastMaterial) {
				bindPipeline(cmd, mesh->material->pipeline);
				lastMaterial = mesh->material;
				// Bind the descriptor sets
				vkCmdBindDescriptorSets(
					cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, mesh->material->pipelineLayout, 0, sets.size(), sets.data(), 1, &uniform_offset);
			}

			if (mesh != lastMesh) {
				VkDeviceSize zeroOffset = 0;
				vkCmdBindVertexBuffers(cmd, 0, 1, &mesh->_vertexBuffer.buffer, &zeroOffset);
				lastMesh = mesh;
			}

			
		
			// Ve send loop index as instance index to use it in shader to access object data in SSBO
			vkCmdDraw(cmd, mesh->_vertices.size(), 1, 0, i);
		}
    }
}


void Engine::createSamplers() {
	// Create samplers for textures
	VkSamplerCreateInfo samplerInfo = vkinit::sampler_create_info(VK_FILTER_NEAREST);

	VKASSERT(vkCreateSampler(_device, &samplerInfo, nullptr, &_blockySampler));
	_deletionStack.push([=]() {
		vkDestroySampler(_device, _blockySampler, nullptr);
		});

	VkSamplerCreateInfo samplerInfo1 = vkinit::sampler_create_info(VK_FILTER_LINEAR);

	VKASSERT(vkCreateSampler(_device, &samplerInfo1, nullptr, &_linearSampler));
	_deletionStack.push([=]() {
		vkDestroySampler(_device, _linearSampler, nullptr);
		});
}

void Engine::createScene(const std::string mainModelFullPath)
{
	{ // Reset
		vkDeviceWaitIdle(_device);
		_renderables.clear();
		_models.clear();

		_sceneDisposeStack.flush();

		_meshes.clear();
		_textures.clear();
	}

	//ASSERT(loadModelFromObj("main"	, Engine::modelPath + "sponza/sponza.obj"));
	ASSERT(loadModelFromObj("main", mainModelFullPath));
	ASSERT(loadModelFromObj("sphere", Engine::modelPath + "sphere/sphere.obj"));

	//getMaterial("colored")->hasTextures = false;

	// Set materials
	for (auto& [key, model] : _models) {
		for (auto& mesh : model.meshes) {
			mesh->material = getMaterial("general");
			/*if (mesh->p_tex != nullptr) {
				mesh->material = getMaterial("diffuse_light");
			} else {
				if (model.lightAffected) {
					mesh->material = getMaterial("color_light");
				} else {
					mesh->material = getMaterial("color");
				}
			}*/
		}
	}

	Model* sphr = getModel("sphere");
	// Light source model should not be affected by light
	sphr->lightAffected = false;

	_renderContext.Init();

	for (int i = 0; i < MAX_LIGHTS; i++) {
		/*_renderables.push_back(std::make_shared<RenderObject>(
			RenderObject{
				.tag   = "light" + std::to_string(i),
				.color = glm::vec4(0.5, 0.5, 0.5, 1.),
				.model = sphr,
				.pos   = _renderContext.sceneData.lights[i].position,
				.scale = glm::vec3(0.1f)
			}
		));*/

		//_renderContext.lightObjects.push_back(_renderables.back());
    }

	_renderables.push_back(std::make_shared<RenderObject>(
		RenderObject{
			.model = getModel("main"),
			.pos = glm::vec3(0, -5.f, 0),
			.rot = glm::vec3(0, 90, 0),
			.scale = glm::vec3(15.f)
		}
	));

	_deletionStack.push([this]() {
		_sceneDisposeStack.flush();
	});
}

bool Engine::loadModelFromObj(const std::string assignedName, const std::string path)
{
	//Partially based on https://github.com/tinyobjloader/tinyobjloader/blob/release/examples/viewer/viewer.cc
	Model& newModel = _models[assignedName];
	newModel.tag = assignedName;
	
	tinyobj::attrib_t inattrib;
	std::vector<tinyobj::shape_t> inshapes;
	std::vector<tinyobj::material_t> materials;
	std::string warn, err;

	// Model bounds later used to scale model to [-1, 1] scale
	float bmin[3], bmax[3];
	bmin[0] = bmin[1] = bmin[2] = std::numeric_limits<float>::max();
	bmax[0] = bmax[1] = bmax[2] = -std::numeric_limits<float>::max();

	std::string baseDir = GetBaseDir(path);
	if (baseDir.empty()) {
		baseDir = ".";
	}
	baseDir += "/";

	pr("Loading model at: " << path);
	bool good = tinyobj::LoadObj(&inattrib, &inshapes, &materials, &warn, &err, path.c_str(), baseDir.c_str());

	if (!warn.empty()) {
		pr("\ttinyobj: WARN: " << warn);
	}
	if (!err.empty()) {
		pr("\ttinyobj: ERR: " << err);
	}
	if (!good || !err.empty()) {
		pr("\ttinyobj: Failed to load " << path);
		return false;
	}

	
	bool regen_all_normals = inattrib.normals.size() == 0;
	tinyobj::attrib_t outattrib;
	std::vector<tinyobj::shape_t> outshapes;
	if (regen_all_normals) {
		computeSmoothingShapes(inattrib, inshapes, outshapes, outattrib);
		computeAllSmoothingNormals(outattrib, outshapes);
	}

	std::vector<tinyobj::shape_t>& shapes = regen_all_normals ? outshapes : inshapes;
	tinyobj::attrib_t& attrib = regen_all_normals ? outattrib : inattrib;

	std::vector<int> mesh_tex_id;

	// Loop over shapes
	for (size_t s = 0; s < shapes.size(); s++) { // Shapes
		pr("\ttinyobj: Loading shape[" << s << "]: " << shapes[s].name);

		size_t faces_in_shape = shapes[s].mesh.num_face_vertices.size();

		int prev_mat_id = -1;
		int mat_id		= -1;

		size_t shape_submesh = 0;

		Mesh* newMesh = nullptr;
		
		// Loop over faces(polygon)
		size_t index_offset = 0;
		for (size_t f = 0; f < faces_in_shape; f++) { // Faces
			size_t vertices_in_face = shapes[s].mesh.num_face_vertices[f];
			ASSERT(vertices_in_face == 3); // triangles

			mat_id = shapes[s].mesh.material_ids[f];

			// If this face has different material, create new mesh

			if (mat_id != prev_mat_id) {
				if (newMesh != nullptr) {
					uploadMesh(*newMesh);
					newModel.meshes.push_back(newMesh);
				}

				// Pick any per-face material ID and use it as texture ID for current mesh
				mesh_tex_id.push_back(mat_id);

				std::string meshName = shapes[s].name + "_::" + std::to_string(shape_submesh);
				ASSERT(getMesh(shapes[s].name) == nullptr); // Mesh must not exist yet

				newMesh = &_meshes[meshName];
				newMesh->tag = meshName;
				newMesh->mat_id = mat_id;

				prev_mat_id = mat_id;
				++shape_submesh;
			}
			
			// Loop over vertices in the face.
			for (size_t v = 0; v < vertices_in_face; v++) { // Vertices
				
				// access to vertex
				tinyobj::index_t idx = shapes[s].mesh.indices[index_offset + v];

				//vertex position
				tinyobj::real_t vx = attrib.vertices[3 * idx.vertex_index + 0];
				tinyobj::real_t vy = attrib.vertices[3 * idx.vertex_index + 1];
				tinyobj::real_t vz = attrib.vertices[3 * idx.vertex_index + 2];

				//vertex normal
				tinyobj::real_t nx{ 0 }, ny{ 0 }, nz{ 0 };
				if (idx.normal_index >= 0) {
					nx = attrib.normals[3 * idx.normal_index + 0];
					ny = attrib.normals[3 * idx.normal_index + 1];
					nz = attrib.normals[3 * idx.normal_index + 2];
				}
				//vertex uv
				tinyobj::real_t ux{ 0 }, uy{ 0 };
				if (idx.texcoord_index >= 0) {
					ux = attrib.texcoords[2 * idx.texcoord_index + 0];
					uy = attrib.texcoords[2 * idx.texcoord_index + 1];
				}

				//copy it into our vertex
				Vertex new_vert{
					.pos = {vx, vy, vz},
					.normal = {nx, ny, nz},
					//we are setting the vertex color as the vertex normal. This is just for display purposess
					.color = {nx, ny, nz},
					.uv = {ux, 1 - uy}
				};

				ASSERT(newMesh != nullptr);
				newMesh->_vertices.push_back(new_vert);

				// Update model bounds
				bmin[v] = std::min(vx, bmin[v]);
				bmin[v] = std::min(vy, bmin[v]);
				bmin[v] = std::min(vz, bmin[v]);

				bmax[v] = std::max(vx, bmax[v]);
				bmax[v] = std::max(vy, bmax[v]);
				bmax[v] = std::max(vz, bmax[v]);
			}
			index_offset += vertices_in_face;
		}

		uploadMesh(*newMesh);
		newModel.meshes.push_back(newMesh);
	}

	// Add default material in case there are none ?
	materials.push_back(tinyobj::material_t());

	for (size_t i = 0; i < materials.size(); i++) {
		pr("\tmaterial[" << i << "].diffuse_texname = " << materials[i].diffuse_texname);
	}
	
	// Only load textures that are used by meshes
	for (size_t mesh_i = 0; mesh_i < mesh_tex_id.size(); ++mesh_i) {
		tinyobj::material_t* mp = &materials[mesh_tex_id[mesh_i]];

		// Texname empty means there's no texture
		if (mp->diffuse_texname.length() > 0) {
			//newModel.meshes[mesh_i]->hasTextures = true;

			std::string texture_filename = baseDir + mp->diffuse_texname;
			Texture* texture = nullptr;
			// Only load the texture if it is not already loaded
			if (getTexture(texture_filename) == nullptr) {
				if (!FileExists(texture_filename)) {
					PRERR("Unable to find file: " << texture_filename);
					EXIT(1);
				}

				if (!(texture = loadTextureFromFile(texture_filename))) {
					PRERR("Unable to load texture: " << texture_filename);
					EXIT(1);
				}
			} else {
				texture = getTexture(texture_filename);
			}

			// Assign texture pointer to the mesh that uses it
			newModel.meshes[mesh_i]->p_tex = texture;
		}
	}


	pr("\tbmin = " << bmin[0] << ", " << bmin[1] << ", " << bmin[2]);
	pr("\tbmax = " << bmax[0] << ", " << bmax[1] << ", " << bmax[2]);

	// Max extent is half of biggest difference of a bounds coordinate
	float maxExtent = 0.5f * (bmax[0] - bmin[0]);
	if (maxExtent < 0.5f * (bmax[1] - bmin[1])) {
		maxExtent = 0.5f * (bmax[1] - bmin[1]);
	}
	if (maxExtent < 0.5f * (bmax[2] - bmin[2])) {
		maxExtent = 0.5f * (bmax[2] - bmin[2]);
	}
	ASSERT(maxExtent > 0.f);
	
	newModel.maxExtent = maxExtent;

	return true;
}

Texture* Engine::loadTextureFromFile(const std::string path)
{
	/* Based on https://github.com/vblanco20-1/vulkan-guide */
	int texWidth, texHeight, texChannels;

	stbi_uc* pixels = stbi_load(path.c_str(), &texWidth, &texHeight, &texChannels, STBI_rgb_alpha); // STBI_default

	if (!pixels) {
		PRERR("Failed to load texture file " << path);
		return nullptr;
	}

	void* pixel_ptr = pixels;
	VkDeviceSize imageSize = texWidth * texHeight * 4;

	//the format R8G8B8A8 matches exactly with the pixels loaded from stb_image lib
	VkFormat image_format = VK_FORMAT_R8G8B8A8_SRGB;

	//allocate temporary buffer for holding texture data to upload
	AllocatedBuffer stagingBuffer = createBuffer(imageSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VMA_MEMORY_USAGE_CPU_ONLY);

	//copy data to buffer
	stagingBuffer.runOnMemoryMap(_allocator, [&](void* data) {
		memcpy(data, pixel_ptr, static_cast<size_t>(imageSize));
	});


	//we no longer need the loaded data, so we can free the pixels as they are now in the staging buffer
	stbi_image_free(pixels);

	VkExtent3D imageExtent{
		.width = static_cast<uint32_t>(texWidth),
		.height = static_cast<uint32_t>(texHeight),
		.depth = 1
	};

	VkImageCreateInfo dimg_info = 
		vkinit::image_create_info(image_format, 
			VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT, imageExtent);

	
	// This creates entry in cache
	Texture& newTexture = _textures[path];
	newTexture.tag = path;

	VmaAllocationCreateInfo dimg_allocinfo = {};
	dimg_allocinfo.usage = VMA_MEMORY_USAGE_GPU_ONLY;

	//allocate and create the image
	vmaCreateImage(_allocator, &dimg_info, &dimg_allocinfo, &newTexture.image.image, &newTexture.image.allocation, nullptr);

	immediate_submit(
		[&](VkCommandBuffer cmd) {
			VkImageMemoryBarrier imageBarrier_toTransfer{
				.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
				.srcAccessMask = 0,
				.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT,
				.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED,
				.newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
				.image = newTexture.image.image,
				.subresourceRange = {
					.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
					.baseMipLevel = 0,
					.levelCount = 1,
					.baseArrayLayer = 0,
					.layerCount = 1
				}
			};

			//barrier the image into the transfer-receive layout
			vkCmdPipelineBarrier(cmd,
				VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT,
				0, 0, nullptr, 0, nullptr, 1, &imageBarrier_toTransfer);

			VkBufferImageCopy copyRegion{
				.bufferOffset = 0,
				.bufferRowLength = 0,
				.bufferImageHeight = 0,
				.imageSubresource = {
					.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
					.mipLevel = 0,
					.baseArrayLayer = 0,
					.layerCount = 1,
				},
				.imageExtent = imageExtent
			};

			//copy the buffer into the image
			vkCmdCopyBufferToImage(cmd, stagingBuffer.buffer, newTexture.image.image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &copyRegion);

			VkImageMemoryBarrier imageBarrier_toReadable = imageBarrier_toTransfer;

			imageBarrier_toReadable.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
			imageBarrier_toReadable.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

			imageBarrier_toReadable.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
			imageBarrier_toReadable.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

			//barrier the image into the shader readable layout
			vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0, 0, nullptr, 0, nullptr, 1, &imageBarrier_toReadable);
		}
	);

	VkImageViewCreateInfo imageinfo = 
		vkinit::imageview_create_info(VK_FORMAT_R8G8B8A8_SRGB, newTexture.image.image, VK_IMAGE_ASPECT_COLOR_BIT);

	VKASSERT(vkCreateImageView(_device, &imageinfo, nullptr, &newTexture.imageView));

	_sceneDisposeStack.push([=]() mutable {
		vkDestroyImageView(_device, newTexture.imageView, nullptr);
		newTexture.image.destroy(_allocator);
		});

	stagingBuffer.destroy(_allocator);
	pr("Texture loaded successfully: " << path);

	return &_textures[path];
}

void Engine::uploadMesh(Mesh& mesh)
{
	//mesh.initBuffers(_allocator);
	const size_t bufferSize = mesh.getBufferSize();

	VkBufferCreateInfo stagingBufferInfo = {
		.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
		.size = sizeof(Vertex) * mesh._vertices.size(),
		.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
		.sharingMode = VK_SHARING_MODE_EXCLUSIVE,
	};

	VmaAllocationCreateInfo allocInfo = {
		.usage = VMA_MEMORY_USAGE_CPU_ONLY,
	};
	//allocate temporary buffer for holding texture data to upload

	AllocatedBuffer stagingBuffer;
	VKASSERT(vmaCreateBuffer(_allocator, &stagingBufferInfo, &allocInfo,
		&stagingBuffer.buffer, &stagingBuffer.allocation, nullptr));

	void* data;
	vmaMapMemory(_allocator, stagingBuffer.allocation, &data);
	memcpy(data, mesh._vertices.data(), mesh._vertices.size() * sizeof(Vertex));
	vmaUnmapMemory(_allocator, stagingBuffer.allocation);

	//allocate vertex buffer
	VkBufferCreateInfo vertexBufferInfo = {
		.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
		//this is the total size, in bytes, of the buffer we are allocating
		.size = mesh.getBufferSize(),
		//this buffer is going to be used as a Vertex Buffer
		.usage = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT
	};

	//let the VMA library know that this data should be GPU native
	VmaAllocationCreateInfo vmaAllocInfo = {
		.usage = VMA_MEMORY_USAGE_GPU_ONLY,
	};

	//allocate the buffer
	VKASSERT(vmaCreateBuffer(_allocator, &vertexBufferInfo, &vmaAllocInfo,
		&mesh._vertexBuffer.buffer,
		&mesh._vertexBuffer.allocation,
		nullptr));

	immediate_submit([&](VkCommandBuffer cmd) {
		VkBufferCopy copy;
		copy.dstOffset = 0;
		copy.srcOffset = 0;
		copy.size = mesh.getBufferSize();
		vkCmdCopyBuffer(cmd, stagingBuffer.buffer, mesh._vertexBuffer.buffer, 1, &copy);
		});

	_sceneDisposeStack.push([&]() {
		mesh._vertexBuffer.destroy(_allocator);
		});

	// Destroy staging buffer now as we don't need it anymore
	stagingBuffer.destroy(_allocator);
}



void RenderContext::UpdateLightPosition(int lightIndex, glm::vec3 newPos)
{
	sceneData.lights[lightIndex].position = newPos;
	lightObjects[lightIndex]->pos = newPos;
}

int RenderContext::GetClosestRadiusIndex(int radius) {
	int min_diff = std::numeric_limits<int>::max();

	int closest_index = 0;
	int i = 0;
	for (auto& [key, value] : atten_map) {
		int diff = std::abs(key - radius);
		if (diff < min_diff) {
			min_diff = diff;
			closest_index = i;
		}
		++i;
	}

	return closest_index;
}

void RenderContext::UpdateLightAttenuation(int lightIndex, int mode)
{
	Light& l = sceneData.lights[lightIndex];

	// Set the radius to the closest radius that is present in table
	// Find the attenuation values that correspond to the radius
	int ind = GetClosestRadiusIndex(l.radius);
	if (mode == 1) { // Increase
		if (ind != atten_map.size() - 1) {
			++ind;
		}
	} else if (mode == 2) { // Decrease
		if (ind > 0) {
			--ind;
		}
	}
	l.radius = atten_map[ind].first;
	pr("Light radius[" << lightIndex << "] set to: " << l.radius << " units");

	glm::vec3 att = atten_map[ind].second;

	l.constant = att.x;
	l.linear = att.y;
	l.quadratic = att.z;
}

void RenderContext::Init()
{
	float off = 5.f;
	std::vector<glm::vec3> lightPos = {
		{ off, 0., off },
		{ off, 0., -off },
		{ -off, 0., off },
		{ -off, 0., -off },
	};

	std::vector<glm::vec3> lightColor = {
		{ 1.f , 1.f , 1.f },
		{ 1.f , 1.f , 1.f },
		{ 1.f , 1.f , 1.f },
		{ 1.f , 1.f , 1.f }
	};

	std::vector<float> radius = { 20.f, 10.f, 30.f, 5.f };
	std::vector<float> intensity = { 2000.f, 100.f, 30.f, 500.f };
	//std::vector<float> intensity = { 1.f, 1.f, 1.f, 1.f };

	float amb = 0.2f;
	sceneData.ambientColor = glm::vec4(amb, amb, amb, 1.f);

	for (int i = 0; i < MAX_LIGHTS; i++) {
		sceneData.lights[i] = {
			.position = lightPos[i],
			.radius = radius[i],

			.color = lightColor[i],

			.ambientFactor = 0.1f,
			.diffuseFactor = 1.0f,
			.specularFactor = 0.5f,
			.intensity = intensity[i],

			//Attenuation skipped. Will be updated based on radius

			.enabled = true
		};

		UpdateLightAttenuation(i, 0);
	}
}



Material* Engine::createMaterial(VkPipeline pipeline, VkPipelineLayout layout, const std::string& name)
{
	auto& mat = _materials[name] = {
		.pipeline = pipeline,
		.pipelineLayout = layout
	};

	_deletionStack.push([&]() { mat.cleanup(_device); });

	return &_materials[name];
}

Material* Engine::getMaterial(const std::string& name)
{
	//search for the object, and return nullptr if not found
	auto it = _materials.find(name);
	if (it == _materials.end()) {
		return nullptr;
	} else {
		return &(*it).second;
	}
}

Mesh* Engine::getMesh(const std::string& name)
{
	auto it = _meshes.find(name);
	if (it == _meshes.end()) {
		return nullptr;
	} else {
		return &(*it).second;
	}
}

Texture* Engine::getTexture(const std::string& name)
{
	auto it = _textures.find(name);
	if (it == _textures.end()) {
		return nullptr;
	} else {
		return &(*it).second;
	}
}

Model* Engine::getModel(const std::string& name)
{
	auto it = _models.find(name);
	if (it == _models.end()) {
		return nullptr;
	} else {
		return &(*it).second;
	}
}



void Engine::bindPipeline(VkCommandBuffer commandBuffer, VkPipeline pipeline)
{
	vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline);
	VkViewport viewport{
		.x = 0.0f,
		.y = 0.0f,
		.width = (float)_windowExtent.width,
		.height = (float)_windowExtent.height,
		.minDepth = 0.0f,
		.maxDepth = 1.0f
	};

	vkCmdSetViewport(commandBuffer, 0, 1, &viewport);

	VkRect2D scissor{
		.offset = { 0, 0 },
		.extent = _windowExtent
	};

	vkCmdSetScissor(commandBuffer, 0, 1, &scissor);
}



glm::mat4 RenderObject::Transform() {
	ASSERT(model != nullptr);
	glm::vec3 scales = scale;
	if (model != nullptr) {
		scales *= glm::vec3(1.f / model->maxExtent);
	}
	return
		glm::translate(glm::mat4(1.f), pos)	   *
		glm::rotate(glm::radians(rot.x), glm::vec3(1, 0, 0)) *
		glm::rotate(glm::radians(rot.y), glm::vec3(0, 1, 0)) *
		glm::rotate(glm::radians(rot.z), glm::vec3(0, 0, 1)) *
		glm::scale(glm::mat4(1.f), scales);
}



VertexInputDescription Vertex::getDescription()
{
	VertexInputDescription description;

	VkVertexInputBindingDescription bindingDescription = {
		.binding = 0,
		.stride = sizeof(Vertex),
		.inputRate = VK_VERTEX_INPUT_RATE_VERTEX,
	};

	description.bindings.push_back(bindingDescription);

	VkVertexInputAttributeDescription positionAttribute = {
		.location = 0,
		.binding = 0,
		.format = VK_FORMAT_R32G32B32_SFLOAT,
		.offset = offsetof(Vertex, pos),
	};

	VkVertexInputAttributeDescription normalAttribute = {
		.location = 1,
		.binding = 0,
		.format = VK_FORMAT_R32G32B32_SFLOAT,
		.offset = offsetof(Vertex, normal),
	};

	VkVertexInputAttributeDescription colorAttribute = {
		.location = 2,
		.binding = 0,
		.format = VK_FORMAT_R32G32B32_SFLOAT,
		.offset = offsetof(Vertex, color),
	};

	VkVertexInputAttributeDescription uvAttribute = {
		.location = 3,
		.binding = 0,
		.format = VK_FORMAT_R32G32_SFLOAT,
		.offset = offsetof(Vertex, uv)
	};

	description.attributes.push_back(positionAttribute);
	description.attributes.push_back(normalAttribute);
	description.attributes.push_back(colorAttribute);
	description.attributes.push_back(uvAttribute);

	return description;
}
