#include "stdafx.h"

#include "engine.h"

#define TINYOBJLOADER_IMPLEMENTATION
#include "tinyobj/tiny_obj_loader.h"

#define STB_IMAGE_IMPLEMENTATION
#include "stb/stb_image.h"

#include "json/json.hpp"


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
		subs = subs.substr(slashp, subs.length() - 1 - slashp);
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


void Engine::loadCubemap(const char* cubemapDirName, bool isHDR)
{
	// Partially based on Sascha Willems' cubemap demo: 
	// https://github.com/SaschaWillems/Vulkan/blob/master/examples/texturecubemap/texturecubemap.cpp

	ASSERT(loadModelFromObj("cube", Engine::MODEL_PATH + "cube/cube.obj"));

	constexpr const char* suff[6] = { "XP", "XN", "YP", "YN", "ZP", "ZN" };

	std::string basePath = Engine::IMAGE_PATH + cubemapDirName;

	VkDeviceSize imageSize;
	AllocatedBuffer stagingBuffer;

	int baseTexW, baseTexH, baseTexChannels;

	VkFormat imageFormat;
	VkDeviceSize bufferOffset = 0;

	for (int i = 0; i < 6; ++i) {
		std::string path = basePath + "/" + suff[i] + (isHDR ? ".hdr" : ".jpg");

		void* pixel_ptr = nullptr;
		int texW, texH, texChannels;
		if (isHDR) {
			ASSERT(stbi_is_hdr(path.c_str()) != 0);
			float* fpix = stbi_loadf(path.c_str(), &texW, &texH, &texChannels, STBI_rgb_alpha);
			pixel_ptr = fpix;

			imageFormat = VK_FORMAT_R32G32B32A32_SFLOAT;
		} else {
			stbi_uc* pix = stbi_load(path.c_str(), &texW, &texH, &texChannels, STBI_rgb_alpha);
			pixel_ptr = pix;

			imageFormat = VK_FORMAT_R8G8B8A8_UNORM;
		}

		ASSERT_MSG(pixel_ptr != nullptr, "Failed to load cubemap face " + std::to_string(i) + ", path: " + path);
		if (i == 0) {
			baseTexW = texW;
			baseTexH = texH;
			baseTexChannels = texChannels;

			imageSize = (VkDeviceSize)baseTexW * (VkDeviceSize)baseTexH * 4 * (isHDR ? 4 : 1);

			VkDeviceSize bufferSize = imageSize * 6;
			// Allocate temporary buffer for holding texture data to upload
			stagingBuffer = allocateBuffer(bufferSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VMA_MEMORY_USAGE_CPU_ONLY);
		} else {
			// Make sure all the faces of cubemap have exactly the same dimensions and color channels
			ASSERT(baseTexW == texW && baseTexH == texH && baseTexChannels == texChannels);
		}



		// Copy data to buffer
		void* dst = (char*)stagingBuffer.gpu_ptr + bufferOffset;
		memcpy(dst, pixel_ptr, static_cast<size_t>(imageSize));
	
		// We no longer need the loaded data, so we can free the pixels as they are now in the staging buffer
		stbi_image_free(pixel_ptr);

		bufferOffset += imageSize;
	}

	VkImageSubresourceRange subresourceRange = {
				.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
				.baseMipLevel = 0,
				.levelCount = 1,
				.baseArrayLayer = 0,
				.layerCount = 6
	};

	// Create optimal tiled target image
	VkImageCreateInfo imageInfo{
		.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
		// This flag is required for cube map images
		.flags = VK_IMAGE_CREATE_CUBE_COMPATIBLE_BIT,
		.imageType = VK_IMAGE_TYPE_2D,
		.format = imageFormat,
		.extent = {
			.width = static_cast<uint32_t>(baseTexW),
			.height = static_cast<uint32_t>(baseTexH),
			.depth = 1
		},
		.mipLevels = 1,
		// Cube faces count as array layers in Vulkan
		.arrayLayers = 6,
		.samples = VK_SAMPLE_COUNT_1_BIT,
		.tiling = VK_IMAGE_TILING_OPTIMAL,
		.usage = VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
		.sharingMode = VK_SHARING_MODE_EXCLUSIVE,
		.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED
	};

	// This creates entry in cache
	Attachment& newTexture = _textures[basePath];
	newTexture.tag = basePath;

	VmaAllocationCreateInfo dimg_allocinfo = {};
	dimg_allocinfo.usage = VMA_MEMORY_USAGE_GPU_ONLY;

	//allocate and create the image
	VK_ASSERT(vmaCreateImage(_allocator, &imageInfo, &dimg_allocinfo, &newTexture.allocImage.image, &newTexture.allocImage.allocation, nullptr));

	immediate_submit(
		[&](VkCommandBuffer cmd) {
			// Setup buffer copy regions for each face including all of its miplevels
			std::vector<VkBufferImageCopy> bufferCopyRegions;
			VkDeviceSize offset = 0;

			for (uint32_t face = 0; face < 6; face++)
			{
				// Calculate offset into staging buffer for the current mip level and face
				VkBufferImageCopy bufferCopyRegion = {
					.bufferOffset = offset,
					.imageSubresource = {
						.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
						.mipLevel = 0,
						.baseArrayLayer = face,
						.layerCount = 1,
					},
					.imageExtent = {
						.width = static_cast<uint32_t>(baseTexW),
						.height = static_cast<uint32_t>(baseTexH),
						.depth = 1,
					}
				};

				bufferCopyRegions.push_back(bufferCopyRegion);
				offset += imageSize;
			}

			vk_utils::imageMemoryBarrier(cmd, newTexture.allocImage.image,
				0, 
				VK_ACCESS_TRANSFER_WRITE_BIT,

				VK_IMAGE_LAYOUT_UNDEFINED,
				VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,

				VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, 
				VK_PIPELINE_STAGE_TRANSFER_BIT,
				subresourceRange);

			//copy the buffer into the image
			vkCmdCopyBufferToImage(cmd, stagingBuffer.buffer, newTexture.allocImage.image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
				static_cast<uint32_t>(bufferCopyRegions.size()), bufferCopyRegions.data());

			vk_utils::imageMemoryBarrier(cmd, newTexture.allocImage.image,
				VK_ACCESS_TRANSFER_WRITE_BIT, 
				VK_ACCESS_SHADER_READ_BIT,

				VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
				VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,

				VK_PIPELINE_STAGE_TRANSFER_BIT,
				VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
				subresourceRange);
		}
	);

	// Create image view
	VkImageViewCreateInfo imageinfo = {
		.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
		.image = newTexture.allocImage.image,
		.viewType = VK_IMAGE_VIEW_TYPE_CUBE,
		.format = imageFormat,
		.subresourceRange = subresourceRange,
	};

	VK_ASSERT(vkCreateImageView(_device, &imageinfo, nullptr, &newTexture.view));

	_sceneDisposeStack.push([=]() mutable {
		vkDestroyImageView(_device, newTexture.view, nullptr);
		vmaDestroyImage(_allocator, newTexture.allocImage.image, newTexture.allocImage.allocation);
	});

	stagingBuffer.destroy(_allocator);
	pr("Cubemap loaded successfully: " << basePath);

	// write cubemap to descriptor set
	newTexture.allocImage.descInfo = {
		.sampler = _linearSampler,//cubemapSampler,
		.imageView = newTexture.view,
		.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
	};

	_skyboxAllocImage = newTexture.allocImage;

	for (int i = 0; i < MAX_FRAMES_IN_FLIGHT; ++i) {
		VkWriteDescriptorSet skyboxWrite =
			vkinit::write_descriptor_image(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 
				_frames[i].globalSet, &newTexture.allocImage.descInfo, 3);
		vkUpdateDescriptorSets(_device, 1, &skyboxWrite, 0, nullptr);
	}
}

