// Copyright (c) 2026 tongchen liu. MIT License, see LICENSE.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "GameFramework/SaveGame.h"
#include "Subsystems/WorldSubsystem.h"
#include "BlockBuildSubsystem.generated.h"

class APlayerController;
class UStaticMesh;
class UStaticMeshComponent;
class UGameViewportClient;
class SWidget;
class STextBlock;
class SBorder;
class SScrollBox;
class SWrapBox;
class SImage;
class USceneCaptureComponent2D;
class UTextureRenderTarget2D;
class UTexture2D;
struct FSlateBrush;

/** One placeable static mesh: found under the piece folders set in Project Settings > Plugins > Block Build Kit, or an engine basic shape. */
struct FBlockBuildItem
{
	FString Path;        // object path, e.g. /Game/BuildPieces/Walls/SM_Wall.SM_Wall
	FString Name;        // asset name (SM_ prefix stripped)
	FString Category;    // first sub-folder below the piece folder (inventory tab)
	FVector Min = FVector::ZeroVector;
	FVector Max = FVector::ZeroVector;
	bool bBounds = false;  // Min / Max are filled the first time the mesh is loaded
};

/** A placed piece. */
USTRUCT()
struct FBlockBuildPiece
{
	GENERATED_BODY()

	UPROPERTY()
	FString MeshPath;

	UPROPERTY()
	FTransform Transform;
};

/** Everything the player has built (save slot from the plugin settings, default "BlockBuild"; loaded and re-spawned on every start). */
UCLASS()
class BLOCKBUILDKIT_API UBlockBuildSaveGame : public USaveGame
{
	GENERATED_BODY()

public:
	UPROPERTY()
	TArray<FBlockBuildPiece> Pieces;

	UPROPERTY()
	TArray<FString> Hotbar;
};

/**
 * Minecraft-style creative build mode. In play, the toggle key (default F) turns it on and off.
 * The hotbar (9 slots, keys 1-9 / mouse wheel) holds pieces picked from the inventory (I: categories on the left, pieces on the
 * right; click = put into the selected slot). Right mouse places the ghost at the aimed spot, snapped to the grid (C cycles
 * 25 / 50 / 100 / 150 cm) and to 45 degree steps of yaw (Q / E); PageUp / PageDown lift or lower it by 25 cm, Home resets;
 * N toggles edge snapping to neighbouring pieces; left mouse removes the aimed piece; Z undoes the last placement.
 * Pieces live in a transient actor, are saved to a SaveGame slot after every change and rebuilt on start.
 * In the editor, Tools > Block Build Kit > Bake build into level turns them into normal Static Mesh Actors.
 * `blockbuild export <name>` writes them to Saved/BlockBuild/<name>.json.
 */
UCLASS()
class BLOCKBUILDKIT_API UBlockBuildSubsystem : public UTickableWorldSubsystem
{
	GENERATED_BODY()

public:
	static UBlockBuildSubsystem* Get(const UObject* WorldContext);
	/** True while build mode is on. Check it in your own fire / interact logic so a left click removes a piece instead of shooting. */
	UFUNCTION(BlueprintPure, Category = "Block Build Kit", meta = (WorldContext = "WorldContextObject"))
	static bool IsBuildModeActive(const UObject* WorldContextObject);

	/** Turn build mode on or off from Blueprint (same as pressing the toggle key). */
	UFUNCTION(BlueprintCallable, Category = "Block Build Kit", meta = (WorldContext = "WorldContextObject"))
	static void SetBuildModeActive(const UObject* WorldContextObject, bool bOn);

	bool IsActive() const { return bActive; }
	bool IsInventoryOpen() const { return bInventoryOpen; }

	void SetActive(bool bOn);
	void OpenInventory();
	void CloseInventory();
	void SelectSlot(int32 Slot);
	/** Scans the piece folders the first time build mode is turned on (or a console command needs the list). */
	void BuildCatalogIfNeeded();

	// console test hooks (blockbuild pick / rot / place / remove / cat) so the mode can be exercised without real key presses
	void TestPick(int32 InSlot, const FString& NamePart);
	void TestSetYaw(float InYaw);
	void TestPlace();
	void TestRemove();
	void TestCategory(int32 CatIndex);

	int32 ExportJson(const FString& Name, FString& OutPath) const;
	void ClearAll();
	int32 NumPieces() const { return Pieces.Num(); }
	FString DebugState() const;

