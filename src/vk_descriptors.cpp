/*The MIT License (MIT)

Copyright (c) 2016 Patrick Marsceill

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in
all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
THE SOFTWARE.*/

/* Repository: https://github.com/vblanco20-1/vulkan-guide/tree/all-chapters */

/* Initial code was modified */

#include "stdafx.h"

#include <vk_descriptors.h>
#include <algorithm>

VkDescriptorPool createPool(VkDevice device, const DescriptorAllocator::PoolSizes& poolSizes, int count, VkDescriptorPoolCreateFlags flags)
{
	std::vector<VkDescriptorPoolSize> sizes;
	sizes.reserve(poolSizes.sizes.size());
	for (auto sz : poolSizes.sizes) {
		sizes.push_back({ sz.first, uint32_t(sz.second * count) });
	}
	VkDescriptorPoolCreateInfo pool_info = {};
	pool_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
	pool_info.flags = flags;
	pool_info.maxSets = count;
	pool_info.poolSizeCount = (uint32_t)sizes.size();
	pool_info.pPoolSizes = sizes.data();

	VkDescriptorPool descriptorPool;
	vkCreateDescriptorPool(device, &pool_info, nullptr, &descriptorPool);

	return descriptorPool;
}

void DescriptorAllocator::reset_pools()
{
	for (auto p : usedPools)
	{
		vkResetDescriptorPool(device, p, 0);
	}

	freePools = usedPools;
	usedPools.clear();
	currentPool = VK_NULL_HANDLE;
}

bool DescriptorAllocator::allocate(VkDescriptorSet* set, VkDescriptorSetLayout layout)
{
	if (currentPool == VK_NULL_HANDLE)
	{
		currentPool = grab_pool();
		usedPools.push_back(currentPool);
	}

	VkDescriptorSetAllocateInfo allocInfo = {};
	allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
	allocInfo.pNext = nullptr;

	allocInfo.pSetLayouts = &layout;
	allocInfo.descriptorPool = currentPool;
	allocInfo.descriptorSetCount = 1;


	VkResult allocResult = vkAllocateDescriptorSets(device, &allocInfo, set);
	bool needReallocate = false;

	switch (allocResult) {
	case VK_SUCCESS:
		//all good, return
		return true;

		break;
	case VK_ERROR_FRAGMENTED_POOL:
	case VK_ERROR_OUT_OF_POOL_MEMORY:
		//reallocate pool
		needReallocate = true;
		break;
	default:
		//unrecoverable error
		return false;
	}

	if (needReallocate)
	{
		//allocate a new pool and retry
		currentPool = grab_pool();
		usedPools.push_back(currentPool);
		allocInfo.descriptorPool = currentPool;

		allocResult = vkAllocateDescriptorSets(device, &allocInfo, set);

		//if it still fails then we have big issues
		if (allocResult == VK_SUCCESS)
		{
			return true;
		}
	}

	return false;
}

void DescriptorAllocator::init(VkDevice newDevice)
{
	device = newDevice;
}

void DescriptorAllocator::cleanup()
{
	//delete every pool held
	for (auto p : freePools)
	{
		vkDestroyDescriptorPool(device, p, nullptr);
	}
	for (auto p : usedPools)
	{
		vkDestroyDescriptorPool(device, p, nullptr);
	}
}

VkDescriptorPool DescriptorAllocator::grab_pool()
{
	if (freePools.size() > 0)
	{
		VkDescriptorPool pool = freePools.back();
		freePools.pop_back();
		return pool;
	} else {
		return createPool(device, descriptorSizes, 1000, 0);
	}
}


void DescriptorLayoutCache::init(VkDevice newDevice)
{
	device = newDevice;
}

