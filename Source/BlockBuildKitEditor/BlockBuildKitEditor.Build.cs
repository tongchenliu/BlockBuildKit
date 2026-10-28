// Copyright (c) 2026 tongchen liu. MIT License, see LICENSE.

using UnrealBuildTool;

public class BlockBuildKitEditor : ModuleRules
{
	public BlockBuildKitEditor(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PrivateDependencyModuleNames.AddRange(new string[]
		{
			"Core", "CoreUObject", "Engine", "Slate", "SlateCore", "UnrealEd", "ToolMenus", "Json", "Projects", "BlockBuildKit"
		});
	}
}
