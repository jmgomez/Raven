/**
 * HUI - render/vulkan/vk_pipeline.c
 * Shaders, samplers, descriptor sets, render passes and pipelines.
 *
 * The whole UI is drawn with a single pipeline: an instanced triangle strip
 * of four vertices whose only vertex buffer is the array of HUI_DrawInstance.
 * A texture is one descriptor set (combined image sampler, binding 0).
 */

#include "vk_backend.h"

#include "generated/ui_vert_spv.h"
#include "generated/ui_frag_spv.h"

/* ---------------------------------------------------------------------------
 *  Device objects shared by every pipeline
 * ------------------------------------------------------------------------- */

static int hui_vk_shader_create(const uint32_t* code, size_t size, VkShaderModule* module) {
	VkShaderModuleCreateInfo info;
	memset(&info, 0, sizeof(info));
	info.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
	info.codeSize = size;
	info.pCode = code;
	return !hui_vk_failed(hui_vk.CreateShaderModule(g_vk.device, &info, NULL, module), "vkCreateShaderModule");
}

static int hui_vk_sampler_create(VkFilter filter, VkSampler* sampler) {
	VkSamplerCreateInfo info;
	memset(&info, 0, sizeof(info));
	info.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
	info.magFilter = filter;
	info.minFilter = filter;
	info.mipmapMode = VK_SAMPLER_MIPMAP_MODE_NEAREST;
	info.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
	info.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
	info.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
	info.maxAnisotropy = 1.0f;
	return !hui_vk_failed(hui_vk.CreateSampler(g_vk.device, &info, NULL, sampler), "vkCreateSampler");
}

int hui_vk_pipeline_init(void) {
	VkDescriptorSetLayoutBinding binding;
	VkDescriptorSetLayoutCreateInfo set_info;
	VkPushConstantRange push;
	VkPipelineLayoutCreateInfo layout_info;

	if (!hui_vk_shader_create(hui_ui_vert_spv, sizeof(hui_ui_vert_spv), &g_vk.vertex_shader)) return HUI_FALSE;
	if (!hui_vk_shader_create(hui_ui_frag_spv, sizeof(hui_ui_frag_spv), &g_vk.fragment_shader)) return HUI_FALSE;
	if (!hui_vk_sampler_create(VK_FILTER_LINEAR, &g_vk.sampler_linear)) return HUI_FALSE;
	if (!hui_vk_sampler_create(VK_FILTER_NEAREST, &g_vk.sampler_nearest)) return HUI_FALSE;

	memset(&binding, 0, sizeof(binding));
	binding.binding = 0;
	binding.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
	binding.descriptorCount = 1;
	binding.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
	memset(&set_info, 0, sizeof(set_info));
	set_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
	set_info.bindingCount = 1;
	set_info.pBindings = &binding;
	if (hui_vk_failed(hui_vk.CreateDescriptorSetLayout(g_vk.device, &set_info, NULL, &g_vk.set_layout),
		"vkCreateDescriptorSetLayout")) return HUI_FALSE;

	/* vec2 scale, vec2 translate */
	push.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;
	push.offset = 0;
	push.size = sizeof(float) * 4;
	memset(&layout_info, 0, sizeof(layout_info));
	layout_info.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
	layout_info.setLayoutCount = 1;
	layout_info.pSetLayouts = &g_vk.set_layout;
	layout_info.pushConstantRangeCount = 1;
	layout_info.pPushConstantRanges = &push;
	if (hui_vk_failed(hui_vk.CreatePipelineLayout(g_vk.device, &layout_info, NULL, &g_vk.pipeline_layout),
		"vkCreatePipelineLayout")) return HUI_FALSE;
	return HUI_TRUE;
}