VkDescriptorSetLayout DescriptorLayoutCache::create_descriptor_layout(VkDescriptorSetLayoutCreateInfo* info)
{
	DescriptorLayoutInfo layoutinfo;
	layoutinfo.bindings.reserve(info->bindingCount);
	bool isSorted = true;
	int32_t lastBinding = -1;
	for (uint32_t i = 0; i < info->bindingCount; i++) {
		layoutinfo.bindings.push_back(info->pBindings[i]);

		//check that the bindings are in strict increasing order
		if (static_cast<int32_t>(info->pBindings[i].binding) > lastBinding)
		{
			lastBinding = info->pBindings[i].binding;
		} else {
			isSorted = false;
		}
	}
	if (!isSorted)
	{
		std::sort(layoutinfo.bindings.begin(), layoutinfo.bindings.end(), [](VkDescriptorSetLayoutBinding& a, VkDescriptorSetLayoutBinding& b) {
			return a.binding < b.binding;
			});
	}

	auto it = layoutCache.find(layoutinfo);
	if (it != layoutCache.end())
	{
		return (*it).second;
	} else {
		VkDescriptorSetLayout layout;
		vkCreateDescriptorSetLayout(device, info, nullptr, &layout);

		//layoutCache.emplace()
		//add to cache
		layoutCache[layoutinfo] = layout;
		return layout;
	}
}


void DescriptorLayoutCache::cleanup()
{
	//delete every descriptor layout held
	for (auto pair : layoutCache)
	{
		vkDestroyDescriptorSetLayout(device, pair.second, nullptr);
	}
}

DescriptorBuilder DescriptorBuilder::begin(DescriptorLayoutCache* layoutCache, DescriptorAllocator* allocator)
{
	DescriptorBuilder builder;

	builder.cache = layoutCache;
	builder.alloc = allocator;
	return builder;
}


DescriptorBuilder& DescriptorBuilder::bind_buffer(uint32_t binding, VkDescriptorBufferInfo* bufferInfo, VkDescriptorType type, VkShaderStageFlags stageFlags)
{
	VkDescriptorSetLayoutBinding newBinding{};

	newBinding.descriptorCount = 1;
	newBinding.descriptorType = type;
	newBinding.pImmutableSamplers = nullptr;
	newBinding.stageFlags = stageFlags;
	newBinding.binding = binding;

	bindings.push_back(newBinding);

	if (bufferInfo != nullptr) { // Added condition to allow for later manual update
		VkWriteDescriptorSet newWrite{};
		newWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		newWrite.pNext = nullptr;

		newWrite.descriptorCount = 1;
		newWrite.descriptorType = type;
		newWrite.pBufferInfo = bufferInfo;
		newWrite.dstBinding = binding;

		writes.push_back(newWrite);
	}

	return *this;
}


DescriptorBuilder& DescriptorBuilder::bind_image_empty(uint32_t binding, uint32_t descriptorCount, VkDescriptorType type, VkShaderStageFlags stageFlags)
{
	if (descriptorCount == 0) {
		return *this;
	}

	VkDescriptorSetLayoutBinding newBinding{};

	newBinding.descriptorCount = descriptorCount;
	newBinding.descriptorType = type;
	newBinding.pImmutableSamplers = nullptr;
	newBinding.stageFlags = stageFlags;
	newBinding.binding = binding;

	bindings.push_back(newBinding);

	return *this;
}

DescriptorBuilder& DescriptorBuilder::bind_image(uint32_t binding, VkDescriptorImageInfo* imageInfo, VkDescriptorType type, VkShaderStageFlags stageFlags)
{
	VkDescriptorSetLayoutBinding newBinding{};

	newBinding.descriptorCount = 1;
	newBinding.descriptorType = type;
	newBinding.pImmutableSamplers = nullptr;
	newBinding.stageFlags = stageFlags;
	newBinding.binding = binding;

	bindings.push_back(newBinding);

	if (imageInfo != nullptr) { // Added condition to allow passing nullptr for later manual update
		VkWriteDescriptorSet newWrite{};
		newWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		newWrite.pNext = nullptr;

		newWrite.descriptorCount = 1;
		newWrite.descriptorType = type;
		newWrite.pImageInfo = imageInfo;
		newWrite.dstBinding = binding;

		writes.push_back(newWrite);
	}

	return *this;
}

