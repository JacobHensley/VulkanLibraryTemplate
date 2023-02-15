#include "AppLayer.h"
#include "Core/Application.h"
#include "ImGui/imgui_impl_vulkan.h"

AppLayer::AppLayer(const std::string& name)
	: Layer("AppLayer")
{
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
}

void AppLayer::OnRender()
{
}

void AppLayer::OnImGUIRender()
{
	ImGui::ShowDemoWindow();
}
