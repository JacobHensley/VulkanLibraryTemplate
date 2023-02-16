#include "AppLayer.h"
#include "Core/Application.h"
#include "ImGui/imgui.h"
#include "ImGui/imgui_impl_vulkan.h"

AppLayer::AppLayer(const std::string& name)
	: Layer("AppLayer")
{
	m_Mesh = CreateRef<Mesh>("assets/models/Suzanne/Suzanne.gltf");
	m_Shader = CreateRef<Shader>("assets/shaders/Demo.glsl");

	FramebufferSpecification framebufferSpec;
	framebufferSpec.AttachmentFormats = { VK_FORMAT_R32G32B32A32_SFLOAT, VK_FORMAT_D24_UNORM_S8_UINT };
	framebufferSpec.Width = 1280;
	framebufferSpec.Height = 720;
	framebufferSpec.ClearOnLoad = false;
	m_Framebuffer = CreateRef<Framebuffer>(framebufferSpec);

	GraphicsPipelineSpecification pipelineSpec;
	pipelineSpec.Shader = m_Shader;
	pipelineSpec.TargetRenderPass = m_Framebuffer->GetRenderPass();
	m_Pipeline = CreateRef<GraphicsPipeline>(pipelineSpec);

	CameraSpecification cameraSpec;
	m_Camera = CreateRef<Camera>(cameraSpec);

	m_CameraBuffer.ViewProjection = m_Camera->GetViewProjection();

	m_CameraUniformBuffer = CreateRef<UniformBuffer>(&m_CameraBuffer, sizeof(CameraBuffer), "Camera Uniform Buffer");

	m_DescriptorPool = CreateDescriptorPool();

	VkWriteDescriptorSet& cameraWriteDescriptor = m_WriteDescriptors.emplace_back();
	cameraWriteDescriptor.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
	cameraWriteDescriptor.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
	cameraWriteDescriptor.dstBinding = 0;
	cameraWriteDescriptor.pBufferInfo = &m_CameraUniformBuffer->GetDescriptorBufferInfo();
	cameraWriteDescriptor.descriptorCount = 1;
}

AppLayer::~AppLayer()
{
}

void AppLayer::OnAttach()
{
	
}

void AppLayer::OnDetach()
{
}

void AppLayer::OnUpdate()
{
	Ref<VulkanDevice> device = Application::GetApp().GetVulkanDevice();

	VK_CHECK_RESULT(vkResetDescriptorPool(device->GetLogicalDevice(), m_DescriptorPool, 0));

	VkDescriptorSetAllocateInfo descriptorSetAllocateInfo{};
	descriptorSetAllocateInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
	descriptorSetAllocateInfo.pSetLayouts = m_Shader->GetDescriptorSetLayouts().data();
	descriptorSetAllocateInfo.descriptorSetCount = m_Shader->GetDescriptorSetLayouts().size();

	m_DescriptorSet = AllocateDescriptorSet(descriptorSetAllocateInfo);

	for (auto& wd : m_WriteDescriptors)
		wd.dstSet = m_DescriptorSet;

	vkUpdateDescriptorSets(device->GetLogicalDevice(), m_WriteDescriptors.size(), m_WriteDescriptors.data(), 0, NULL);
}

void AppLayer::OnRender()
{
	Ref<VulkanDevice> device = Application::GetApp().GetVulkanDevice();

	VkCommandBuffer activeCommandBuffer = device->CreateCommandBuffer(VK_COMMAND_BUFFER_LEVEL_PRIMARY, true);
	BeginRenderPass(m_Framebuffer, activeCommandBuffer, false);

	vkCmdBindPipeline(activeCommandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, m_Pipeline->GetPipeline());

	glm::mat4 transform = glm::mat4(1.0f);

	VkDeviceSize offset = 0;
	VkBuffer vertexBuffer = m_Mesh->GetVertexBuffer()->GetBuffer();
	vkCmdBindVertexBuffers(activeCommandBuffer, 0, 1, &vertexBuffer, &offset);
	vkCmdBindIndexBuffer(activeCommandBuffer, m_Mesh->GetIndexBuffer()->GetBuffer(), 0, VK_INDEX_TYPE_UINT32);

	vkCmdPushConstants(activeCommandBuffer, m_Pipeline->GetPipelineLayout(), VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(glm::mat4), &transform);
	vkCmdBindDescriptorSets(activeCommandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, m_Pipeline->GetPipelineLayout(), 0, 1, &m_DescriptorSet, 0, nullptr);

	for (const SubMesh& subMesh : m_Mesh->GetSubMeshes())
		vkCmdDrawIndexed(activeCommandBuffer, subMesh.IndexCount, 1, subMesh.IndexOffset, subMesh.VertexOffset, 0);

	EndRenderPass(activeCommandBuffer);
	device->FlushCommandBuffer(activeCommandBuffer, true);
}

void AppLayer::OnImGUIRender()
{
	ImGuiIO io = ImGui::GetIO();
	io.ConfigWindowsMoveFromTitleBarOnly = true;
	bool open = true;
	ImGuiWindowFlags flags = ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse | ImGuiWindowFlags_NoCollapse;

	ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
	ImGui::Begin("Editor", &open, flags);

	ImVec2 size = ImGui::GetContentRegionAvail();

	// Draw scene
	const VkDescriptorImageInfo& descriptorInfo = m_Framebuffer->GetImage(0)->GetDescriptorImageInfo();
	ImTextureID textureID = ImGui_ImplVulkan_AddTexture(descriptorInfo.sampler, descriptorInfo.imageView, descriptorInfo.imageLayout);

	ImGui::Image((void*)textureID, ImVec2(size.x, size.y), ImVec2::ImVec2(0, 1), ImVec2::ImVec2(1, 0));
	bool resized = m_Framebuffer->Resize(size.x, size.y);

	if (resized)
	{
		m_Camera->Resize(size.x, size.y);
	}

	ImGui::End();
	ImGui::PopStyleVar();
}