DescriptorBuilder& DescriptorBuilder::bind_image_array(uint32_t binding, std::vector<VkDescriptorImageInfo>& imageInfos, VkDescriptorType type, VkShaderStageFlags stageFlags)
{
	VkDescriptorSetLayoutBinding newBinding{};

	newBinding.descriptorCount = imageInfos.size();
	newBinding.descriptorType = type;
	newBinding.pImmutableSamplers = nullptr;
	newBinding.stageFlags = stageFlags;
	newBinding.binding = binding;

	bindings.push_back(newBinding);

	if (!imageInfos.empty()) { // Added condition to allow for later manual update
		VkWriteDescriptorSet newWrite{};
		newWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		newWrite.pNext = nullptr;

		newWrite.descriptorCount = imageInfos.size();
		newWrite.descriptorType = type;
		newWrite.pImageInfo = imageInfos.data();
		newWrite.dstBinding = binding;

		writes.push_back(newWrite);
	}

	return *this;
}

bool DescriptorBuilder::build(VkDescriptorSet& set, VkDescriptorSetLayout& layout)
{
	std::vector<VkDescriptorBindingFlags> bindingFlags;
	for (auto& b : bindings) {
		bindingFlags.push_back(VK_DESCRIPTOR_BINDING_PARTIALLY_BOUND_BIT_EXT);
	}

	VkDescriptorSetLayoutBindingFlagsCreateInfo flagsInfo = {
		.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_BINDING_FLAGS_CREATE_INFO,
		.pNext = nullptr,
		.bindingCount = static_cast<uint32_t>(bindings.size()),
		.pBindingFlags = bindingFlags.data()
	};

	//build layout first
	VkDescriptorSetLayoutCreateInfo layoutInfo{};
	layoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
	layoutInfo.pNext = &flagsInfo;

	layoutInfo.pBindings = bindings.data();
	layoutInfo.bindingCount = static_cast<uint32_t>(bindings.size());

	layout = cache->create_descriptor_layout(&layoutInfo);

	//allocate descriptor
	bool success = alloc->allocate(&set, layout);
	if (!success) { return false; };

	//write descriptor

	for (VkWriteDescriptorSet& w : writes) {
		w.dstSet = set;
	}

	vkUpdateDescriptorSets(alloc->device, static_cast<uint32_t>(writes.size()), writes.data(), 0, nullptr);

	return true;
}


bool DescriptorBuilder::build(VkDescriptorSet& set)
{
	VkDescriptorSetLayout layout;
	return build(set, layout);
}


bool DescriptorLayoutCache::DescriptorLayoutInfo::operator==(const DescriptorLayoutInfo& other) const
{
	if (other.bindings.size() != bindings.size())
	{
		return false;
	} else {
		//compare each of the bindings is the same. Bindings are sorted so they will match
		for (int i = 0; i < bindings.size(); i++) {
			if (other.bindings[i].binding != bindings[i].binding)
			{
				return false;
			}
			if (other.bindings[i].descriptorType != bindings[i].descriptorType)
			{
				return false;
			}
			if (other.bindings[i].descriptorCount != bindings[i].descriptorCount)
			{
				return false;
			}
			if (other.bindings[i].stageFlags != bindings[i].stageFlags)
			{
				return false;
			}
		}
		return true;
	}
}

size_t DescriptorLayoutCache::DescriptorLayoutInfo::hash() const
{
	using std::size_t;
	using std::hash;

	size_t result = hash<size_t>()(bindings.size());

	for (const VkDescriptorSetLayoutBinding& b : bindings)
	{
		//pack the binding data into a single int64. Not fully correct but its ok
		size_t binding_hash = b.binding | b.descriptorType << 8 | b.descriptorCount << 16 | b.stageFlags << 24;

		//shuffle the packed binding data and xor it with the main hash
		result ^= hash<size_t>()(binding_hash);
	}

	return result;
}
