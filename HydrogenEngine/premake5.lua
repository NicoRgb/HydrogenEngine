project "HydrogenEngine"
	kind "StaticLib"
	language "C++"
	cppdialect "C++20"
	characterset "MBCS"

	targetdir ("%{wks.location}/bin/" .. outputdir)
	objdir ("%{wks.location}/bin-int/" .. outputdir)

	files { 
		"Include/**.h", "Include/**.hpp", "Source/**.cpp",
	}

	includedirs { 
		"Include",
		"%{wks.location}/Extern/spdlog/include",
		"%{wks.location}/Extern/glm",
		"%{wks.location}/Extern/json/single_include/nlohmann",
	}

	if _OPTIONS["with-imgui"] then
		includedirs { "%{wks.location}/Extern/imgui-docking" }
		files {
			"%{wks.location}/Extern/imgui-docking/imgui.cpp",
			"%{wks.location}/Extern/imgui-docking/imgui_draw.cpp",
			"%{wks.location}/Extern/imgui-docking/imgui_tables.cpp",
			"%{wks.location}/Extern/imgui-docking/imgui_widgets.cpp",
			"%{wks.location}/Extern/imgui-docking/backends/imgui_impl_vulkan.cpp",
			"%{wks.location}/Extern/imgui-docking/backends/imgui_impl_vulkan.h",
			"%{wks.location}/Extern/imgui-docking/backends/imgui_impl_win32.cpp",
			"%{wks.location}/Extern/imgui-docking/backends/imgui_impl_win32.h"
		}
		defines { "HY_WITH_IMGUI" }
	else
		defines { "HY_NO_IMGUI" }
	end

	filter "system:windows"
		defines { "HY_SYSTEM_WINDOWS" }
		includedirs { "$(VULKAN_SDK)/Include" }
		libdirs { "$(VULKAN_SDK)/Lib" }
		links { "vulkan-1" }

	filter "system:macosx"
		defines { "HY_SYSTEM_MACOS" }
		-- Adjust path if Vulkan SDK is installed somewhere else
		includedirs { os.getenv("HOME") .. "/VulkanSDK/1.4.321.0/macOS/include" }
		libdirs { os.getenv("HOME") .. "/VulkanSDK/1.4.321.0/macOS/lib" }
		links { "vulkan" }
		linkoptions { "-rpath $(HOME)/VulkanSDK/1.4.321.0/macOS/lib" }
		-- buildoptions { "-fobjc-arc" }

	filter "configurations:Debug"
		defines { "HY_DEBUG" }
		optimize "Off"
		symbols "On"
		links { "shaderc_shared" }

	filter "configurations:Release"
		defines { "HY_NDEBUG", "NDEBUG" }
		optimize "On"
		symbols "On"
		links { "shaderc_shared" }

	filter "configurations:Dist"
		defines { "HY_DIST", "HY_NDEBUG", "NDEBUG" }
		optimize "On"
		symbols "Off"
		links { "shaderc_shared" }

	filter "action:vs*"
		buildoptions { "/utf-8" }
