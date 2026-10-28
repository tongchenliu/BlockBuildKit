// Copyright (c) 2026 tongchen liu. MIT License, see LICENSE.

using UnrealBuildTool;

public class BlockBuildKit : ModuleRules
{
	public BlockBuildKit(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(new string[] { "Core", "CoreUObject", "Engine", "InputCore", "DeveloperSettings" });
		PrivateDependencyModuleNames.AddRange(new string[] { "Slate", "SlateCore", "Json", "AssetRegistry" });
	}
}
