#include "AppLayer.h"
#include "Core/Application.h"
#include "Graphics/TextureCube.h"

#include "ImGui/imgui.h"
#include "ImGui/imgui_impl_vulkan.h"

#include <glm/gtc/type_ptr.hpp>

AppLayer::AppLayer(const std::string& name)
	: Layer("AppLayer")
{
	m_Mesh = CreateRef<Mesh>("assets/models/Suzanne/glTF/Suzanne.gltf");
	m_Shader = CreateRef<Shader>("assets/shaders/Demo.glsl");
	m_PreethamSkyShader = CreateRef<Shader>("assets/shaders/PreethamSky.glsl");
	m_SkyboxShader = CreateRef<Shader>("assets/shaders/Skybox.glsl");

	m_TextureCube = CreateRef<TextureCube>(2048, 2048, VK_FORMAT_R32G32B32A32_SFLOAT);

	FramebufferSpecification framebufferSpec;
	framebufferSpec.AttachmentFormats = { VK_FORMAT_R32G32B32A32_SFLOAT, VK_FORMAT_D24_UNORM_S8_UINT };
	framebufferSpec.Width = 1280;
	framebufferSpec.Height = 720;
	framebufferSpec.ClearOnLoad = true;
	m_Framebuffer = CreateRef<Framebuffer>(framebufferSpec);

	GraphicsPipelineSpecification pipelineSpec;
	pipelineSpec.Shader = m_Shader;
	pipelineSpec.TargetRenderPass = m_Framebuffer->GetRenderPass();
	m_Pipeline = CreateRef<GraphicsPipeline>(pipelineSpec);

	{
		ComputePipelineSpecification computePipelineSpec;
		computePipelineSpec.Shader = m_PreethamSkyShader;
		m_PreethamSkyPipeline = CreateRef<ComputePipeline>(computePipelineSpec);
	}

	{
		GraphicsPipelineSpecification pipelineSpec;
		pipelineSpec.Shader = m_SkyboxShader;
		pipelineSpec.TargetRenderPass = m_Framebuffer->GetRenderPass();
		m_SkyboxPipeline = CreateRef<GraphicsPipeline>(pipelineSpec);
	}

	CameraSpecification cameraSpec;
	m_Camera = CreateRef<Camera>(cameraSpec);

	m_CameraBuffer.ViewProjection = m_Camera->GetViewProjection();
	m_CameraBuffer.InverseViewProjection = m_Camera->GetInverseViewProjection();
	m_CameraBuffer.CameraPosition = m_Camera->GetPosition();

	m_CameraUniformBuffer = CreateRef<UniformBuffer>(&m_CameraBuffer, sizeof(CameraBuffer), "Camera Uniform Buffer");

	m_DescriptorPool = CreateDescriptorPool();

	{
		m_ViewportDescriptorPool = CreateDescriptorPool();

		VkDescriptorSetAllocateInfo descriptorSetAllocateInfo{};
		descriptorSetAllocateInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;

		descriptorSetAllocateInfo.pSetLayouts = &ImGui_ImplVulkan_GetDescriptorSetLayout();
		descriptorSetAllocateInfo.descriptorSetCount = 1;

		m_ViewportImageDescriptorSet = AllocateDescriptorSet(descriptorSetAllocateInfo, m_ViewportDescriptorPool);
	}

	{
		Ref<VulkanDevice> device = Application::GetApp().GetVulkanDevice();

		VkDescriptorSetAllocateInfo descriptorSetAllocateInfo{};
		descriptorSetAllocateInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
		descriptorSetAllocateInfo.pSetLayouts = m_PreethamSkyShader->GetDescriptorSetLayouts().data();
		descriptorSetAllocateInfo.descriptorSetCount = m_PreethamSkyShader->GetDescriptorSetLayouts().size();

		m_ComputeDescriptorSet = AllocateDescriptorSet(descriptorSetAllocateInfo, m_DescriptorPool);

		VkWriteDescriptorSet cubeMapWriteDescriptor{};
		cubeMapWriteDescriptor.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		cubeMapWriteDescriptor.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
		cubeMapWriteDescriptor.dstBinding = 0;
		cubeMapWriteDescriptor.pImageInfo = &m_TextureCube->GetDescriptorImageInfo();
		cubeMapWriteDescriptor.descriptorCount = 1;
		cubeMapWriteDescriptor.dstSet = m_ComputeDescriptorSet;

		vkUpdateDescriptorSets(device->GetLogicalDevice(), 1, &cubeMapWriteDescriptor, 0, NULL);
	}

	{
		Ref<VulkanDevice> device = Application::GetApp().GetVulkanDevice();

		VkDescriptorSetAllocateInfo descriptorSetAllocateInfo{};
		descriptorSetAllocateInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
		descriptorSetAllocateInfo.pSetLayouts = m_SkyboxShader->GetDescriptorSetLayouts().data();
		descriptorSetAllocateInfo.descriptorSetCount = m_SkyboxShader->GetDescriptorSetLayouts().size();

		m_SkyboxDescriptorSet = AllocateDescriptorSet(descriptorSetAllocateInfo, m_DescriptorPool);

		std::vector<VkWriteDescriptorSet> skyboxWriteDescriptors;

		VkWriteDescriptorSet& cameraWriteDescriptor = skyboxWriteDescriptors.emplace_back();
		cameraWriteDescriptor.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		cameraWriteDescriptor.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
		cameraWriteDescriptor.dstBinding = 0;
		cameraWriteDescriptor.pBufferInfo = &m_CameraUniformBuffer->GetDescriptorBufferInfo();
		cameraWriteDescriptor.descriptorCount = 1;
		cameraWriteDescriptor.dstSet = m_SkyboxDescriptorSet;

		VkWriteDescriptorSet& cubeMapWriteDescriptor = skyboxWriteDescriptors.emplace_back();
		cubeMapWriteDescriptor.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		cubeMapWriteDescriptor.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
		cubeMapWriteDescriptor.dstBinding = 1;
		cubeMapWriteDescriptor.pImageInfo = &m_TextureCube->GetDescriptorImageInfo();
		cubeMapWriteDescriptor.descriptorCount = 1;
		cubeMapWriteDescriptor.dstSet = m_SkyboxDescriptorSet;

		vkUpdateDescriptorSets(device->GetLogicalDevice(), skyboxWriteDescriptors.size(), skyboxWriteDescriptors.data(), 0, NULL);
	}

	
	Ref<VulkanDevice> device = Application::GetApp().GetVulkanDevice();
	//VK_CHECK_RESULT(vkResetDescriptorPool(device->GetLogicalDevice(), m_DescriptorPool, 0));

	VkDescriptorSetAllocateInfo descriptorSetAllocateInfo{};
	descriptorSetAllocateInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
	descriptorSetAllocateInfo.pSetLayouts = m_Shader->GetDescriptorSetLayouts().data();
	descriptorSetAllocateInfo.descriptorSetCount = m_Shader->GetDescriptorSetLayouts().size();

	m_DescriptorSet = AllocateDescriptorSet(descriptorSetAllocateInfo, m_DescriptorPool);

	VkWriteDescriptorSet& cameraWriteDescriptor = m_WriteDescriptors.emplace_back();
	cameraWriteDescriptor.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
	cameraWriteDescriptor.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
	cameraWriteDescriptor.dstBinding = 0;
	cameraWriteDescriptor.pBufferInfo = &m_CameraUniformBuffer->GetDescriptorBufferInfo();
	cameraWriteDescriptor.descriptorCount = 1;
	cameraWriteDescriptor.dstSet = m_DescriptorSet;

	VkWriteDescriptorSet& albedoTextureWriteDescriptor = m_WriteDescriptors.emplace_back();
	albedoTextureWriteDescriptor.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
	albedoTextureWriteDescriptor.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
	albedoTextureWriteDescriptor.dstBinding = 1;
	albedoTextureWriteDescriptor.pImageInfo = &m_Mesh->GetTextures()[m_Mesh->GetMaterialBuffers()[0].AlbedoMapIndex]->GetDescriptorImageInfo();
	albedoTextureWriteDescriptor.descriptorCount = 1;
	albedoTextureWriteDescriptor.dstSet = m_DescriptorSet;

	VkWriteDescriptorSet& metallicRoughnessTextureWriteDescriptor = m_WriteDescriptors.emplace_back();
	metallicRoughnessTextureWriteDescriptor.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
	metallicRoughnessTextureWriteDescriptor.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
	metallicRoughnessTextureWriteDescriptor.dstBinding = 2;
	metallicRoughnessTextureWriteDescriptor.pImageInfo = &m_Mesh->GetTextures()[m_Mesh->GetMaterialBuffers()[0].MetallicRoughnessMapIndex]->GetDescriptorImageInfo();
	metallicRoughnessTextureWriteDescriptor.descriptorCount = 1;
	metallicRoughnessTextureWriteDescriptor.dstSet = m_DescriptorSet;

	VkWriteDescriptorSet& normalTextureWriteDescriptor = m_WriteDescriptors.emplace_back();
	normalTextureWriteDescriptor.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
	normalTextureWriteDescriptor.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
	normalTextureWriteDescriptor.dstBinding = 3;
	normalTextureWriteDescriptor.pImageInfo = &m_Mesh->GetTextures()[m_Mesh->GetMaterialBuffers()[0].NormalMapIndex]->GetDescriptorImageInfo();
	normalTextureWriteDescriptor.descriptorCount = 1;
	normalTextureWriteDescriptor.dstSet = m_DescriptorSet;

	vkUpdateDescriptorSets(device->GetLogicalDevice(), m_WriteDescriptors.size(), m_WriteDescriptors.data(), 0, NULL);

	CameraBuffer* mem = m_CameraUniformBuffer->Map<CameraBuffer>();
	memcpy(mem, &m_CameraBuffer, sizeof(CameraBuffer));
	m_CameraUniformBuffer->Unmap();
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
	m_Camera->Update();

	{
		Ref<VulkanDevice> device = Application::GetApp().GetVulkanDevice();
		VkCommandBuffer activeCommandBuffer = device->CreateCommandBuffer(VK_COMMAND_BUFFER_LEVEL_PRIMARY, true);
		vkCmdBindPipeline(activeCommandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, m_PreethamSkyPipeline->GetPipeline());
		vkCmdBindDescriptorSets(activeCommandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, m_PreethamSkyPipeline->GetPipelineLayout(), 0, 1, &m_ComputeDescriptorSet, 0, nullptr);
		vkCmdPushConstants(activeCommandBuffer, m_PreethamSkyPipeline->GetPipelineLayout(), VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(glm::vec3), &m_SkyBoxSettings);
		vkCmdDispatch(activeCommandBuffer, 64, 64, 6);
		device->FlushCommandBuffer(activeCommandBuffer, true);
	}
}

