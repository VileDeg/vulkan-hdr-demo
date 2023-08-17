#include "stdafx.h"

#include "engine.h"

#define TINYOBJLOADER_IMPLEMENTATION
#include "tinyobj/tiny_obj_loader.h"

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
			newModel.meshes[mesh_i]->diffuseTex->globalIndex = modelLoaderGlobalDiffuseTexIndex;
			//newModel.meshes[mesh_i]->diffuseTex->type = Attachment::Type::Diffuse;
			_diffTexInsertionOrdered.push_back(newModel.meshes[mesh_i]->diffuseTex);

			++modelLoaderGlobalDiffuseTexIndex;
		}

		// Texname empty means there's no texture
		if (mp->bump_texname.length() > 0) {
			loadModelTexture(baseDir, mp->bump_texname, &newModel.meshes[mesh_i]->bumpTex);
			newModel.meshes[mesh_i]->bumpTex->globalIndex = modelLoaderGlobalBumpTexIndex;
			//newModel.meshes[mesh_i]->bumpTex->type = Attachment::Type::Bump;
			_bumpTexInsertionOrdered.push_back(newModel.meshes[mesh_i]->bumpTex);

			++modelLoaderGlobalBumpTexIndex;
		}

		// Set material lighting colors for use in Phong lighting model
		newModel.meshes[mesh_i]->gpuMat = {
			.ambientColor = glm::make_vec3(mp->ambient),
			.diffuseColor = glm::make_vec3(mp->diffuse),
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
