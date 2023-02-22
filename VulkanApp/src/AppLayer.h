#pragma once
#include "Core/Core.h"
#include "Core/Layer.h"
#include "Graphics/Mesh.h"
#include "Graphics/Shader.h"
#include "Graphics/Framebuffer.h"
#include "Graphics/GraphicsPipeline.h"
#include "Graphics/Camera.h"
#include "Graphics/VulkanBuffers.h"
#include "Graphics/VulkanTools.h"
#include <vulkan/vulkan.h>

using namespace VkLibrary;

struct CameraBuffer
{
	glm::mat4 ViewProjection;
};

class AppLayer : public Layer
{
	public:
		AppLayer(const std::string& name);
		~AppLayer();

	public:
		void OnAttach();
		void OnDetach();

		void OnUpdate();
		void OnRender();

		void OnImGUIRender();

	private:
		VkDescriptorPool CreateDescriptorPool();
		VkDescriptorSet AllocateDescriptorSet(VkDescriptorSetAllocateInfo allocateInfo, VkDescriptorPool pool);
		void BeginRenderPass(Ref<Framebuffer> framebuffer, VkCommandBuffer activeCommandBuffer, bool explicitClear);
		void EndRenderPass(VkCommandBuffer activeCommandBuffer);

		void UpdateViewportDescriptor();
	private:
		Ref<Mesh> m_Mesh;
		Ref<Shader> m_Shader;
		Ref<Framebuffer> m_Framebuffer;
		Ref<GraphicsPipeline> m_Pipeline;
		Ref<Camera> m_Camera;
		Ref<UniformBuffer> m_CameraUniformBuffer;
		CameraBuffer m_CameraBuffer;

		VkDescriptorPool m_DescriptorPool = VK_NULL_HANDLE;
		VkDescriptorPool m_ViewportDescriptorPool = VK_NULL_HANDLE;
		VkDescriptorSet m_DescriptorSet = VK_NULL_HANDLE;
		VkDescriptorSet m_ViewportImageDescriptorSet = VK_NULL_HANDLE;
		VkImageView m_ViewportImageView = VK_NULL_HANDLE;
		std::vector<VkWriteDescriptorSet> m_WriteDescriptors;
};