void AppLayer::OnRender()
{
	Ref<VulkanDevice> device = Application::GetApp().GetVulkanDevice();

	{
		VkCommandBuffer activeCommandBuffer = device->CreateCommandBuffer(VK_COMMAND_BUFFER_LEVEL_PRIMARY, true);
		BeginRenderPass(m_Framebuffer, activeCommandBuffer, false);

		vkCmdBindPipeline(activeCommandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, m_SkyboxPipeline->GetPipeline());
		vkCmdBindDescriptorSets(activeCommandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, m_SkyboxPipeline->GetPipelineLayout(), 0, 1, &m_SkyboxDescriptorSet, 0, nullptr);
		vkCmdDraw(activeCommandBuffer, 3, 1, 0, 0);

		EndRenderPass(activeCommandBuffer);
		device->FlushCommandBuffer(activeCommandBuffer, true);
	}

	VkCommandBuffer activeCommandBuffer = device->CreateCommandBuffer(VK_COMMAND_BUFFER_LEVEL_PRIMARY, true);
	BeginRenderPass(m_Framebuffer, activeCommandBuffer, false);
	
	vkCmdBindPipeline(activeCommandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, m_Pipeline->GetPipeline());

	glm::mat4 transform = glm::scale(glm::mat4(1.0f), glm::vec3(1.0f));

	VkDeviceSize offset = 0;
	VkBuffer vertexBuffer = m_Mesh->GetVertexBuffer()->GetBuffer();
	vkCmdBindVertexBuffers(activeCommandBuffer, 0, 1, &vertexBuffer, &offset);
	vkCmdBindIndexBuffer(activeCommandBuffer, m_Mesh->GetIndexBuffer()->GetBuffer(), 0, VK_INDEX_TYPE_UINT32);

	vkCmdPushConstants(activeCommandBuffer, m_Pipeline->GetPipelineLayout(), VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(glm::mat4), &transform);
	vkCmdBindDescriptorSets(activeCommandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, m_Pipeline->GetPipelineLayout(), 0, 1, &m_DescriptorSet, 0, nullptr);

	for (int i = 0;i < m_Mesh->GetSubMeshes().size();i++)
	{
		const SubMesh& subMesh = m_Mesh->GetSubMeshes()[i];

		vkCmdDrawIndexed(activeCommandBuffer, subMesh.IndexCount, 1, subMesh.IndexOffset, subMesh.VertexOffset, 0);
	}
		

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
	bool resized = m_Framebuffer->Resize(size.x, size.y);
	if (resized)
	{
		m_Camera->Resize(size.x, size.y);
	}
	
	m_CameraBuffer.ViewProjection = m_Camera->GetViewProjection();
	m_CameraBuffer.InverseViewProjection = m_Camera->GetInverseViewProjection();
	m_CameraBuffer.CameraPosition = m_Camera->GetPosition();

	CameraBuffer* mem = m_CameraUniformBuffer->Map<CameraBuffer>();
	memcpy(mem, &m_CameraBuffer, sizeof(CameraBuffer));
	m_CameraUniformBuffer->Unmap();

	UpdateViewportDescriptor();
	ImGui::Image(m_ViewportImageDescriptorSet, ImVec2(size.x, size.y), ImVec2::ImVec2(0, 1), ImVec2::ImVec2(1, 0));

	ImGui::End();
	ImGui::PopStyleVar();

	ImGui::Begin("Settings");
	ImGui::DragFloat3("Test", glm::value_ptr(m_SkyBoxSettings), 0.1f, 0.0f, 10.0f);
	ImGui::End();
}

