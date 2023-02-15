#pragma once
#include "Core/Layer.h"
#include <vulkan/vulkan.h>

using namespace VkLibrary;

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
};