void hui_vk_pipeline_shutdown(void) {
	uint32_t i;
	for (i = 0; i < g_vk.pass_count; i++) {
		if (g_vk.passes[i].pipeline) hui_vk.DestroyPipeline(g_vk.device, g_vk.passes[i].pipeline, NULL);
		if (g_vk.passes[i].render_pass) hui_vk.DestroyRenderPass(g_vk.device, g_vk.passes[i].render_pass, NULL);
	}
	g_vk.pass_count = 0;
	for (i = 0; i < g_vk.pool_count; i++) hui_vk.DestroyDescriptorPool(g_vk.device, g_vk.pools[i], NULL);
	hui_free(g_vk.pools);
	hui_free(g_vk.pool_used);
	g_vk.pools = NULL;
	g_vk.pool_used = NULL;
	g_vk.pool_count = g_vk.pool_cap = 0;
	if (g_vk.pipeline_layout) hui_vk.DestroyPipelineLayout(g_vk.device, g_vk.pipeline_layout, NULL);
	if (g_vk.set_layout) hui_vk.DestroyDescriptorSetLayout(g_vk.device, g_vk.set_layout, NULL);
	if (g_vk.sampler_linear) hui_vk.DestroySampler(g_vk.device, g_vk.sampler_linear, NULL);
	if (g_vk.sampler_nearest) hui_vk.DestroySampler(g_vk.device, g_vk.sampler_nearest, NULL);
	if (g_vk.vertex_shader) hui_vk.DestroyShaderModule(g_vk.device, g_vk.vertex_shader, NULL);
	if (g_vk.fragment_shader) hui_vk.DestroyShaderModule(g_vk.device, g_vk.fragment_shader, NULL);
	g_vk.pipeline_layout = VK_NULL_HANDLE;
	g_vk.set_layout = VK_NULL_HANDLE;
	g_vk.sampler_linear = g_vk.sampler_nearest = VK_NULL_HANDLE;
	g_vk.vertex_shader = g_vk.fragment_shader = VK_NULL_HANDLE;
}

/* ---------------------------------------------------------------------------
 *  Render pass + pipeline
 * ------------------------------------------------------------------------- */

static int hui_vk_render_pass_create(VkFormat format, VkRenderPass* render_pass) {
	VkAttachmentDescription attachment;
	VkAttachmentReference reference;
	VkSubpassDescription subpass;
	VkSubpassDependency dependency;
	VkRenderPassCreateInfo info;

	memset(&attachment, 0, sizeof(attachment));
	attachment.format = format;
	attachment.samples = VK_SAMPLE_COUNT_1_BIT;
	attachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
	attachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
	attachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
	attachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
	attachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	attachment.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

	reference.attachment = 0;
	reference.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

	memset(&subpass, 0, sizeof(subpass));
	subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
	subpass.colorAttachmentCount = 1;
	subpass.pColorAttachments = &reference;

	/* the image is only ours once the semaphore of vkAcquireNextImageKHR is signaled */
	memset(&dependency, 0, sizeof(dependency));
	dependency.srcSubpass = VK_SUBPASS_EXTERNAL;
	dependency.dstSubpass = 0;
	dependency.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
	dependency.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
	dependency.srcAccessMask = 0;
	dependency.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;

	memset(&info, 0, sizeof(info));
	info.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
	info.attachmentCount = 1;
	info.pAttachments = &attachment;
	info.subpassCount = 1;
	info.pSubpasses = &subpass;
	info.dependencyCount = 1;
	info.pDependencies = &dependency;
	return !hui_vk_failed(hui_vk.CreateRenderPass(g_vk.device, &info, NULL, render_pass), "vkCreateRenderPass");
}