VkDescriptorPool AppLayer::CreateDescriptorPool()
{
	VkDevice device = Application::GetVulkanDevice()->GetLogicalDevice();

	VkDescriptorPoolSize poolSizes[] =
	{
		{ VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 10 },
		{ VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 10 },
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

VkDescriptorSet AppLayer::AllocateDescriptorSet(VkDescriptorSetAllocateInfo allocateInfo, VkDescriptorPool pool)
{
	VkDevice device = Application::GetApp().GetVulkanDevice()->GetLogicalDevice();

	allocateInfo.descriptorPool = pool;

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

void AppLayer::UpdateViewportDescriptor()
{
	const VkDescriptorImageInfo& descriptorInfo = m_Framebuffer->GetImage(0)->GetDescriptorImageInfo();
	// Up to date
	if (m_ViewportImageView == descriptorInfo.imageView)
		return;

	VkDescriptorImageInfo desc_image{};
	desc_image.sampler = descriptorInfo.sampler;
	desc_image.imageView = descriptorInfo.imageView;
	desc_image.imageLayout = descriptorInfo.imageLayout;
	VkWriteDescriptorSet write_desc{};
	write_desc.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
	write_desc.dstSet = m_ViewportImageDescriptorSet;
	write_desc.descriptorCount = 1;
	write_desc.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
	write_desc.pImageInfo = &desc_image;

	VkDevice device = Application::GetVulkanDevice()->GetLogicalDevice();
	vkUpdateDescriptorSets(device, 1, &write_desc, 0, nullptr);
	m_ViewportImageView = descriptorInfo.imageView;
}
