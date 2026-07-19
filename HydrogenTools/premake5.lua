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
	links { "HydrogenEngine", "assimp-vc145-mtd" }

	files { "Include/**.h", "Include/**.hpp", "Source/**.cpp" }

	postbuildcommands { "{COPY} %{wks.location}Extern/bin/assimp/assimp-vc145-mtd.dll %{cfg.targetdir}" }

	filter "configurations:Debug"
		defines { "HY_DEBUG" }
		optimize "Off"
		symbols "On"

	filter "configurations:Release"
		defines { "HY_NDEBUG", "NDEBUG" }
		optimize "On"
		symbols "On"

	filter "configurations:Dist"
		defines { "HY_DIST", "HY_NDEBUG", "NDEBUG" }
		optimize "On"
		symbols "Off"

	filter "action:vs*"
		buildoptions { "/utf-8" }
