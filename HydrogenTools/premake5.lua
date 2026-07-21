-- premake5.lua

project "HydrogenTools"
	kind "ConsoleApp"
	language "C++"
	cppdialect "C++20"

	targetdir ("%{wks.location}/bin/" .. outputdir)
	objdir ("%{wks.location}/bin-int/" .. outputdir)

	includedirs { "%{wks.location}/HydrogenEngine/Include", "Include" }
	externalincludedirs { "%{wks.location}/Extern/spdlog/include", "%{wks.location}/Extern/glm", "%{wks.location}/Extern/imgui-docking", "%{wks.location}/Extern/ImTextEdit",
	"%{wks.location}/Extern/ImGuizmo", "%{wks.location}/Extern/json/single_include/nlohmann", "%{wks.location}/Extern/entt/single_include",
	"%{wks.location}/Extern/sol2/include", "%{wks.location}/Extern/bin/lua", "$(VULKAN_SDK)/Include",
	"%{wks.location}/Extern/bin/reactphysics3d/include", "%{wks.location}/Extern/bin/assimp/include" }

	libdirs { "%{wks.location}/Extern/bin/assimp" }
	links { "HydrogenEngine" }

	files { "Include/**.h", "Include/**.hpp", "Source/**.cpp", "Resources/resource.rc" }

	filter "configurations:Debug"
		defines { "HY_DEBUG" }
		optimize "Off"
		symbols "On"
		links { "assimp-vc145-mtd" }
		postbuildcommands { "{COPY} %{wks.location}Extern/bin/assimp/assimp-vc145-mtd.dll %{cfg.targetdir}" }

	filter "configurations:Release"
		kind "WindowedApp"
		entrypoint "mainCRTStartup"
		defines { "HY_NDEBUG", "NDEBUG" }
		optimize "On"
		symbols "On"
		links { "assimp-vc145-mt" }
		postbuildcommands { "{COPY} %{wks.location}Extern/bin/assimp/assimp-vc145-mt.dll %{cfg.targetdir}" }

	filter "action:vs*"
		buildoptions { "/utf-8" }