static int hui_vk_pipeline_create(VkRenderPass render_pass, VkPipeline* pipeline) {
	VkPipelineShaderStageCreateInfo stages[2];
	VkVertexInputBindingDescription binding;
	VkVertexInputAttributeDescription attributes[8];
	VkPipelineVertexInputStateCreateInfo vertex_input;
	VkPipelineInputAssemblyStateCreateInfo assembly;
	VkPipelineViewportStateCreateInfo viewport;
	VkPipelineRasterizationStateCreateInfo raster;
	VkPipelineMultisampleStateCreateInfo multisample;
	VkPipelineColorBlendAttachmentState blend_attachment;
	VkPipelineColorBlendStateCreateInfo blend;
	VkPipelineDynamicStateCreateInfo dynamic;
	VkGraphicsPipelineCreateInfo info;
	static const VkDynamicState dynamic_states[] = { VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR };
	/* must match HUI_DrawInstance and the inputs of ui.vert */
	static const struct { VkFormat format; uint32_t offset; } layout[8] = {
		{ VK_FORMAT_R32G32B32A32_SFLOAT, offsetof(HUI_DrawInstance, rect) },
		{ VK_FORMAT_R32G32B32A32_SFLOAT, offsetof(HUI_DrawInstance, uv) },
		{ VK_FORMAT_R32G32B32A32_SFLOAT, offsetof(HUI_DrawInstance, clip) },
		{ VK_FORMAT_R32G32B32A32_SFLOAT, offsetof(HUI_DrawInstance, radii) },
		{ VK_FORMAT_R8G8B8A8_UNORM,      offsetof(HUI_DrawInstance, color) },
		{ VK_FORMAT_R8G8B8A8_UNORM,      offsetof(HUI_DrawInstance, border_color) },
		{ VK_FORMAT_R32_SFLOAT,          offsetof(HUI_DrawInstance, border_width) },
		{ VK_FORMAT_R32_UINT,            offsetof(HUI_DrawInstance, flags) }
	};
	uint32_t i;

	memset(stages, 0, sizeof(stages));
	stages[0].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
	stages[0].stage = VK_SHADER_STAGE_VERTEX_BIT;
	stages[0].module = g_vk.vertex_shader;
	stages[0].pName = "main";
	stages[1].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
	stages[1].stage = VK_SHADER_STAGE_FRAGMENT_BIT;
	stages[1].module = g_vk.fragment_shader;
	stages[1].pName = "main";

	binding.binding = 0;
	binding.stride = sizeof(HUI_DrawInstance);
	binding.inputRate = VK_VERTEX_INPUT_RATE_INSTANCE;
	for (i = 0; i < 8; i++) {
		attributes[i].location = i;
		attributes[i].binding = 0;
		attributes[i].format = layout[i].format;
		attributes[i].offset = layout[i].offset;
	}
	memset(&vertex_input, 0, sizeof(vertex_input));
	vertex_input.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
	vertex_input.vertexBindingDescriptionCount = 1;
	vertex_input.pVertexBindingDescriptions = &binding;
	vertex_input.vertexAttributeDescriptionCount = 8;
	vertex_input.pVertexAttributeDescriptions = attributes;

	memset(&assembly, 0, sizeof(assembly));
	assembly.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
	assembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP;

	memset(&viewport, 0, sizeof(viewport));
	viewport.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
	viewport.viewportCount = 1;
	viewport.scissorCount = 1;

	memset(&raster, 0, sizeof(raster));
	raster.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
	raster.polygonMode = VK_POLYGON_MODE_FILL;
	raster.cullMode = VK_CULL_MODE_NONE;
	raster.frontFace = VK_FRONT_FACE_CLOCKWISE;
	raster.lineWidth = 1.0f;

	memset(&multisample, 0, sizeof(multisample));
	multisample.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
	multisample.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

	/* the fragment shader outputs premultiplied colors */
	memset(&blend_attachment, 0, sizeof(blend_attachment));
	blend_attachment.blendEnable = VK_TRUE;
	blend_attachment.srcColorBlendFactor = VK_BLEND_FACTOR_ONE;
	blend_attachment.dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
	blend_attachment.colorBlendOp = VK_BLEND_OP_ADD;
	blend_attachment.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
	blend_attachment.dstAlphaBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
	blend_attachment.alphaBlendOp = VK_BLEND_OP_ADD;
	blend_attachment.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT |
		VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
	memset(&blend, 0, sizeof(blend));
	blend.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
	blend.attachmentCount = 1;
	blend.pAttachments = &blend_attachment;

	memset(&dynamic, 0, sizeof(dynamic));
	dynamic.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
	dynamic.dynamicStateCount = (uint32_t)HUI_COUNT_OF(dynamic_states);
	dynamic.pDynamicStates = dynamic_states;

	memset(&info, 0, sizeof(info));
	info.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
	info.stageCount = 2;
	info.pStages = stages;
	info.pVertexInputState = &vertex_input;
	info.pInputAssemblyState = &assembly;
	info.pViewportState = &viewport;
	info.pRasterizationState = &raster;
	info.pMultisampleState = &multisample;
	info.pColorBlendState = &blend;
	info.pDynamicState = &dynamic;
	info.layout = g_vk.pipeline_layout;
	info.renderPass = render_pass;
	info.subpass = 0;
	return !hui_vk_failed(hui_vk.CreateGraphicsPipelines(g_vk.device, VK_NULL_HANDLE, 1, &info, NULL, pipeline),
		"vkCreateGraphicsPipelines");
}

