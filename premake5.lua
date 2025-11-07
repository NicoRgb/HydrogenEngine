-- premake5.lua

workspace "HydrogenEngine"
	configurations { "Debug", "Release", "Dist" }
	platforms { "Win64" }
	architecture "x86_64" 
	startproject "HydrogenRuntime"

	flags { "MultiProcessorCompile" }

outputdir = "%{cfg.buildcfg}-%{cfg.system}-%{cfg.architecture}"

include "HydrogenEngine"
include "HydrogenEditor"
include "HydrogenRuntime"
include "HydrogenTools"
