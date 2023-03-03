#include "stdafx.h"
#include "Enigne.h"

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

void Engine::drawObjects(VkCommandBuffer cmd, const std::vector<RenderObject>& objects)
{
	glm::mat4 projMat = glm::perspective(glm::radians(45.f), _windowExtent.width / (float)_windowExtent.height, 0.01f, 200.f);
	projMat[1][1] *= -1; //Flip y-axis

	Mesh* lastMesh = nullptr;
	Material* lastMaterial = nullptr;
	for (int i = 0; i < objects.size(); i++) {
        const RenderObject& obj = objects[i];
		if (obj.material != lastMaterial) {
			bindPipeline(cmd, obj.material->pipeline);
			lastMaterial = obj.material;
		}

		if (obj.mesh != lastMesh) {
			VkDeviceSize zeroOffset = 0;
			vkCmdBindVertexBuffers(cmd, 0, 1, &obj.mesh->_vertexBuffer.buffer, &zeroOffset);
			lastMesh = obj.mesh;
		}
		
        glm::mat4 modelMat = obj.transform;
        glm::mat4 mvp = projMat * _camera.GetViewMat() * modelMat;
		MeshPushConstants pushConsts{
			.render_matrix = mvp
		};

        vkCmdPushConstants(cmd, obj.material->pipelineLayout, VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(MeshPushConstants), &pushConsts);
		
        vkCmdDraw(cmd, obj.mesh->_vertices.size(), 1, 0, 0);
    }
}

void Engine::createScene()
{
	//Rotate to face the camera
	glm::mat4 modelMat = glm::rotate(glm::mat4(1.f), glm::radians(180.f), glm::vec3(0, 1, 0));
	RenderObject model{
		.mesh = getMesh("model"),
        .material = getMaterial("default"),
        .transform = modelMat
    };

	_renderables.push_back(model);
	for (int x = -20; x < 20; x++) {
		for (int y = -20; y < 20; y++) {
			glm::mat4 modelMat = glm::mat4(1.f);
			modelMat = glm::translate(glm::mat4(1.0f), glm::vec3(x, 0.0f, y));
			float sf = 0.2f;
			modelMat = glm::scale(modelMat, glm::vec3(sf, sf, sf));
			RenderObject tri{
                .mesh = getMesh("triangle"),
                .material = getMaterial("default"),
                .transform = modelMat
            };
            _renderables.push_back(tri);
        }
    }
}