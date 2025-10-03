workspace "HydrogenEngine"
	configurations { "Debug", "Release", "Dist" }
	platforms { "x86_64", "arm64" }
	startproject "HydrogenRuntime"

	flags { "MultiProcessorCompile" }

outputdir = "%{cfg.buildcfg}-%{cfg.system}-%{cfg.architecture}"

filter "platforms:x86_64"
	architecture "x86_64"

filter "platforms:arm64"
	architecture "ARM64"

group "Engine"
	include "HydrogenEngine"
	include "HydrogenEditor"
	include "HydrogenRuntime"