hui_vk_pass_t* hui_vk_pass_get(VkFormat format) {
	hui_vk_pass_t* pass;
	uint32_t i;
	for (i = 0; i < g_vk.pass_count; i++) {
		if (g_vk.passes[i].format == format) return &g_vk.passes[i];
	}
	if (g_vk.pass_count == HUI_VK_MAX_PASSES) {
		hui_error(HUI_ERROR_BACKEND, HUI_SEVERITY_ERROR, "Vulkan: too many different swapchain formats");
		return NULL;
	}
	pass = &g_vk.passes[g_vk.pass_count];
	memset(pass, 0, sizeof(*pass));
	pass->format = format;
	if (!hui_vk_render_pass_create(format, &pass->render_pass)) return NULL;
	if (!hui_vk_pipeline_create(pass->render_pass, &pass->pipeline)) {
		hui_vk.DestroyRenderPass(g_vk.device, pass->render_pass, NULL);
		return NULL;
	}
	g_vk.pass_count++;
	return pass;
}

/* ---------------------------------------------------------------------------
 *  Descriptor sets
 * ------------------------------------------------------------------------- */

int hui_vk_set_allocate(VkDescriptorSet* set, VkDescriptorPool* pool) {
	VkDescriptorSetAllocateInfo allocation;
	uint32_t i;

	memset(&allocation, 0, sizeof(allocation));
	allocation.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
	allocation.descriptorSetCount = 1;
	allocation.pSetLayouts = &g_vk.set_layout;

	for (i = 0; i < g_vk.pool_count; i++) {
		if (g_vk.pool_used[i] < HUI_VK_SETS_PER_POOL) break;
	}
	if (i == g_vk.pool_count) {
		/* every pool is full: add one */
		VkDescriptorPoolSize size;
		VkDescriptorPoolCreateInfo info;
		VkDescriptorPool created;
		size.type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
		size.descriptorCount = HUI_VK_SETS_PER_POOL;
		memset(&info, 0, sizeof(info));
		info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
		info.flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT;
		info.maxSets = HUI_VK_SETS_PER_POOL;
		info.poolSizeCount = 1;
		info.pPoolSizes = &size;
		if (hui_vk_failed(hui_vk.CreateDescriptorPool(g_vk.device, &info, NULL, &created), "vkCreateDescriptorPool")) return HUI_FALSE;
		if (g_vk.pool_count == g_vk.pool_cap) {
			g_vk.pool_cap = g_vk.pool_cap ? g_vk.pool_cap * 2 : 4;
			g_vk.pools = (VkDescriptorPool*)hui_realloc(g_vk.pools, sizeof(VkDescriptorPool) * g_vk.pool_cap);
			g_vk.pool_used = (uint32_t*)hui_realloc(g_vk.pool_used, sizeof(uint32_t) * g_vk.pool_cap);
		}
		g_vk.pools[g_vk.pool_count] = created;
		g_vk.pool_used[g_vk.pool_count] = 0;
		g_vk.pool_count++;
	}

	allocation.descriptorPool = g_vk.pools[i];
	if (hui_vk_failed(hui_vk.AllocateDescriptorSets(g_vk.device, &allocation, set), "vkAllocateDescriptorSets")) return HUI_FALSE;
	g_vk.pool_used[i]++;
	*pool = g_vk.pools[i];
	return HUI_TRUE;
}

void hui_vk_set_free(VkDescriptorSet set, VkDescriptorPool pool) {
	uint32_t i;
	if (!set) return;
	hui_vk.FreeDescriptorSets(g_vk.device, pool, 1, &set);
	for (i = 0; i < g_vk.pool_count; i++) {
		if (g_vk.pools[i] == pool && g_vk.pool_used[i] > 0) g_vk.pool_used[i]--;
	}
}

void hui_vk_set_write(VkDescriptorSet set, VkImageView view, VkSampler sampler) {
	VkDescriptorImageInfo image;
	VkWriteDescriptorSet write;
	image.sampler = sampler;
	image.imageView = view;
	image.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
	memset(&write, 0, sizeof(write));
	write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
	write.dstSet = set;
	write.dstBinding = 0;
	write.descriptorCount = 1;
	write.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
	write.pImageInfo = &image;
	hui_vk.UpdateDescriptorSets(g_vk.device, 1, &write, 0, NULL);
}