bool Engine::loadModelFromObj(const std::string assignedName, const std::string path)
{
	// Partially based on https://github.com/tinyobjloader/tinyobjloader/blob/release/examples/viewer/viewer.cc

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


	std::unordered_map<Mesh*, std::unordered_map<Vertex, uint32_t>> meshVertexMap;

	// Loop over shapes
	for (size_t s = 0; s < shapes.size(); s++) { // Shapes
		pr("\ttinyobj: Loading shape[" << s << "]: " << shapes[s].name);

		size_t faces_in_shape = shapes[s].mesh.num_face_vertices.size();

		int prev_mat_id = -1;
		int mat_id = -1;

		Mesh* currentMesh = nullptr;

		ASSERT(faces_in_shape > 0);

		// Loop over faces(polygon)
		size_t index_offset = 0;
		for (size_t f = 0; f < faces_in_shape; f++) { // Faces
			size_t vertices_in_face = shapes[s].mesh.num_face_vertices[f];
			ASSERT(vertices_in_face == 3); // triangles

			mat_id = shapes[s].mesh.material_ids[f];


			// If this face has different material, 
			// check if a mesh with such material was already created
			if (mat_id != prev_mat_id) {
				bool mesh_already_created = false; //std::find(mesh_mat_ids.begin(), mesh_mat_ids.end(), mat_id) != mesh_mat_ids.end();
				for (auto& [mesh, uniqV] : meshVertexMap) {
					if (mesh->mat_id == mat_id) {
						// Mesh with such material found!
						currentMesh = mesh;
						mesh_already_created = true;
					}
				}

				if (!mesh_already_created) {
					std::string meshName = "MESH_MAT: " + materials[mat_id].name; // + "_::" + std::to_string(shape_submesh);
					ASSERT(getMesh(meshName) == nullptr); // Mesh must not exist yet

					// Create new mesh
					currentMesh = &_meshes[meshName];
					currentMesh->tag = meshName;
					currentMesh->mat_id = mat_id;

					// Add new mesh and create uniqueVertices map for it
					meshVertexMap[currentMesh] = {};
				}

				prev_mat_id = mat_id;
			}

			auto& uniqVert = meshVertexMap[currentMesh];

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



				ASSERT(currentMesh != nullptr);
				// Add vertex only if it wasn't already added
				if (uniqVert.count(new_vert) == 0) {
					uniqVert[new_vert] = static_cast<uint32_t>(currentMesh->vertices.size());
					currentMesh->vertices.push_back(new_vert);
				}
				// Add vertex index
				currentMesh->indices.push_back(uniqVert[new_vert]);
				//currentMesh->vertices.push_back(new_vert);

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
	}

	for (auto& [mesh, uniqV] : meshVertexMap) {
		ASSERT(mesh != nullptr);

		uploadMesh(*mesh);
		newModel.meshes.push_back(mesh);
	}

	// Lambda for convenience
	auto loadModelTexture = [this](std::string baseDir, std::string texName, Attachment** dst) {
		std::string texture_filename = baseDir + texName;
		Attachment* texture = nullptr;
		// Only load the texture if it is not already loaded
		if (getTexture(texture_filename) == nullptr) {
			if (!FileExists(texture_filename)) {
				PRERR("Unable to find file: " << texture_filename);
				EXIT(1);
			}

			if (!(texture = loadTextureFromFile(texture_filename.c_str()))) {
				PRERR("Unable to load texture: " << texture_filename);
				EXIT(1);
			}
		} else {
			texture = getTexture(texture_filename);
		}

		// Assign texture pointer to the mesh that uses it
		//newModel.meshes[mesh_i]->diffuseTex = texture;
		*dst = texture;
	};

	// Only load textures that are used by meshes
	size_t mesh_i = 0;
	for (auto& [mesh, uniqV] : meshVertexMap) {
		tinyobj::material_t* mp = &materials[mesh->mat_id];

		// Texname empty means there's no texture
		if (mp->diffuse_texname.length() > 0) {
			loadModelTexture(baseDir, mp->diffuse_texname, &newModel.meshes[mesh_i]->diffuseTex);
		}

		// Texname empty means there's no texture
		if (mp->bump_texname.length() > 0) {
			loadModelTexture(baseDir, mp->bump_texname, &newModel.meshes[mesh_i]->bumpTex);
		}

		// Set material lighting colors for use in Phong lighting model
		newModel.meshes[mesh_i]->gpuMat = {
			.ambientColor  = glm::make_vec3(mp->ambient),
			.diffuseColor  = glm::make_vec3(mp->diffuse),
			.specularColor = glm::make_vec3(mp->specular)
		};

		++mesh_i;
	}

	/*pr("\tbmin = " << bmin[0] << ", " << bmin[1] << ", " << bmin[2]);
	pr("\tbmax = " << bmax[0] << ", " << bmax[1] << ", " << bmax[2]);*/

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



Attachment* Engine::loadTextureFromFile(const char* path)
{
	/* Based on https://github.com/vblanco20-1/vulkan-guide */

	int texWidth, texHeight, texChannels;
	stbi_uc* pixels = stbi_load(path, &texWidth, &texHeight, &texChannels, STBI_rgb_alpha);
	if (!pixels) {
		PRERR("Failed to load texture file " << path);
		return nullptr;
	}

	void* pixel_ptr = pixels;
	VkDeviceSize imageSize = texWidth * texHeight * 4;

	// The format R8G8B8A8 matches exactly with the pixels loaded from stb_image lib
	VkFormat imageFormat = VK_FORMAT_R8G8B8A8_SRGB;

	// Allocate temporary buffer for holding texture data to upload
	AllocatedBuffer stagingBuffer = allocateBuffer(imageSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VMA_MEMORY_USAGE_CPU_ONLY);

	// Copy data to buffer
	memcpy(stagingBuffer.gpu_ptr, pixel_ptr, static_cast<size_t>(imageSize));
	
	// We no longer need the loaded data, so we can free the pixels as they are now in the staging buffer
	stbi_image_free(pixels);

	VkExtent3D imageExtent{
		.width = static_cast<uint32_t>(texWidth),
		.height = static_cast<uint32_t>(texHeight),
		.depth = 1
	};

	VkImageSubresourceRange subresourceRange = {
		.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
		.baseMipLevel = 0,
		.levelCount = 1,
		.baseArrayLayer = 0,
		.layerCount = 1
	};

	VkImageCreateInfo dimg_info =
		vkinit::image_create_info(imageFormat,
			VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT, imageExtent);

	// This creates entry in cache
	Attachment& newTexture = _textures[path];
	newTexture.tag = path;

	VmaAllocationCreateInfo dimg_allocinfo = {};
	dimg_allocinfo.usage = VMA_MEMORY_USAGE_GPU_ONLY;

	//allocate and create the image
	vmaCreateImage(_allocator, &dimg_info, &dimg_allocinfo, &newTexture.allocImage.image, &newTexture.allocImage.allocation, nullptr);

	immediate_submit(
		[&](VkCommandBuffer cmd) {
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

			vk_utils::imageMemoryBarrier(cmd, newTexture.allocImage.image,
				0,
				VK_ACCESS_TRANSFER_WRITE_BIT,

				VK_IMAGE_LAYOUT_UNDEFINED,
				VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,

				VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
				VK_PIPELINE_STAGE_TRANSFER_BIT,
				subresourceRange);

			//copy the buffer into the image
			vkCmdCopyBufferToImage(cmd, stagingBuffer.buffer, newTexture.allocImage.image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &copyRegion);

			vk_utils::imageMemoryBarrier(cmd, newTexture.allocImage.image,
				VK_ACCESS_TRANSFER_WRITE_BIT,
				VK_ACCESS_SHADER_READ_BIT,

				VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
				VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,

				VK_PIPELINE_STAGE_TRANSFER_BIT,
				VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
				subresourceRange);
		}
	);

	VkImageViewCreateInfo imageinfo =
		vkinit::imageview_create_info(VK_FORMAT_R8G8B8A8_SRGB, newTexture.allocImage.image, VK_IMAGE_ASPECT_COLOR_BIT);

	VK_ASSERT(vkCreateImageView(_device, &imageinfo, nullptr, &newTexture.view));

	_sceneDisposeStack.push([=]() mutable {
		vkDestroyImageView(_device, newTexture.view, nullptr);
		vmaDestroyImage(_allocator, newTexture.allocImage.image, newTexture.allocImage.allocation);
		});

	stagingBuffer.destroy(_allocator);
	pr("Texture loaded successfully: " << path);

	return &_textures[path];


}

void Engine::createMeshBuffer(Mesh& mesh, bool isVertexBuffer)
{
	ASSERT(isVertexBuffer && mesh.vertices.size() > 0 || !isVertexBuffer && mesh.indices.size() > 0);

	const size_t bufferSize = isVertexBuffer ?
		mesh.vertices.size() * sizeof(Vertex) :
		mesh.indices.size() * sizeof(uint32_t);

	// Allocate temporary buffer for holding texture data to upload
	AllocatedBuffer stagingBuffer = allocateBuffer(bufferSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VMA_MEMORY_USAGE_CPU_ONLY);

	if (isVertexBuffer) {
		memcpy(stagingBuffer.gpu_ptr, mesh.vertices.data(), mesh.vertices.size() * sizeof(Vertex));
	} else {
		memcpy(stagingBuffer.gpu_ptr, mesh.indices.data(), mesh.indices.size() * sizeof(uint32_t));
	}

	VkBufferUsageFlags usg = (isVertexBuffer ? VK_BUFFER_USAGE_VERTEX_BUFFER_BIT : VK_BUFFER_USAGE_INDEX_BUFFER_BIT);

	AllocatedBuffer& allocBuffer = isVertexBuffer ? mesh.vertexBuffer : mesh.indexBuffer;

	allocBuffer = allocateBuffer(bufferSize, usg | VK_BUFFER_USAGE_TRANSFER_DST_BIT, VMA_MEMORY_USAGE_GPU_ONLY);

	immediate_submit([&](VkCommandBuffer cmd) {
		VkBufferCopy copy;
		copy.dstOffset = 0;
		copy.srcOffset = 0;
		copy.size = bufferSize;
		vkCmdCopyBuffer(cmd, stagingBuffer.buffer, allocBuffer.buffer, 1, &copy);
		});

	_sceneDisposeStack.push([&]() {
		allocBuffer.destroy(_allocator);
		});

	// Destroy staging buffer now as we don't need it anymore
	stagingBuffer.destroy(_allocator);
}

void Engine::uploadMesh(Mesh& mesh)
{
	// Create vertex buffer
	createMeshBuffer(mesh, true);
	// Create index buffer
	createMeshBuffer(mesh, false);
}

void Engine::loadScene(std::string fullScenePath)
{
	using namespace nlohmann;

	std::string scene_name = "dobrovic-sponza";
	std::ifstream in(fullScenePath);
	ASSERT(in.good());

	json j;
	j << in;

	CreateSceneData data = {
		.bumpStrength = j["bump_strength"],
		.modelPath = j["model_name"],
		.skyboxPath = j["skybox"]
	};

	for (int i = 0; i < MAX_LIGHTS; ++i) {
		auto l = j["lights"];
		data.intensity[i] = l[i]["intensity"];
		auto p = l[i]["position"];
		data.position[i].x = p[0];
		data.position[i].y = p[1];
		data.position[i].z = p[2];
	}

	createScene(data);
}

void Engine::saveScene(std::string fullScenePath)
{
	using namespace nlohmann;

	auto& rc = _renderContext;
	auto& sd = rc.sceneData;

	json j;
	j["model_name"] = rc.modelName;
	j["bump_strength"] = sd.bumpStrength;

	auto arr = json::array();

	for (int i = 0; i < MAX_LIGHTS; ++i) {
		auto& l = sd.lights[i];
		auto& p = l.position;

		auto arr1 = json::object();
		arr1["position"] = { p.x, p.y, p.z };
		arr1["intensity"] = l.intensity;

		arr.push_back(arr1);
	}

	j["lights"] = arr;
	j["skybox"] = rc.skyboxName;

	std::ofstream out(fullScenePath);
	ASSERT(out.good());

	out << std::setw(2) << j;
}



void Engine::createSamplers() {
	// Create samplers for textures
	VkSamplerCreateInfo samplerInfo1 = vkinit::sampler_create_info(VK_FILTER_LINEAR);

	VK_ASSERT(vkCreateSampler(_device, &samplerInfo1, nullptr, &_linearSampler));
	_deletionStack.push([=]() {
		vkDestroySampler(_device, _linearSampler, nullptr);
		});
}

void Engine::createScene(CreateSceneData data)
{
	{ // Reset for when we load a scene at runtime
		vkDeviceWaitIdle(_device);

		_renderables.clear();
		_renderContext.lightObjects.clear();
		_models.clear();

		_sceneDisposeStack.flush();

		_meshes.clear();
		_textures.clear();
	}

	// Main model of the scene
	ASSERT(loadModelFromObj("main", Engine::MODEL_PATH + data.modelPath));

	// Sphere model of the light source
	ASSERT(loadModelFromObj("sphere", Engine::MODEL_PATH + "sphere/sphere.obj"));

	// Set materials
	for (auto& [key, model] : _models) {
		for (auto& mesh : model.meshes) {
			mesh->material = getMaterial("general");
		}
	}

	loadCubemap(data.skyboxPath.c_str(), true);

	_skyboxObject = std::make_shared<RenderObject>(
		RenderObject{
			.tag = "Skybox",
			.color = {1, 0, 1, 1},
			.model = &_models["cube"],
			.isSkybox = true
		}
	);
	_skyboxObject->model->meshes[0]->material = &_materials["skybox"];

	_renderContext.Init(data);

	if (getModel("sphere")) {
		Model* sphr = getModel("sphere");
		// Light source model should not be affected by light
		sphr->lightAffected = false;
		sphr->useObjectColor = true;

		sphr->meshes[0]->gpuMat.ambientColor *= 10.f;
		sphr->meshes[0]->gpuMat.diffuseColor *= 10.f;
		sphr->meshes[0]->gpuMat.specularColor *= 10.f;

		for (int i = 0; i < MAX_LIGHTS; i++) {
			auto ptr = std::make_shared<RenderObject>(
				RenderObject{
					.tag = "light" + std::to_string(i),
					.color = glm::vec4(10, 10, 10, 1.),
					.model = sphr,
					.pos = _renderContext.sceneData.lights[i].position,
					.scale = glm::vec3(0.1f)
				}
			);

			_renderables.push_back(ptr);

			_renderContext.lightObjects.push_back(ptr);
		}
	}

	if (getModel("main")) {
		_renderables.push_back(std::make_shared<RenderObject>(
			RenderObject{
				.model = getModel("main"),
				.pos = glm::vec3(0, -5.f, 0),
				.rot = glm::vec3(0, 90, 0),
				.scale = glm::vec3(15.f)
			}
		));
	}

	_deletionStack.push([this]() {
		_sceneDisposeStack.flush();
	});
}

Material* Engine::createMaterial(VkPipeline pipeline, VkPipelineLayout layout, const std::string& name)
{
	auto& mat = _materials[name] = {
		.tag = name,
		.pipeline = pipeline,
		.pipelineLayout = layout
	};

	setDebugName(VK_OBJECT_TYPE_PIPELINE, pipeline, name);

	_deletionStack.push([&]() { 
		vkDestroyPipeline(_device, mat.pipeline, nullptr);
		vkDestroyPipelineLayout(_device, mat.pipelineLayout, nullptr);
	});

	return &_materials[name];
}