	virtual void OnWorldBeginPlay(UWorld& InWorld) override;
	virtual void Deinitialize() override;
	virtual void Tick(float DeltaTime) override;
	virtual TStatId GetStatId() const override { RETURN_QUICK_DECLARE_CYCLE_STAT(UBlockBuildSubsystem, STATGROUP_Tickables); }

protected:
	virtual bool DoesSupportWorldType(const EWorldType::Type WorldType) const override;

private:
	void LoadCatalog();
	void LoadSave();
	void WriteSave();
	void EnsureActor();
	UStaticMeshComponent* SpawnPieceComponent(const FBlockBuildPiece& Piece);
	void RebuildAll();
	void UpdateGhost(APlayerController* PC);
	void Place();
	void RemoveAimed(APlayerController* PC);
	void Undo();
	const FBlockBuildItem* SlotItem(int32 Slot) const;
	UStaticMesh* LoadMesh(const FString& Path);
	FTransform ComposeTransform(const FBlockBuildItem& Item, const FVector& Hit) const;
	/** World AABB of Item placed at P with yaw (corners of the local box rotated). */
	static FBox WorldBox(const FBlockBuildItem& Item, const FVector& P, float InYaw);
	/** Edge snapping: slides P so the ghost box meets the nearest neighbouring piece edge (both X and Y), within SnapTol. */
	void SnapToNeighbours(const FBlockBuildItem& Item, FVector& P, float InYaw) const;

	// thumbnails (runtime scene capture, base colour, a few per frame while the inventory is open)
	void EnsureCaptureRig();
	void PumpThumbnails();
	const FSlateBrush* ThumbBrush(const FString& Path);
	void QueueThumb(const FString& Path);

	// UI
	void EnsureWidgets(UGameViewportClient* VP);
	void RemoveWidgets();
	void RefreshHotbar();
	void RefreshStatus(const FString& Extra);
	void FillInventory();
	void ShowCategory(int32 CatIndex);

	TArray<FBlockBuildItem> Catalog;
	TArray<FString> Categories;
	TMap<FString, int32> CatalogIndex;   // path -> catalog index
	TArray<FBlockBuildPiece> Pieces;
	UPROPERTY(Transient)
	TArray<TObjectPtr<UStaticMeshComponent>> PieceComps;
	UPROPERTY(Transient)
	TObjectPtr<AActor> BuildActor;
	UPROPERTY(Transient)
	TObjectPtr<UStaticMeshComponent> Ghost;
	UPROPERTY(Transient)
	TMap<FString, TObjectPtr<UStaticMesh>> MeshCache;
	UPROPERTY(Transient)
	TObjectPtr<AActor> CaptureRig;
	UPROPERTY(Transient)
	TObjectPtr<USceneCaptureComponent2D> Capture;
	UPROPERTY(Transient)
	TObjectPtr<UStaticMeshComponent> CaptureMesh;
	UPROPERTY(Transient)
	TMap<FString, TObjectPtr<UTexture2D>> Thumbs;
	UPROPERTY(Transient)
	TObjectPtr<UTextureRenderTarget2D> ThumbRT;
	TMap<FString, TSharedPtr<FSlateBrush>> ThumbBrushes;
	TArray<FString> ThumbQueue;

	TArray<FString> Hotbar;    // 9 catalog paths ("" = empty)
	int32 Slot = 0;
	float Yaw = 0.0f;
	float ZOffset = 0.0f;
	int32 GridIndex = 1;       // 25 / 50 / 100 / 150
	bool bEdgeSnap = true;     // N toggles
	bool bActive = false;
	bool bInventoryOpen = false;
	bool bGhostValid = false;
	FTransform GhostTransform;
	int32 CurCategory = 0;
	bool bWidgets = false;
	double LastSaveRequest = -1.0;
	bool bSaveDirty = false;
	bool bCatalogBuilt = false;

	TSharedPtr<SWidget> HotbarRoot;
	TArray<TSharedPtr<SBorder>> SlotBorders;
	TArray<TSharedPtr<STextBlock>> SlotTexts;
	TArray<TSharedPtr<SImage>> SlotImages;
	TSharedPtr<STextBlock> StatusText;
	TSharedPtr<SWidget> InventoryRoot;
	TSharedPtr<SScrollBox> CategoryBox;
	TSharedPtr<SWrapBox> ItemBox;
	TSharedPtr<STextBlock> InventoryTitle;
};
