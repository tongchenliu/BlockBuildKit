// Copyright (c) 2026 tongchen liu. MIT License, see LICENSE.

#include "BlockBuildSettings.h"

UBlockBuildSettings::UBlockBuildSettings()
{
	ToggleKey = EKeys::F;
	FDirectoryPath Default;
	Default.Path = TEXT("/Game/BuildPieces");
	PieceFolders.Add(Default);
}
