#include "Hydrogen/Platform/Vulkan/VulkanDebugGUI.hpp"
#include "Hydrogen/Platform/Vulkan/VulkanRenderPass.hpp"
#include "Hydrogen/Platform/Vulkan/VulkanCommandQueue.hpp"
#include "Hydrogen/Core.hpp"

// TODO: This could staticly change in the future -> use preprocessor
#define VK_USE_PLATFORM_WIN32_KHR
#include "backends/imgui_impl_vulkan.h"

using namespace Hydrogen;

static void CheckVulkanResult(VkResult res)
{
	HY_ASSERT(res == VK_SUCCESS, "ImGui Vulkan command returend an error code {}", (uint64_t)res);
}

VulkanDebugGUI::VulkanDebugGUI(const std::shared_ptr<RenderContext>& renderContext, const std::shared_ptr<RenderPass>& renderPass)
	: m_RenderContext(RenderContext::Get<VulkanRenderContext>(renderContext))
{
	VkDescriptorPoolSize pool_sizes[] =
	{
		{ VK_DESCRIPTOR_TYPE_SAMPLER, 1000 },
		{ VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1000 },
		{ VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, 1000 },
		{ VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1000 },
		{ VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER, 1000 },
		{ VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER, 1000 },
		{ VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1000 },
		{ VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1000 },
		{ VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, 1000 },
		{ VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC, 1000 },
		{ VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT, 1000 }
	};

	VkDescriptorPoolCreateInfo pool_info = {};
	pool_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
	pool_info.flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT;
	pool_info.maxSets = 1000;
	pool_info.poolSizeCount = std::size(pool_sizes);
	pool_info.pPoolSizes = pool_sizes;

	HY_ASSERT(vkCreateDescriptorPool(m_RenderContext->GetDevice(), &pool_info, nullptr, &m_ImguiPool) == VK_SUCCESS, "Failed to create vulkan descriptor pool");

	ImGui::CreateContext();
	ImGuiIO& io = ImGui::GetIO();
	io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
	io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;
	io.ConfigFlags |= ImGuiConfigFlags_ViewportsEnable;

	m_RenderContext->GetViewport()->InitImGui();

	ImGui_ImplVulkan_InitInfo init_info = {};
	init_info.Instance = m_RenderContext->GetInstance();
	init_info.PhysicalDevice = m_RenderContext->GetPhysicalDevice();
	init_info.Device = m_RenderContext->GetDevice();
	init_info.Queue = m_RenderContext->GetGraphicsQueue();
	init_info.DescriptorPool = m_ImguiPool;
	init_info.MinImageCount = 3;
	init_info.ImageCount = 3;
	init_info.MSAASamples = VK_SAMPLE_COUNT_1_BIT;

	init_info.Instance = m_RenderContext->GetInstance();
	init_info.PhysicalDevice = m_RenderContext->GetPhysicalDevice();
	init_info.Device = m_RenderContext->GetDevice();
	init_info.QueueFamily = m_RenderContext->GetGraphicsQueueFamily();
	init_info.Queue = m_RenderContext->GetGraphicsQueue();
	//init_info.PipelineCache = g_PipelineCache;
	init_info.DescriptorPool = m_ImguiPool;
	init_info.RenderPass = RenderPass::Get<VulkanRenderPass>(renderPass)->GetRenderPass();
	init_info.Subpass = 0;
	init_info.MinImageCount = 3;
	init_info.ImageCount = 3;
	init_info.MSAASamples = VK_SAMPLE_COUNT_1_BIT;
	init_info.CheckVkResultFn = CheckVulkanResult;

	ImGui_ImplVulkan_Init(&init_info);
}

VulkanDebugGUI::~VulkanDebugGUI()
{
	ImGui_ImplVulkan_Shutdown();
	vkDestroyDescriptorPool(m_RenderContext->GetDevice(), m_ImguiPool, nullptr);
}

void VulkanDebugGUI::BeginFrame()
{
	main_draw_data = nullptr;

	ImGui_ImplVulkan_NewFrame();
	m_RenderContext->GetViewport()->ImGuiNewFrame();
	ImGui::NewFrame();
}

void VulkanDebugGUI::EndFrame()
{
	ImGui::Render();
	main_draw_data = ImGui::GetDrawData();
}

void VulkanDebugGUI::Render(const std::shared_ptr<CommandQueue>& commandQueue)
{
	auto c = CommandQueue::Get<VulkanCommandQueue>(commandQueue);
	ImGui_ImplVulkan_RenderDrawData(main_draw_data, c->GetCommandBuffers()[m_RenderContext->GetCurrentFrame()]);
}
