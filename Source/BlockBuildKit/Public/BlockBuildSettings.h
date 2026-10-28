// Copyright (c) 2026 tongchen liu. MIT License, see LICENSE.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"
#include "Engine/EngineTypes.h"
#include "InputCoreTypes.h"
#include "BlockBuildSettings.generated.h"

/** Project Settings > Plugins > Block Build Kit. Saved to Config/DefaultGame.ini. */
UCLASS(Config = Game, DefaultConfig, meta = (DisplayName = "Block Build Kit"))
class BLOCKBUILDKIT_API UBlockBuildSettings : public UDeveloperSettings
{
	GENERATED_BODY()

public:
	UBlockBuildSettings();

	virtual FName GetCategoryName() const override { return TEXT("Plugins"); }

	/** Turn the whole build mode off without removing the plugin. */
	UPROPERTY(Config, EditAnywhere, Category = "General")
	bool bEnabled = true;

	/** Show the in-game build UI in Chinese. */
	UPROPERTY(Config, EditAnywhere, Category = "General", meta = (DisplayName = "Chinese UI (中文界面)"))
	bool bChineseUI = false;

	/** Key that turns build mode on and off in play. */
	UPROPERTY(Config, EditAnywhere, Category = "General")
	FKey ToggleKey;

	/** SaveGame slot the build is stored in (Saved/SaveGames/<name>.sav). */
	UPROPERTY(Config, EditAnywhere, Category = "General")
	FString SaveSlotName = TEXT("BlockBuild");

	/**
	 * Folders whose Static Meshes show up in the build inventory. The first sub-folder becomes the tab,
	 * e.g. /Game/BuildPieces/Walls/SM_Wall_A goes into the "Walls" tab.
	 * For a packaged game, also add these folders to Project Settings > Packaging > Additional Asset Directories to Cook.
	 */
	UPROPERTY(Config, EditAnywhere, Category = "Pieces", meta = (LongPackageName))
	TArray<FDirectoryPath> PieceFolders;

	/** Also offer the engine's Cube / Plane / Cylinder / Cone / Sphere, so the kit works before you add any meshes. */
	UPROPERTY(Config, EditAnywhere, Category = "Pieces")
	bool bIncludeBasicShapes = true;

	/**
	 * Record how you build (place / remove / undo / rotate / grid / height, with timestamps) to Saved/BlockBuild/Sessions.
	 * The files stay on this computer and nothing is uploaded. While it is on, the build status line shows "REC".
	 * Tools > Block Build Kit > Export build for sharing puts them into the file you can choose to share.
	 */
	UPROPERTY(Config, EditAnywhere, Category = "Sharing", meta = (DisplayName = "Record build sessions (local only)"))
	bool bRecordSessions = false;

	/** Page opened by the "Share it" link after exporting a build for sharing. */
	UPROPERTY(Config, EditAnywhere, Category = "Sharing")
	FString ShareSubmitURL = TEXT("https://github.com/tongchenliu/BlockBuildKit/issues/new?template=share-build.yml");
};
