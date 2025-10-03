-- premake5.lua

project "HydrogenEngine"
	kind "StaticLib"
	language "C++"
	cppdialect "C++20"
	characterset ( "MBCS" )

	targetdir ("%{wks.location}/bin/" .. outputdir)
	objdir ("%{wks.location}/bin-int/" .. outputdir)

	includedirs { "Include", "%{wks.location}/Extern/spdlog/include", "%{wks.location}/Extern/glm", "%{wks.location}/Extern/json/single_include/nlohmann", "%{wks.location}/Extern/imgui-docking", "$(VULKAN_SDK)/Include" }
	files { "Include/**.h", "Include/**.hpp", "Source/**.cpp",
			"%{wks.location}/Extern/imgui-docking/imgui.cpp", "%{wks.location}/Extern/imgui-docking/imgui_draw.cpp", "%{wks.location}/Extern/imgui-docking/imgui_tables.cpp", "%{wks.location}/Extern/imgui-docking/imgui_widgets.cpp",
			"%{wks.location}/Extern/imgui-docking/backends/imgui_impl_win32.h", "%{wks.location}/Extern/imgui-docking/backends/imgui_impl_win32.cpp",
			"%{wks.location}/Extern/imgui-docking/backends/imgui_impl_vulkan.h", "%{wks.location}/Extern/imgui-docking/backends/imgui_impl_vulkan.cpp" }

	libdirs { "$(VULKAN_SDK)/Lib" }
	links { "vulkan-1" }

	filter "system:windows"
		defines { "HY_SYSTEM_WINDOWS" }

	filter "system:macosx"
		defines { "HY_SYSTEM_MACOS" }

	filter "configurations:Debug"
		defines { "HY_DEBUG" }
		optimize "Off"
		symbols "On"
		links { "shaderc_combinedd" }

	filter "configurations:Release"
		defines { "HY_NDEBUG", "NDEBUG" }
		optimize "On"
		symbols "On"
		links { "shaderc_combined" }

	filter "configurations:Dist"
		defines { "HY_DIST", "HY_NDEBUG", "NDEBUG" }
		optimize "On"
		symbols "Off"
		links { "shaderc_combined" }

	filter "action:vs*"
		buildoptions { "/utf-8" }