VkDescriptorPool AppLayer::CreateDescriptorPool()
{
	VkDevice device = Application::GetVulkanDevice()->GetLogicalDevice();

	VkDescriptorPoolSize poolSizes[] =
	{
		{ VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 10 },
		{ VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 10 },
		{ VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 10 }
	};

	VkDescriptorPoolCreateInfo descriptorPoolCreateInfo = {};
	descriptorPoolCreateInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
	descriptorPoolCreateInfo.flags = 0;
	descriptorPoolCreateInfo.maxSets = 1000;
	descriptorPoolCreateInfo.poolSizeCount = 1;
	descriptorPoolCreateInfo.pPoolSizes = poolSizes;

	VkDescriptorPool descriptorPool;
	VK_CHECK_RESULT(vkCreateDescriptorPool(device, &descriptorPoolCreateInfo, nullptr, &descriptorPool));

	return descriptorPool;
}

VkDescriptorSet AppLayer::AllocateDescriptorSet(VkDescriptorSetAllocateInfo allocateInfo)
{
	VkDevice device = Application::GetApp().GetVulkanDevice()->GetLogicalDevice();

	allocateInfo.descriptorPool = m_DescriptorPool;

	VkDescriptorSet descriptorSet;
	VK_CHECK_RESULT(vkAllocateDescriptorSets(device, &allocateInfo, &descriptorSet));

	return descriptorSet;
}

void AppLayer::BeginRenderPass(Ref<Framebuffer> framebuffer, VkCommandBuffer activeCommandBuffer, bool explicitClear)
{
	VkClearValue clearColor[2];
	clearColor[0].color = { 0.1f, 0.1f, 0.1f, 1.0f };
	clearColor[1].depthStencil = { 1.0f, 0 };

	VkExtent2D extent = { framebuffer->GetWidth(), framebuffer->GetHeight() };

	VkRenderPassBeginInfo renderPassInfo{};
	renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
	renderPassInfo.renderPass = framebuffer->GetRenderPass();
	renderPassInfo.framebuffer = framebuffer->GetFramebuffer();
	renderPassInfo.renderArea.offset = { 0, 0 };
	renderPassInfo.renderArea.extent = extent;
	renderPassInfo.clearValueCount = 2;
	renderPassInfo.pClearValues = clearColor;

	VkViewport viewport{};
	viewport.x = 0.0f;
	viewport.y = 0.0f;
	viewport.width = (float)extent.width;
	viewport.height = (float)extent.height;
	viewport.minDepth = 0.0f;
	viewport.maxDepth = 1.0f;

	vkCmdSetViewport(activeCommandBuffer, 0, 1, &viewport);

	VkRect2D scissor;
	scissor.offset = renderPassInfo.renderArea.offset;
	scissor.extent = renderPassInfo.renderArea.extent;

	vkCmdSetScissor(activeCommandBuffer, 0, 1, &scissor);

	vkCmdBeginRenderPass(activeCommandBuffer, &renderPassInfo, VK_SUBPASS_CONTENTS_INLINE);

	if (explicitClear)
	{
		const uint32_t colorAttachmentCount = (uint32_t)framebuffer->GetColorAttachmentCount();
		const uint32_t totalAttachmentCount = colorAttachmentCount + (framebuffer->HasDepthAttachment() ? 1 : 0);

		VkClearValue clearValues;
		glm::vec4 clearColor = framebuffer->GetSpecification().ClearColor;
		clearValues.color = { clearColor.r, clearColor.g, clearColor.b, clearColor.a };

		std::vector<VkClearAttachment> attachments(totalAttachmentCount);
		std::vector<VkClearRect> clearRects(totalAttachmentCount);

		for (uint32_t i = 0; i < colorAttachmentCount; i++)
		{
			attachments[i].aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
			attachments[i].colorAttachment = i;
			attachments[i].clearValue = clearValues;

			clearRects[i].rect.offset = { (int32_t)0, (int32_t)0 };
			clearRects[i].rect.extent = { extent.width, extent.height };
			clearRects[i].baseArrayLayer = 0;
			clearRects[i].layerCount = 1;
		}

		if (framebuffer->HasDepthAttachment())
		{
			clearValues.depthStencil = { 1.0f, 0 };

			attachments[colorAttachmentCount].aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT;
			attachments[colorAttachmentCount].clearValue = clearValues;
			clearRects[colorAttachmentCount].rect.offset = { (int32_t)0, (int32_t)0 };
			clearRects[colorAttachmentCount].rect.extent = { extent.width,  extent.height };
			clearRects[colorAttachmentCount].baseArrayLayer = 0;
			clearRects[colorAttachmentCount].layerCount = 1;
		}

		vkCmdClearAttachments(activeCommandBuffer, totalAttachmentCount, attachments.data(), totalAttachmentCount, clearRects.data());
	}
}

void AppLayer::EndRenderPass(VkCommandBuffer activeCommandBuffer)
{
	vkCmdEndRenderPass(activeCommandBuffer);
}