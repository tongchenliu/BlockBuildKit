// Copyright (c) 2026 tongchen liu. MIT License, see LICENSE.

#include "BlockBuildSubsystem.h"
#include "BlockBuildSettings.h"
#include "BlockBuildKit.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "AssetRegistry/IAssetRegistry.h"
#include "AssetRegistry/ARFilter.h"
#include "Interfaces/IPluginManager.h"
#include "Misc/EngineVersion.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "Engine/StaticMesh.h"
#include "Engine/GameViewportClient.h"
#include "Components/StaticMeshComponent.h"
#include "Components/SceneCaptureComponent2D.h"
#include "Engine/TextureRenderTarget2D.h"
#include "Engine/Texture2D.h"
#include "TextureResource.h"
#include "Widgets/Images/SImage.h"
#include "Camera/PlayerCameraManager.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/Pawn.h"
#include "Kismet/GameplayStatics.h"
#include "Materials/MaterialInterface.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "UObject/StrongObjectPtr.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "HAL/FileManager.h"
#include "HAL/IConsoleManager.h"
#include "HAL/PlatformTime.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"
#include "Framework/Application/SlateApplication.h"
#include "Brushes/SlateColorBrush.h"
#include "Brushes/SlateRoundedBoxBrush.h"
#include "Styling/CoreStyle.h"
#include "Styling/SlateTypes.h"
#include "Widgets/SCompoundWidget.h"
#include "Widgets/SOverlay.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Widgets/Layout/SWrapBox.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Text/STextBlock.h"

// ---------------------------------------------------------------------------------------------------------------------
// Settings helpers (Project Settings > Plugins > Block Build Kit)

static const UBlockBuildSettings* BBSettings() { return GetDefault<UBlockBuildSettings>(); }
static bool BBZh() { const UBlockBuildSettings* S = BBSettings(); return S && S->bChineseUI; }
static FString BBT(const TCHAR* En, const TCHAR* Zh) { return BBZh() ? FString(Zh) : FString(En); }
static FString BBSaveSlot()
{
	const UBlockBuildSettings* S = BBSettings();
	return (S && !S->SaveSlotName.IsEmpty()) ? S->SaveSlotName : FString(TEXT("BlockBuild"));
}
static FKey BBToggleKey()
{
	const UBlockBuildSettings* S = BBSettings();
	return (S && S->ToggleKey.IsValid()) ? S->ToggleKey : EKeys::F;
}
static FString BBPluginVersion()
{
	const TSharedPtr<IPlugin> Plugin = IPluginManager::Get().FindPlugin(TEXT("BlockBuildKit"));
	return Plugin.IsValid() ? Plugin->GetDescriptor().VersionName : FString(TEXT("unknown"));
}
static TArray<TSharedPtr<FJsonValue>> BBVec(const FVector& V)
{
	TArray<TSharedPtr<FJsonValue>> A;
	A.Add(MakeShared<FJsonValueNumber>(FMath::RoundToDouble(V.X * 10.0) / 10.0));
	A.Add(MakeShared<FJsonValueNumber>(FMath::RoundToDouble(V.Y * 10.0) / 10.0));
	A.Add(MakeShared<FJsonValueNumber>(FMath::RoundToDouble(V.Z * 10.0) / 10.0));
	return A;
}
static TSharedPtr<FJsonObject> BBPieceJson(const FString& Mesh, const FTransform& T)
{
	TSharedPtr<FJsonObject> O = MakeShared<FJsonObject>();
	O->SetStringField(TEXT("mesh"), Mesh);
	O->SetArrayField(TEXT("loc"), BBVec(T.GetLocation()));
	O->SetNumberField(TEXT("yaw"), FMath::RoundToDouble(T.Rotator().Yaw * 10.0) / 10.0);
	return O;
}
static FString BBHelpLine()
{
	const FString Key = BBToggleKey().GetDisplayName().ToString();
	return BBZh()
		? FString::Printf(TEXT("I 背包   1-9/滚轮 选格   右键 放置   左键 拆除   Q/E 转45°   C 网格   N 贴边吸附   PgUp/PgDn 升降   Home 归零   Z 撤销   %s 退出"), *Key)
		: FString::Printf(TEXT("I inventory   1-9/wheel slot   RMB place   LMB remove   Q/E rotate 45°   C grid   N edge snap   PgUp/PgDn raise/lower   Home reset   Z undo   %s exit"), *Key);
}

namespace BlockBuildStyle
{
	static FSlateFontInfo Font(const TCHAR* Weight, int32 Size) { return FCoreStyle::GetDefaultFontStyle(Weight, Size); }
	static const FLinearColor TextMain(0.93f, 0.94f, 0.96f, 1.f);
	static const FLinearColor TextDim(0.62f, 0.66f, 0.72f, 1.f);
	static const FSlateBrush* SlotBg()   { static const FSlateRoundedBoxBrush B(FLinearColor(0.f, 0.f, 0.f, 0.62f), 6.f); return &B; }
	static const FSlateBrush* SlotSel()  { static const FSlateRoundedBoxBrush B(FLinearColor(0.95f, 0.82f, 0.35f, 0.9f), 6.f); return &B; }
	static const FSlateBrush* SlotIn()   { static const FSlateRoundedBoxBrush B(FLinearColor(0.08f, 0.09f, 0.12f, 0.96f), 5.f); return &B; }
	static const FSlateBrush* Dim()      { static const FSlateColorBrush B(FLinearColor(0.f, 0.f, 0.f, 0.55f)); return &B; }
	static const FSlateBrush* Panel()    { static const FSlateRoundedBoxBrush B(FLinearColor(0.045f, 0.05f, 0.065f, 0.97f), 10.f); return &B; }
	static const FSlateBrush* Side()     { static const FSlateRoundedBoxBrush B(FLinearColor(0.028f, 0.031f, 0.042f, 1.f), 10.f); return &B; }
	static const FSlateBrush* Status()   { static const FSlateRoundedBoxBrush B(FLinearColor(0.f, 0.f, 0.f, 0.5f), 6.f); return &B; }

	static const FButtonStyle& ItemButton()
	{
		static FButtonStyle Style = []()
		{
			FButtonStyle S;
			S.SetNormal(FSlateRoundedBoxBrush(FLinearColor(0.12f, 0.135f, 0.17f, 1.f), 6.f));
			S.SetHovered(FSlateRoundedBoxBrush(FLinearColor(0.19f, 0.22f, 0.29f, 1.f), 6.f));
			S.SetPressed(FSlateRoundedBoxBrush(FLinearColor(0.11f, 0.32f, 0.72f, 1.f), 6.f));
			S.SetNormalPadding(FMargin(8.f, 6.f));
			S.SetPressedPadding(FMargin(8.f, 7.f, 8.f, 5.f));
			return S;
		}();
		return Style;
	}
	static const FButtonStyle& CatButton(bool bSelected)
	{
		static FButtonStyle Normal = []()
		{
			FButtonStyle S;
			S.SetNormal(FSlateRoundedBoxBrush(FLinearColor(1.f, 1.f, 1.f, 0.0f), 6.f));
			S.SetHovered(FSlateRoundedBoxBrush(FLinearColor(1.f, 1.f, 1.f, 0.07f), 6.f));
			S.SetPressed(FSlateRoundedBoxBrush(FLinearColor(1.f, 1.f, 1.f, 0.12f), 6.f));
			S.SetNormalPadding(FMargin(12.f, 8.f));
			S.SetPressedPadding(FMargin(12.f, 9.f, 12.f, 7.f));
			return S;
		}();
		static FButtonStyle Sel = []()
		{
			FButtonStyle S = Normal;
			S.SetNormal(FSlateRoundedBoxBrush(FLinearColor(0.11f, 0.32f, 0.72f, 0.9f), 6.f));
			S.SetHovered(FSlateRoundedBoxBrush(FLinearColor(0.14f, 0.38f, 0.8f, 0.95f), 6.f));
			return S;
		}();
		return bSelected ? Sel : Normal;
	}
}

// ---------------------------------------------------------------------------------------------------------------------
// Inventory panel: takes keyboard focus so I / Esc / Tab close it and 1-9 pick the hotbar slot while it is open.

class SBlockBuildInventory : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SBlockBuildInventory) {}
		SLATE_DEFAULT_SLOT(FArguments, Content)
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs, UBlockBuildSubsystem* InOwner)
	{
		Owner = InOwner;
		ChildSlot[InArgs._Content.Widget];
	}

	virtual bool SupportsKeyboardFocus() const override { return true; }

	virtual FReply OnKeyDown(const FGeometry& MyGeometry, const FKeyEvent& InKeyEvent) override
	{
		const FKey K = InKeyEvent.GetKey();
		if (K == EKeys::I || K == EKeys::Escape || K == EKeys::Tab || K == BBToggleKey())
		{
			if (Owner.IsValid())
			{
				Owner->CloseInventory();
			}
			return FReply::Handled();
		}
		static const FKey Nums[9] = { EKeys::One, EKeys::Two, EKeys::Three, EKeys::Four, EKeys::Five, EKeys::Six, EKeys::Seven, EKeys::Eight, EKeys::Nine };
		for (int32 I = 0; I < 9; ++I)
		{
			if (K == Nums[I])
			{
				if (Owner.IsValid())
				{
					Owner->SelectSlot(I);
				}
				return FReply::Handled();
			}
		}
		return FReply::Unhandled();
	}

private:
	TWeakObjectPtr<UBlockBuildSubsystem> Owner;
};

// ---------------------------------------------------------------------------------------------------------------------

static const float GBBGridSteps[] = { 25.f, 50.f, 100.f, 150.f };
static const int32 GBBNumGrid = 4;
static const int32 GBBHotbarSlots = 9;
static const int32 GBBThumbSize = 96;

UBlockBuildSubsystem* UBlockBuildSubsystem::Get(const UObject* WorldContext)
{
	if (!WorldContext || !GEngine)
	{
		return nullptr;
	}
	UWorld* World = GEngine->GetWorldFromContextObject(WorldContext, EGetWorldErrorMode::ReturnNull);
	return World ? World->GetSubsystem<UBlockBuildSubsystem>() : nullptr;
}

bool UBlockBuildSubsystem::IsBuildModeActive(const UObject* WorldContextObject)
{
	const UBlockBuildSubsystem* B = Get(WorldContextObject);
	return B && B->bActive;
}

void UBlockBuildSubsystem::SetBuildModeActive(const UObject* WorldContextObject, bool bOn)
{
	if (UBlockBuildSubsystem* B = Get(WorldContextObject))
	{
		B->SetActive(bOn);
	}
}

bool UBlockBuildSubsystem::DoesSupportWorldType(const EWorldType::Type WorldType) const
{
	const UBlockBuildSettings* S = BBSettings();
	if (S && !S->bEnabled)
	{
		return false;
	}
	return WorldType == EWorldType::Game || WorldType == EWorldType::PIE;
}

void UBlockBuildSubsystem::OnWorldBeginPlay(UWorld& InWorld)
{
	Super::OnWorldBeginPlay(InWorld);
	Hotbar.Init(TEXT(""), GBBHotbarSlots);
	MapName = UWorld::RemovePIEPrefix(InWorld.GetMapName());
	// the piece list is scanned the first time build mode is turned on (BuildCatalogIfNeeded), so worlds that never build pay nothing
	LoadSave();
	RebuildAll();
	UE_LOG(LogBlockBuild, Log, TEXT("Block Build: ready, %d saved pieces. Press %s in play to build."), Pieces.Num(), *BBToggleKey().GetDisplayName().ToString());
}

void UBlockBuildSubsystem::Deinitialize()
{
	if (bSaveDirty)
	{
		WriteSave();
	}
	FlushSession();
	RemoveWidgets();
	Super::Deinitialize();
}

// ------------------------------------------------------------------ catalog / save ------------------------------------

void UBlockBuildSubsystem::LoadCatalog()
{
	Catalog.Reset();
	Categories.Reset();
	CatalogIndex.Reset();
	const UBlockBuildSettings* Settings = BBSettings();
	TSet<FString> Seen;
	FString ShapesCat;

	// 1) the engine basic shapes: always there, so the kit works in an empty project with no assets of your own
	if (!Settings || Settings->bIncludeBasicShapes)
	{
		static const TCHAR* Shapes[] = { TEXT("Cube"), TEXT("Plane"), TEXT("Cylinder"), TEXT("Cone"), TEXT("Sphere") };
		ShapesCat = BBT(TEXT("Basic Shapes"), TEXT("基础形状"));
		for (const TCHAR* Shape : Shapes)
		{
			FBlockBuildItem It;
			It.Path = FString::Printf(TEXT("/Engine/BasicShapes/%s.%s"), Shape, Shape);
			It.Name = Shape;
			It.Category = ShapesCat;
			Seen.Add(It.Path);
			Catalog.Add(MoveTemp(It));
		}
	}

	// 2) every Static Mesh under the piece folders; the first sub-folder below a piece folder becomes its inventory tab
	TArray<FString> Roots;
	if (Settings)
	{
		for (const FDirectoryPath& D : Settings->PieceFolders)
		{
			FString P = D.Path;
			P.TrimStartAndEndInline();
			while (P.EndsWith(TEXT("/")))
			{
				P.LeftChopInline(1);
			}
			if (!P.IsEmpty())
			{
				Roots.AddUnique(P);
			}
		}
	}
	if (Roots.Num() > 0)
	{
		IAssetRegistry& AR = FAssetRegistryModule::GetRegistry();
#if WITH_EDITOR
		AR.ScanPathsSynchronous(Roots, false);
#endif
		for (const FString& RootPath : Roots)
		{
			FARFilter Filter;
			Filter.PackagePaths.Add(FName(*RootPath));
			Filter.bRecursivePaths = true;
			Filter.ClassPaths.Add(UStaticMesh::StaticClass()->GetClassPathName());
			Filter.bRecursiveClasses = true;
			TArray<FAssetData> Assets;
			AR.GetAssets(Filter, Assets);
			const FString RootName = FPaths::GetCleanFilename(RootPath);
			for (const FAssetData& AD : Assets)
			{
				FBlockBuildItem It;
				It.Path = AD.GetObjectPathString();
				if (It.Path.IsEmpty() || Seen.Contains(It.Path))
				{
					continue;
				}
				It.Name = AD.AssetName.ToString();
				if (It.Name.StartsWith(TEXT("SM_"), ESearchCase::IgnoreCase) && It.Name.Len() > 3)
				{
					It.Name.RightChopInline(3);
				}
				FString Rel = AD.PackagePath.ToString();
				Rel.RightChopInline(FMath::Min(RootPath.Len(), Rel.Len()));
				Rel.RemoveFromStart(TEXT("/"));
				FString First, Rest;
				if (Rel.Split(TEXT("/"), &First, &Rest))
				{
					It.Category = First;
				}
				else
				{
					It.Category = Rel.IsEmpty() ? RootName : Rel;
				}
				Seen.Add(It.Path);
				Catalog.Add(MoveTemp(It));
			}
		}
	}

	// tabs: basic shapes first, then the folder names alphabetically; pieces sorted by name inside each tab
	for (const FBlockBuildItem& It : Catalog)
	{
		Categories.AddUnique(It.Category);
	}
	Categories.Sort([&ShapesCat](const FString& A, const FString& B)
	{
		if (A == ShapesCat) { return B != ShapesCat; }
		if (B == ShapesCat) { return false; }
		return A < B;
	});
	Catalog.StableSort([](const FBlockBuildItem& A, const FBlockBuildItem& B) { return A.Name < B.Name; });
	CatalogIndex.Reset();
	for (int32 I = 0; I < Catalog.Num(); ++I)
	{
		CatalogIndex.Add(Catalog[I].Path, I);
	}
}

void UBlockBuildSubsystem::BuildCatalogIfNeeded()
{
	if (bCatalogBuilt)
	{
		return;
	}
	bCatalogBuilt = true;
	const double T0 = FPlatformTime::Seconds();
	LoadCatalog();
	// meshes already loaded for the saved pieces give their bounds right away (edge snapping against them needs it)
	for (FBlockBuildItem& It : Catalog)
	{
		if (const TObjectPtr<UStaticMesh>* Found = MeshCache.Find(It.Path))
		{
			if (*Found)
			{
				const FBox Box = (*Found)->GetBoundingBox();
				It.Min = Box.Min;
				It.Max = Box.Max;
				It.bBounds = true;
			}
		}
	}
	UE_LOG(LogBlockBuild, Log, TEXT("Block Build: %d pieces in %d categories (%.2f s)."), Catalog.Num(), Categories.Num(), FPlatformTime::Seconds() - T0);
}

void UBlockBuildSubsystem::LoadSave()
{
	Pieces.Reset();
	if (!UGameplayStatics::DoesSaveGameExist(BBSaveSlot(), 0))
	{
		return;
	}
	if (UBlockBuildSaveGame* S = Cast<UBlockBuildSaveGame>(UGameplayStatics::LoadGameFromSlot(BBSaveSlot(), 0)))
	{
		Pieces = S->Pieces;
		for (int32 I = 0; I < GBBHotbarSlots && I < S->Hotbar.Num(); ++I)
		{
			Hotbar[I] = S->Hotbar[I];
		}
	}
}

void UBlockBuildSubsystem::WriteSave()
{
	bSaveDirty = false;
	UBlockBuildSaveGame* S = Cast<UBlockBuildSaveGame>(UGameplayStatics::CreateSaveGameObject(UBlockBuildSaveGame::StaticClass()));
	if (!S)
	{
		return;
	}
	S->Pieces = Pieces;
	S->Hotbar = Hotbar;
	if (!UGameplayStatics::SaveGameToSlot(S, BBSaveSlot(), 0))
	{
		UE_LOG(LogBlockBuild, Warning, TEXT("Block Build: save failed."));
	}
}

int32 UBlockBuildSubsystem::ExportJson(const FString& Name, FString& OutPath) const
{
	FString Safe = Name.IsEmpty() ? TEXT("build") : Name;
	Safe = Safe.Replace(TEXT("/"), TEXT("_")).Replace(TEXT("\\"), TEXT("_")).Replace(TEXT(".."), TEXT("_"));
	TArray<TSharedPtr<FJsonValue>> Arr;
	for (const FBlockBuildPiece& P : Pieces)
	{
		TSharedPtr<FJsonObject> O = MakeShared<FJsonObject>();
		O->SetStringField(TEXT("mesh"), P.MeshPath);
		const FVector L = P.Transform.GetLocation();
		const FRotator R = P.Transform.Rotator();
		const FVector S = P.Transform.GetScale3D();
		auto Arr3 = [](double A, double B, double C)
		{
			TArray<TSharedPtr<FJsonValue>> V;
			V.Add(MakeShared<FJsonValueNumber>(A));
			V.Add(MakeShared<FJsonValueNumber>(B));
			V.Add(MakeShared<FJsonValueNumber>(C));
			return V;
		};
		O->SetArrayField(TEXT("loc"), Arr3(L.X, L.Y, L.Z));
		O->SetArrayField(TEXT("rot"), Arr3(R.Pitch, R.Yaw, R.Roll));
		O->SetArrayField(TEXT("scale"), Arr3(S.X, S.Y, S.Z));
		Arr.Add(MakeShared<FJsonValueObject>(O));
	}
	TSharedPtr<FJsonObject> Root = MakeShared<FJsonObject>();
	Root->SetStringField(TEXT("name"), Safe);
	Root->SetStringField(TEXT("exported"), FDateTime::Now().ToString());
	Root->SetNumberField(TEXT("count"), Pieces.Num());
	Root->SetArrayField(TEXT("pieces"), Arr);
	FString Out;
	const TSharedRef<TJsonWriter<>> W = TJsonWriterFactory<>::Create(&Out);
	FJsonSerializer::Serialize(Root.ToSharedRef(), W);

	// Saved/BlockBuild/<name>.json: share it, or keep it as a backup of a build
	const FString Local = FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("BlockBuild"));
	IFileManager::Get().MakeDirectory(*Local, true);
	OutPath = FPaths::ConvertRelativePathToFull(FPaths::Combine(Local, Safe + TEXT(".json")));
	if (!FFileHelper::SaveStringToFile(Out, *OutPath))
	{
		UE_LOG(LogBlockBuild, Warning, TEXT("Block Build: could not write %s"), *OutPath);
		return 0;
	}
	return Pieces.Num();
}

void UBlockBuildSubsystem::ClearAll()
{
	Pieces.Reset();
	RebuildAll();
	WriteSave();
	RefreshStatus(BBT(TEXT("Cleared"), TEXT("已清空")));
}

FString UBlockBuildSubsystem::DebugState() const
{
	return FString::Printf(TEXT("active=%d inv=%d pieces=%d catalog=%d slot=%d yaw=%.0f z=%.0f grid=%.0f"),
		bActive ? 1 : 0, bInventoryOpen ? 1 : 0, Pieces.Num(), Catalog.Num(), Slot + 1, Yaw, ZOffset, GBBGridSteps[GridIndex]);
}

// ------------------------------------------------------------------ world pieces --------------------------------------

void UBlockBuildSubsystem::EnsureActor()
{
	if (BuildActor && IsValid(BuildActor))
	{
		return;
	}
	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}
	FActorSpawnParameters P;
	P.Name = TEXT("BlockBuild_Pieces");
	P.ObjectFlags |= RF_Transient;
	P.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	BuildActor = World->SpawnActor<AActor>(AActor::StaticClass(), FTransform::Identity, P);
	if (!BuildActor)
	{
		return;
	}
#if WITH_EDITOR
	BuildActor->SetActorLabel(TEXT("BlockBuild_Pieces"));
#endif
	USceneComponent* Root = NewObject<USceneComponent>(BuildActor, TEXT("Root"));
	Root->SetMobility(EComponentMobility::Static);
	BuildActor->SetRootComponent(Root);
	Root->RegisterComponent();
	BuildActor->AddInstanceComponent(Root);

	Ghost = NewObject<UStaticMeshComponent>(BuildActor, TEXT("Ghost"));
	Ghost->SetMobility(EComponentMobility::Movable);
	Ghost->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Ghost->SetCastShadow(false);
	Ghost->SetVisibility(false);
	Ghost->SetupAttachment(Root);
	Ghost->RegisterComponent();
}

UStaticMesh* UBlockBuildSubsystem::LoadMesh(const FString& Path)
{
	if (TObjectPtr<UStaticMesh>* Found = MeshCache.Find(Path))
	{
		return *Found;
	}
	UStaticMesh* M = Cast<UStaticMesh>(StaticLoadObject(UStaticMesh::StaticClass(), nullptr, *Path));
	MeshCache.Add(Path, M);
	if (M)
	{
		if (const int32* Idx = CatalogIndex.Find(Path))
		{
			FBlockBuildItem& It = Catalog[*Idx];
			if (!It.bBounds)
			{
				const FBox Box = M->GetBoundingBox();
				It.Min = Box.Min;
				It.Max = Box.Max;
				It.bBounds = true;
			}
		}
	}
	if (!M)
	{
		UE_LOG(LogBlockBuild, Warning, TEXT("Block Build: mesh not found %s"), *Path);
	}
	return M;
}

UStaticMeshComponent* UBlockBuildSubsystem::SpawnPieceComponent(const FBlockBuildPiece& Piece)
{
	EnsureActor();
	UStaticMesh* M = BuildActor ? LoadMesh(Piece.MeshPath) : nullptr;
	if (!M)
	{
		return nullptr;
	}
	UStaticMeshComponent* C = NewObject<UStaticMeshComponent>(BuildActor);
	C->SetMobility(EComponentMobility::Static);
	C->SetStaticMesh(M);
	C->SetCollisionProfileName(TEXT("BlockAll"));
	C->SetupAttachment(BuildActor->GetRootComponent());
	C->SetRelativeTransform(Piece.Transform);
	C->RegisterComponent();
	BuildActor->AddInstanceComponent(C);
	return C;
}

void UBlockBuildSubsystem::RebuildAll()
{
	for (UStaticMeshComponent* C : PieceComps)
	{
		if (C)
		{
			C->DestroyComponent();
		}
	}
	PieceComps.Reset();
	PieceComps.Reserve(Pieces.Num());
	for (const FBlockBuildPiece& P : Pieces)
	{
		PieceComps.Add(SpawnPieceComponent(P));
	}
}

// ------------------------------------------------------------------ mode / input --------------------------------------

void UBlockBuildSubsystem::SetActive(bool bOn)
{
	if (bOn == bActive)
	{
		return;
	}
	if (bOn)
	{
		BuildCatalogIfNeeded();
	}
	bActive = bOn;
	{
		TSharedPtr<FJsonObject> E = MakeShared<FJsonObject>();
		E->SetBoolField(TEXT("on"), bActive);
		Rec(TEXT("mode"), E);
	}
	if (!bActive && bInventoryOpen)
	{
		CloseInventory();
	}
	EnsureActor();
	if (Ghost)
	{
		Ghost->SetVisibility(false);
	}
	bGhostValid = false;
	if (HotbarRoot.IsValid())
	{
		HotbarRoot->SetVisibility(bActive ? EVisibility::HitTestInvisible : EVisibility::Collapsed);
	}
	RefreshHotbar();
	RefreshStatus(TEXT(""));
}

void UBlockBuildSubsystem::OpenInventory()
{
	UWorld* World = GetWorld();
	APlayerController* PC = World ? World->GetFirstPlayerController() : nullptr;
	if (!PC || bInventoryOpen || !InventoryRoot.IsValid())
	{
		return;
	}
	bInventoryOpen = true;
	FillInventory();
	InventoryRoot->SetVisibility(EVisibility::Visible);
	FInputModeUIOnly Mode;
	Mode.SetWidgetToFocus(InventoryRoot);
	Mode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
	PC->SetInputMode(Mode);
	PC->SetShowMouseCursor(true);
	PC->FlushPressedKeys();
	if (FSlateApplication::IsInitialized())
	{
		FSlateApplication::Get().SetKeyboardFocus(InventoryRoot, EFocusCause::SetDirectly);
	}
	if (Ghost)
	{
		Ghost->SetVisibility(false);
	}
}

void UBlockBuildSubsystem::CloseInventory()
{
	UWorld* World = GetWorld();
	APlayerController* PC = World ? World->GetFirstPlayerController() : nullptr;
	if (!bInventoryOpen)
	{
		return;
	}
	bInventoryOpen = false;
	if (InventoryRoot.IsValid())
	{
		InventoryRoot->SetVisibility(EVisibility::Collapsed);
	}
	if (PC)
	{
		PC->SetInputMode(FInputModeGameOnly());
		PC->SetShowMouseCursor(false);
		PC->FlushPressedKeys();
	}
	if (FSlateApplication::IsInitialized())
	{
		FSlateApplication::Get().SetAllUserFocusToGameViewport();
	}
	RefreshHotbar();
}

void UBlockBuildSubsystem::SelectSlot(int32 InSlot)
{
	Slot = ((InSlot % GBBHotbarSlots) + GBBHotbarSlots) % GBBHotbarSlots;
	if (bActive)
	{
		TSharedPtr<FJsonObject> E = MakeShared<FJsonObject>();
		E->SetNumberField(TEXT("slot"), Slot + 1);
		E->SetStringField(TEXT("mesh"), Hotbar.IsValidIndex(Slot) ? Hotbar[Slot] : FString());
		Rec(TEXT("slot"), E);
	}
	RefreshHotbar();
}

const FBlockBuildItem* UBlockBuildSubsystem::SlotItem(int32 InSlot) const
{
	if (!Hotbar.IsValidIndex(InSlot) || Hotbar[InSlot].IsEmpty())
	{
		return nullptr;
	}
	const int32* Idx = CatalogIndex.Find(Hotbar[InSlot]);
	return Idx ? &Catalog[*Idx] : nullptr;
}

void UBlockBuildSubsystem::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
	UWorld* World = GetWorld();
	APlayerController* PC = World ? World->GetFirstPlayerController() : nullptr;
	UGameViewportClient* VP = World ? World->GetGameViewport() : nullptr;
	if (!PC || !VP || !PC->IsLocalController())
	{
		return;
	}
	EnsureWidgets(VP);

	if (bSaveDirty && FPlatformTime::Seconds() - LastSaveRequest > 0.8)
	{
		WriteSave();
	}

	PumpThumbnails();

	if (!bInventoryOpen && PC->WasInputKeyJustPressed(BBToggleKey()))
	{
		SetActive(!bActive);
	}
	if (!bActive)
	{
		return;
	}
	if (bInventoryOpen)
	{
		// the panel has focus: 1-9 are read here through the PC too (UIOnly still updates the key state for polled keys)
		return;
	}

	if (PC->WasInputKeyJustPressed(EKeys::I))
	{
		OpenInventory();
		return;
	}
	static const FKey NumKeys[GBBHotbarSlots] = { EKeys::One, EKeys::Two, EKeys::Three, EKeys::Four, EKeys::Five, EKeys::Six, EKeys::Seven, EKeys::Eight, EKeys::Nine };
	for (int32 I = 0; I < GBBHotbarSlots; ++I)
	{
		if (PC->WasInputKeyJustPressed(NumKeys[I]))
		{
			SelectSlot(I);
		}
	}
	if (PC->WasInputKeyJustPressed(EKeys::MouseScrollUp))
	{
		SelectSlot(Slot - 1);
	}
	if (PC->WasInputKeyJustPressed(EKeys::MouseScrollDown))
	{
		SelectSlot(Slot + 1);
	}
	if (PC->WasInputKeyJustPressed(EKeys::Q))
	{
		Yaw = FMath::Fmod(Yaw - 45.f + 360.f, 360.f);
		RefreshStatus(TEXT(""));
		RecNum(TEXT("yaw"), TEXT("yaw"), Yaw);
	}
	if (PC->WasInputKeyJustPressed(EKeys::E))
	{
		Yaw = FMath::Fmod(Yaw + 45.f, 360.f);
		RefreshStatus(TEXT(""));
		RecNum(TEXT("yaw"), TEXT("yaw"), Yaw);
	}
	if (PC->WasInputKeyJustPressed(EKeys::C))
	{
		GridIndex = (GridIndex + 1) % GBBNumGrid;
		RefreshStatus(TEXT(""));
		RecNum(TEXT("grid"), TEXT("cm"), GBBGridSteps[GridIndex]);
	}
	if (PC->WasInputKeyJustPressed(EKeys::PageUp))
	{
		ZOffset += 25.f;
		RefreshStatus(TEXT(""));
		RecNum(TEXT("height"), TEXT("z"), ZOffset);
	}
	if (PC->WasInputKeyJustPressed(EKeys::PageDown))
	{
		ZOffset -= 25.f;
		RefreshStatus(TEXT(""));
		RecNum(TEXT("height"), TEXT("z"), ZOffset);
	}
	if (PC->WasInputKeyJustPressed(EKeys::Home))
	{
		ZOffset = 0.f;
		RefreshStatus(TEXT(""));
		RecNum(TEXT("height"), TEXT("z"), ZOffset);
	}
	if (PC->WasInputKeyJustPressed(EKeys::Z))
	{
		Undo();
	}
	if (PC->WasInputKeyJustPressed(EKeys::N))
	{
		bEdgeSnap = !bEdgeSnap;
		RecNum(TEXT("snap"), TEXT("on"), bEdgeSnap ? 1.0 : 0.0);
		RefreshStatus(bEdgeSnap ? BBT(TEXT("Edge snap: on"), TEXT("贴边吸附：开")) : BBT(TEXT("Edge snap: off"), TEXT("贴边吸附：关")));
	}

	UpdateGhost(PC);

	if (PC->WasInputKeyJustPressed(EKeys::RightMouseButton))
	{
		Place();
	}
	if (PC->WasInputKeyJustPressed(EKeys::LeftMouseButton))
	{
		RemoveAimed(PC);
	}
}

FTransform UBlockBuildSubsystem::ComposeTransform(const FBlockBuildItem& Item, const FVector& Hit) const
{
	const float G = GBBGridSteps[GridIndex];
	FVector P;
	P.X = FMath::RoundToDouble(Hit.X / G) * G;
	P.Y = FMath::RoundToDouble(Hit.Y / G) * G;
	// the piece stands on the surface it was aimed at: its lowest point sits at the hit height
	P.Z = Hit.Z - Item.Min.Z + ZOffset;
	if (bEdgeSnap)
	{
		SnapToNeighbours(Item, P, Yaw);
	}
	return FTransform(FRotator(0.f, Yaw, 0.f), P, FVector::OneVector);
}

FBox UBlockBuildSubsystem::WorldBox(const FBlockBuildItem& Item, const FVector& P, float InYaw)
{
	const FRotator R(0.f, InYaw, 0.f);
	FBox B(ForceInit);
	for (int32 I = 0; I < 8; ++I)
	{
		const FVector C((I & 1) ? Item.Max.X : Item.Min.X, (I & 2) ? Item.Max.Y : Item.Min.Y, (I & 4) ? Item.Max.Z : Item.Min.Z);
		B += P + R.RotateVector(C);
	}
	return B;
}

void UBlockBuildSubsystem::SnapToNeighbours(const FBlockBuildItem& Item, FVector& P, float InYaw) const
{
	// Pieces can have any size (no common module), so a plain grid leaves gaps or overlaps. Slide the ghost so its box
	// edge meets the nearest edge of a neighbouring piece: first along X, then along Y (each only when the boxes overlap on
	// the other axes, so a floor tile snaps to the tile beside it and a wall to the wall it continues), then the top of a
	// piece below for stacking.
	const float Tol = FMath::Max(GBBGridSteps[GridIndex] * 0.75f, 40.f);
	FBox G = WorldBox(Item, P, InYaw);
	const FVector GC = G.GetCenter();
	TArray<FBox> Near;
	for (const FBlockBuildPiece& Pc : Pieces)
	{
		const int32* Idx = CatalogIndex.Find(Pc.MeshPath);
		if (!Idx || !Catalog[*Idx].bBounds)
		{
			continue;
		}
		const FBox B = WorldBox(Catalog[*Idx], Pc.Transform.GetLocation(), Pc.Transform.Rotator().Yaw);
		if (FVector::DistSquared(B.GetCenter(), GC) < FMath::Square(3000.f))
		{
			Near.Add(B);
		}
	}
	if (Near.Num() == 0)
	{
		return;
	}
	auto Overlap1D = [](double AMin, double AMax, double BMin, double BMax, double Margin)
	{
		return AMin < BMax - Margin && AMax > BMin + Margin;
	};
	for (int32 Axis = 0; Axis < 2; ++Axis)
	{
		const int32 Other = 1 - Axis;
		double Best = 0.0;
		bool bFound = false;
		for (const FBox& B : Near)
		{
			// the neighbour must share some extent on the other horizontal axis and in height
			// (height test is loose: aiming at the top of a neighbouring tile puts the ghost one tile-thickness higher, and it must still snap beside it)
			if (!Overlap1D(G.Min[Other], G.Max[Other], B.Min[Other], B.Max[Other], 5.0) || !Overlap1D(G.Min.Z - Tol, G.Max.Z + Tol, B.Min.Z, B.Max.Z, 0.0))
			{
				continue;
			}
			// abut only: ghost min edge to neighbour max edge, or ghost max edge to neighbour min edge
			const double Cands[2] = { B.Max[Axis] - G.Min[Axis], B.Min[Axis] - G.Max[Axis] };
			for (double D : Cands)
			{
				if (FMath::Abs(D) <= Tol && (!bFound || FMath::Abs(D) < FMath::Abs(Best)))
				{
					Best = D;
					bFound = true;
				}
			}
		}
		if (bFound && FMath::Abs(Best) > 0.01)
		{
			P[Axis] += Best;
			G = G.ShiftBy(FVector(Axis == 0 ? Best : 0.0, Axis == 1 ? Best : 0.0, 0.0));
		}
	}
	// level: a neighbour the ghost now abuts (touching on X or Y) with its bottom near the ghost bottom -> same floor level
	if (FMath::Abs(ZOffset) < 0.01)
	{
		double BestL = 0.0;
		bool bL = false;
		for (const FBox& B : Near)
		{
			const bool bTouchX = (FMath::Abs(B.Max.X - G.Min.X) < 1.0 || FMath::Abs(B.Min.X - G.Max.X) < 1.0) && Overlap1D(G.Min.Y, G.Max.Y, B.Min.Y, B.Max.Y, 5.0);
			const bool bTouchY = (FMath::Abs(B.Max.Y - G.Min.Y) < 1.0 || FMath::Abs(B.Min.Y - G.Max.Y) < 1.0) && Overlap1D(G.Min.X, G.Max.X, B.Min.X, B.Max.X, 5.0);
			if (!bTouchX && !bTouchY)
			{
				continue;
			}
			const double D = B.Min.Z - G.Min.Z;
			if (FMath::Abs(D) <= Tol && (!bL || FMath::Abs(D) < FMath::Abs(BestL)))
			{
				BestL = D;
				bL = true;
			}
		}
		if (bL && FMath::Abs(BestL) > 0.01)
		{
			P.Z += BestL;
			G = G.ShiftBy(FVector(0.0, 0.0, BestL));
		}
	}
	// stacking: ghost bottom close to a neighbour top (and horizontally overlapping) -> sit exactly on it
	double BestZ = 0.0;
	bool bZ = false;
	for (const FBox& B : Near)
	{
		if (!Overlap1D(G.Min.X, G.Max.X, B.Min.X, B.Max.X, 5.0) || !Overlap1D(G.Min.Y, G.Max.Y, B.Min.Y, B.Max.Y, 5.0))
		{
			continue;
		}
		const double D = B.Max.Z - G.Min.Z;
		if (FMath::Abs(D) <= Tol && (!bZ || FMath::Abs(D) < FMath::Abs(BestZ)))
		{
			BestZ = D;
			bZ = true;
		}
	}
	if (bZ && FMath::Abs(BestZ) > 0.01 && FMath::Abs(ZOffset) < 0.01)
	{
		P.Z += BestZ;
	}
}

void UBlockBuildSubsystem::UpdateGhost(APlayerController* PC)
{
	bGhostValid = false;
	const FBlockBuildItem* Item = SlotItem(Slot);
	if (!Ghost || !Item)
	{
		if (Ghost)
		{
			Ghost->SetVisibility(false);
		}
		return;
	}
	UStaticMesh* M = LoadMesh(Item->Path);
	if (!M)
	{
		Ghost->SetVisibility(false);
		return;
	}
	if (Ghost->GetStaticMesh() != M)
	{
		Ghost->SetStaticMesh(M);
		// preview: plain cyan, so glass or dark pieces still show where they will go
		static TStrongObjectPtr<UMaterialInstanceDynamic> GhostMID;
		if (!GhostMID.IsValid())
		{
			if (UMaterialInterface* Base = Cast<UMaterialInterface>(StaticLoadObject(UMaterialInterface::StaticClass(), nullptr,
				TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial"))))
			{
				UMaterialInstanceDynamic* MID = UMaterialInstanceDynamic::Create(Base, GetTransientPackage());
				MID->SetVectorParameterValue(TEXT("Color"), FLinearColor(0.25f, 0.85f, 1.0f, 1.f));
				GhostMID.Reset(MID);
			}
		}
		if (GhostMID.IsValid())
		{
			for (int32 I = 0; I < Ghost->GetNumMaterials(); ++I)
			{
				Ghost->SetMaterial(I, GhostMID.Get());
			}
		}
	}
	FVector Eye;
	FRotator Rot;
	PC->GetPlayerViewPoint(Eye, Rot);
	const FVector End = Eye + Rot.Vector() * 1500.f;
	FCollisionQueryParams Q(SCENE_QUERY_STAT(BlockBuildGhost), false);
	if (APawn* Pawn = PC->GetPawn())
	{
		Q.AddIgnoredActor(Pawn);
	}
	FHitResult HR;
	FVector Hit;
	if (GetWorld()->LineTraceSingleByChannel(HR, Eye, End, ECC_Visibility, Q))
	{
		Hit = HR.ImpactPoint;
	}
	else
	{
		Hit = Eye + Rot.Vector() * 600.f;
	}
	GhostTransform = ComposeTransform(*Item, Hit);
	Ghost->SetWorldTransform(GhostTransform);
	Ghost->SetVisibility(true);
	bGhostValid = true;
}

void UBlockBuildSubsystem::Place()
{
	const FBlockBuildItem* Item = SlotItem(Slot);
	if (!bGhostValid || !Item)
	{
		RefreshStatus(BBT(TEXT("Pick a piece for this slot in the inventory (I) first"), TEXT("先在背包(I)里选一个物件放到当前格")));
		return;
	}
	FBlockBuildPiece P;
	P.MeshPath = Item->Path;
	P.Transform = GhostTransform;
	Pieces.Add(P);
	PieceComps.Add(SpawnPieceComponent(P));
	{
		TSharedPtr<FJsonObject> E = BBPieceJson(P.MeshPath, P.Transform);
		E->SetNumberField(TEXT("piece"), Pieces.Num() - 1);
		Rec(TEXT("place"), E);
	}
	bSaveDirty = true;
	LastSaveRequest = FPlatformTime::Seconds();
	RefreshStatus(FString::Printf(TEXT("%s %s"), *BBT(TEXT("Placed"), TEXT("放置")), *Item->Name));
}

void UBlockBuildSubsystem::RemoveAimed(APlayerController* PC)
{
	FVector Eye;
	FRotator Rot;
	PC->GetPlayerViewPoint(Eye, Rot);
	FCollisionQueryParams Q(SCENE_QUERY_STAT(BlockBuildRemove), false);
	if (APawn* Pawn = PC->GetPawn())
	{
		Q.AddIgnoredActor(Pawn);
	}
	FHitResult HR;
	if (!GetWorld()->LineTraceSingleByChannel(HR, Eye, Eye + Rot.Vector() * 1500.f, ECC_Visibility, Q))
	{
		return;
	}
	UStaticMeshComponent* C = Cast<UStaticMeshComponent>(HR.GetComponent());
	const int32 Idx = C ? PieceComps.IndexOfByKey(C) : INDEX_NONE;
	if (Idx == INDEX_NONE)
	{
		RefreshStatus(BBT(TEXT("You can only remove pieces you built"), TEXT("只能拆除自己搭建的物件")));
		return;
	}
	{
		TSharedPtr<FJsonObject> E = BBPieceJson(Pieces[Idx].MeshPath, Pieces[Idx].Transform);
		E->SetNumberField(TEXT("piece"), Idx);
		Rec(TEXT("remove"), E);
	}
	C->DestroyComponent();
	PieceComps.RemoveAt(Idx);
	Pieces.RemoveAt(Idx);
	bSaveDirty = true;
	LastSaveRequest = FPlatformTime::Seconds();
	RefreshStatus(BBT(TEXT("Removed"), TEXT("已拆除")));
}

void UBlockBuildSubsystem::Undo()
{
	if (Pieces.Num() == 0)
	{
		return;
	}
	const int32 Idx = Pieces.Num() - 1;
	if (PieceComps.IsValidIndex(Idx) && PieceComps[Idx])
	{
		PieceComps[Idx]->DestroyComponent();
	}
	if (PieceComps.IsValidIndex(Idx))
	{
		PieceComps.RemoveAt(Idx);
	}
	{
		TSharedPtr<FJsonObject> E = BBPieceJson(Pieces[Idx].MeshPath, Pieces[Idx].Transform);
		E->SetNumberField(TEXT("piece"), Idx);
		Rec(TEXT("undo"), E);
	}
	Pieces.RemoveAt(Idx);
	bSaveDirty = true;
	LastSaveRequest = FPlatformTime::Seconds();
	RefreshStatus(BBT(TEXT("Undone"), TEXT("撤销")));
}

// ------------------------------------------------------------------ UI --------------------------------------------------

void UBlockBuildSubsystem::EnsureWidgets(UGameViewportClient* VP)
{
	using namespace BlockBuildStyle;
	if (bWidgets && HotbarRoot.IsValid())
	{
		return;
	}
	bWidgets = true;
	SlotBorders.SetNum(GBBHotbarSlots);
	SlotTexts.SetNum(GBBHotbarSlots);
	SlotImages.SetNum(GBBHotbarSlots);

	TSharedRef<SHorizontalBox> Row = SNew(SHorizontalBox);
	for (int32 I = 0; I < GBBHotbarSlots; ++I)
	{
		Row->AddSlot().AutoWidth().Padding(3.f, 0.f)
		[
			SAssignNew(SlotBorders[I], SBorder).BorderImage(SlotBg()).Padding(3.f)
			[
				SNew(SBorder).BorderImage(SlotIn()).Padding(0.f)
				[
					SNew(SBox).WidthOverride(96.f).HeightOverride(96.f)
					[
						SNew(SOverlay)
						+ SOverlay::Slot()
						[
							SAssignNew(SlotImages[I], SImage).Visibility(EVisibility::Collapsed)
						]
						+ SOverlay::Slot().HAlign(HAlign_Left).VAlign(VAlign_Top).Padding(5.f, 2.f)
						[
							SNew(STextBlock).Text(FText::AsNumber(I + 1)).Font(Font(TEXT("Bold"), 11)).ColorAndOpacity(TextDim)
								.ShadowOffset(FVector2D(1.f, 1.f)).ShadowColorAndOpacity(FLinearColor::Black)
						]
						+ SOverlay::Slot().HAlign(HAlign_Center).VAlign(VAlign_Bottom).Padding(3.f, 0.f, 3.f, 2.f)
						[
							SAssignNew(SlotTexts[I], STextBlock).Font(Font(TEXT("Regular"), 8)).ColorAndOpacity(TextMain)
								.AutoWrapText(true).Justification(ETextJustify::Center)
								.ShadowOffset(FVector2D(1.f, 1.f)).ShadowColorAndOpacity(FLinearColor::Black)
						]
					]
				]
			]
		];
	}

	HotbarRoot = SNew(SOverlay).Visibility(EVisibility::Collapsed)
		+ SOverlay::Slot().HAlign(HAlign_Center).VAlign(VAlign_Bottom).Padding(FMargin(0.f, 0.f, 0.f, 14.f))
		[
			SNew(SVerticalBox)
			+ SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center).Padding(0.f, 0.f, 0.f, 4.f)
			[
				SNew(STextBlock).Font(Font(TEXT("Regular"), 11)).ColorAndOpacity(TextDim)
					.ShadowOffset(FVector2D(1.f, 1.f)).ShadowColorAndOpacity(FLinearColor::Black)
					.Text(FText::FromString(BBHelpLine()))
			]
			+ SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center).Padding(0.f, 0.f, 0.f, 6.f)
			[
				SNew(SBorder).BorderImage(Status()).Padding(FMargin(12.f, 5.f))
				[
					SAssignNew(StatusText, STextBlock).Font(Font(TEXT("Regular"), 13)).ColorAndOpacity(FLinearColor(1.f, 0.93f, 0.75f, 1.f))
				]
			]
			+ SVerticalBox::Slot().AutoHeight()
			[
				Row
			]
		];
	VP->AddViewportWidgetContent(HotbarRoot.ToSharedRef(), 7);

	InventoryRoot = SNew(SBlockBuildInventory, this).Visibility(EVisibility::Collapsed)
	[
		SNew(SBorder).BorderImage(Dim()).Padding(0.f).HAlign(HAlign_Center).VAlign(VAlign_Center)
		[
			SNew(SBorder).BorderImage(Panel()).Padding(14.f)
			[
				SNew(SBox).WidthOverride(1180.f).HeightOverride(700.f)
				[
					SNew(SVerticalBox)
					+ SVerticalBox::Slot().AutoHeight().Padding(6.f, 2.f, 6.f, 10.f)
					[
						SNew(SHorizontalBox)
						+ SHorizontalBox::Slot().FillWidth(1.f)
						[
							SAssignNew(InventoryTitle, STextBlock).Font(Font(TEXT("Bold"), 20)).ColorAndOpacity(TextMain)
						]
						+ SHorizontalBox::Slot().AutoWidth()
						[
							SNew(STextBlock).Font(Font(TEXT("Regular"), 13)).ColorAndOpacity(TextDim)
								.Text(FText::FromString(BBT(TEXT("Click a piece = put it in the current slot (choose the slot with 1-9)   I / Esc close"), TEXT("点物件 = 放进当前物品格（先按 1-9 选格）   I / Esc 关闭"))))
						]
					]
					+ SVerticalBox::Slot().FillHeight(1.f)
					[
						SNew(SHorizontalBox)
						+ SHorizontalBox::Slot().AutoWidth().Padding(0.f, 0.f, 10.f, 0.f)
						[
							SNew(SBorder).BorderImage(Side()).Padding(8.f)
							[
								SNew(SBox).WidthOverride(190.f)
								[
									SAssignNew(CategoryBox, SScrollBox)
								]
							]
						]
						+ SHorizontalBox::Slot().FillWidth(1.f)
						[
							SNew(SScrollBox)
							+ SScrollBox::Slot()
							[
								SAssignNew(ItemBox, SWrapBox).UseAllottedSize(true).InnerSlotPadding(FVector2D(6.f, 6.f))
							]
						]
					]
				]
			]
		]
	];
	VP->AddViewportWidgetContent(InventoryRoot.ToSharedRef(), 12);
	RefreshHotbar();
	RefreshStatus(TEXT(""));
}

void UBlockBuildSubsystem::RemoveWidgets()
{
	UWorld* World = GetWorld();
	if (UGameViewportClient* VP = World ? World->GetGameViewport() : nullptr)
	{
		if (HotbarRoot.IsValid())
		{
			VP->RemoveViewportWidgetContent(HotbarRoot.ToSharedRef());
		}
		if (InventoryRoot.IsValid())
		{
			VP->RemoveViewportWidgetContent(InventoryRoot.ToSharedRef());
		}
	}
	HotbarRoot.Reset();
	InventoryRoot.Reset();
	CategoryBox.Reset();
	ItemBox.Reset();
	StatusText.Reset();
	InventoryTitle.Reset();
	SlotBorders.Reset();
	SlotTexts.Reset();
	SlotImages.Reset();
	ThumbBrushes.Reset();
	bWidgets = false;
}

void UBlockBuildSubsystem::RefreshHotbar()
{
	using namespace BlockBuildStyle;
	for (int32 I = 0; I < SlotBorders.Num(); ++I)
	{
		if (SlotBorders[I].IsValid())
		{
			SlotBorders[I]->SetBorderImage(I == Slot ? SlotSel() : SlotBg());
		}
		const FBlockBuildItem* It = SlotItem(I);
		if (SlotTexts[I].IsValid())
		{
			SlotTexts[I]->SetText(FText::FromString(It ? It->Name.Replace(TEXT("_"), TEXT(" ")) : FString()));
		}
		if (SlotImages.IsValidIndex(I) && SlotImages[I].IsValid())
		{
			if (It)
			{
				SlotImages[I]->SetImage(ThumbBrush(It->Path));
				SlotImages[I]->SetVisibility(EVisibility::HitTestInvisible);
			}
			else
			{
				SlotImages[I]->SetVisibility(EVisibility::Collapsed);
			}
		}
	}
	if (InventoryTitle.IsValid())
	{
		InventoryTitle->SetText(FText::FromString(FString::Printf(TEXT("%s %d"), *BBT(TEXT("Build inventory · slot"), TEXT("建造背包 · 当前格")), Slot + 1)));
	}
}

void UBlockBuildSubsystem::RefreshStatus(const FString& Extra)
{
	if (!StatusText.IsValid())
	{
		return;
	}
	const FBlockBuildItem* It = SlotItem(Slot);
	FString S = BBZh()
		? FString::Printf(TEXT("%s   朝向 %.0f°   网格 %.0fcm   贴边(N) %s   高度 %+.0fcm   已建 %d"),
			It ? *It->Name : TEXT("(空格)"), Yaw, GBBGridSteps[GridIndex], bEdgeSnap ? TEXT("开") : TEXT("关"), ZOffset, Pieces.Num())
		: FString::Printf(TEXT("%s   yaw %.0f°   grid %.0fcm   edge snap (N) %s   height %+.0fcm   built %d"),
			It ? *It->Name : TEXT("(empty)"), Yaw, GBBGridSteps[GridIndex], bEdgeSnap ? TEXT("on") : TEXT("off"), ZOffset, Pieces.Num());
	if (const UBlockBuildSettings* RecSettings = BBSettings())
	{
		if (RecSettings->bRecordSessions)
		{
			S += BBZh() ? TEXT("   ● 录制中") : TEXT("   ● REC");
		}
	}
	if (!Extra.IsEmpty())
	{
		S += TEXT("   ·  ") + Extra;
	}
	StatusText->SetText(FText::FromString(S));
}

void UBlockBuildSubsystem::FillInventory()
{
	using namespace BlockBuildStyle;
	if (!CategoryBox.IsValid())
	{
		return;
	}
	CategoryBox->ClearChildren();
	for (int32 C = 0; C < Categories.Num(); ++C)
	{
		int32 Count = 0;
		for (const FBlockBuildItem& It : Catalog)
		{
			Count += It.Category == Categories[C] ? 1 : 0;
		}
		CategoryBox->AddSlot().Padding(0.f, 2.f)
		[
			SNew(SButton).ButtonStyle(&CatButton(C == CurCategory)).HAlign(HAlign_Left)
				.OnClicked_Lambda([this, C]() { ShowCategory(C); return FReply::Handled(); })
			[
				SNew(STextBlock).Font(Font(TEXT("Regular"), 15)).ColorAndOpacity(TextMain)
					.Text(FText::FromString(FString::Printf(TEXT("%s  %d"), *Categories[C], Count)))
			]
		];
	}
	ShowCategory(CurCategory);
	RefreshHotbar();
}

void UBlockBuildSubsystem::ShowCategory(int32 CatIndex)
{
	using namespace BlockBuildStyle;
	if (!ItemBox.IsValid() || !Categories.IsValidIndex(CatIndex))
	{
		return;
	}
	if (CatIndex != CurCategory)
	{
		CurCategory = CatIndex;
		FillInventory();   // re-styles the category buttons; it calls back here with the same index
		return;
	}
	ItemBox->ClearChildren();
	for (int32 I = 0; I < Catalog.Num(); ++I)
	{
		const FBlockBuildItem& It = Catalog[I];
		if (It.Category != Categories[CatIndex])
		{
			continue;
		}
		if (!It.bBounds)
		{
			LoadMesh(It.Path);   // fills It.Min / It.Max
		}
		const FVector Size = It.Max - It.Min;
		const FString Dim = FString::Printf(TEXT("%.1f × %.1f × %.1f m"), Size.X / 100.f, Size.Y / 100.f, Size.Z / 100.f);
		ItemBox->AddSlot()
		[
			SNew(SBox).WidthOverride(150.f)
			[
				SNew(SButton).ButtonStyle(&ItemButton()).HAlign(HAlign_Fill)
					.OnClicked_Lambda([this, I]()
					{
						if (Catalog.IsValidIndex(I) && Hotbar.IsValidIndex(Slot))
						{
							Hotbar[Slot] = Catalog[I].Path;
							{
								TSharedPtr<FJsonObject> E = MakeShared<FJsonObject>();
								E->SetNumberField(TEXT("slot"), Slot + 1);
								E->SetStringField(TEXT("mesh"), Catalog[I].Path);
								Rec(TEXT("pick"), E);
							}
							bSaveDirty = true;
							LastSaveRequest = FPlatformTime::Seconds();
							RefreshHotbar();
							RefreshStatus(TEXT(""));
						}
						return FReply::Handled();
					})
				[
					SNew(SVerticalBox)
					+ SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center).Padding(0.f, 0.f, 0.f, 4.f)
					[
						SNew(SBox).WidthOverride(GBBThumbSize).HeightOverride(GBBThumbSize)
						[
							SNew(SImage).Image(ThumbBrush(It.Path))
						]
					]
					+ SVerticalBox::Slot().AutoHeight()
					[
						SNew(STextBlock).Font(Font(TEXT("Bold"), 11)).ColorAndOpacity(TextMain).AutoWrapText(true)
							.Text(FText::FromString(It.Name.Replace(TEXT("_"), TEXT(" "))))
					]
					+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 3.f, 0.f, 0.f)
					[
						SNew(STextBlock).Font(Font(TEXT("Regular"), 10)).ColorAndOpacity(TextDim).Text(FText::FromString(Dim))
					]
				]
			]
		];
	}
}

// ------------------------------------------------------------------ thumbnails ----------------------------------------

static const float GBBCaptureZ = 900000.f;   // far above the world so nothing else is in the frame

void UBlockBuildSubsystem::EnsureCaptureRig()
{
	if (CaptureRig && IsValid(CaptureRig) && Capture && CaptureMesh)
	{
		return;
	}
	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}
	FActorSpawnParameters P;
	P.ObjectFlags |= RF_Transient;
	P.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	CaptureRig = World->SpawnActor<AActor>(AActor::StaticClass(), FTransform(FVector(0.f, 0.f, GBBCaptureZ)), P);
	if (!CaptureRig)
	{
		return;
	}
#if WITH_EDITOR
	CaptureRig->SetActorLabel(TEXT("BlockBuild_ThumbRig"));
#endif
	USceneComponent* Root = NewObject<USceneComponent>(CaptureRig, TEXT("Root"));
	CaptureRig->SetRootComponent(Root);
	Root->RegisterComponent();
	CaptureMesh = NewObject<UStaticMeshComponent>(CaptureRig, TEXT("Mesh"));
	CaptureMesh->SetMobility(EComponentMobility::Movable);
	CaptureMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	CaptureMesh->SetCastShadow(false);
	CaptureMesh->bVisibleInReflectionCaptures = false;
	CaptureMesh->SetVisibleInSceneCaptureOnly(true);
	CaptureMesh->SetupAttachment(Root);
	CaptureMesh->RegisterComponent();
	Capture = NewObject<USceneCaptureComponent2D>(CaptureRig, TEXT("Capture"));
	Capture->bCaptureEveryFrame = false;
	Capture->bCaptureOnMovement = false;
	Capture->bAlwaysPersistRenderingState = true;
	Capture->CaptureSource = ESceneCaptureSource::SCS_BaseColor;   // unlit albedo: readable at night too
	Capture->PrimitiveRenderMode = ESceneCapturePrimitiveRenderMode::PRM_UseShowOnlyList;
	Capture->ShowOnlyComponents.Add(CaptureMesh);
	Capture->FOVAngle = 30.f;
	Capture->ShowFlags.SetAtmosphere(false);
	Capture->ShowFlags.SetFog(false);
	Capture->ShowFlags.SetVolumetricFog(false);
	Capture->ShowFlags.SetSkyLighting(false);
	Capture->SetupAttachment(Root);
	Capture->RegisterComponent();
}

void UBlockBuildSubsystem::QueueThumb(const FString& Path)
{
	if (!Thumbs.Contains(Path) && !ThumbQueue.Contains(Path))
	{
		ThumbQueue.Add(Path);
	}
}

const FSlateBrush* UBlockBuildSubsystem::ThumbBrush(const FString& Path)
{
	if (TSharedPtr<FSlateBrush>* Found = ThumbBrushes.Find(Path))
	{
		return Found->Get();
	}
	TSharedPtr<FSlateBrush> B = MakeShared<FSlateBrush>();
	B->DrawAs = ESlateBrushDrawType::Image;
	B->ImageSize = FVector2D(GBBThumbSize, GBBThumbSize);
	B->TintColor = FSlateColor(FLinearColor(0.12f, 0.13f, 0.16f, 1.f));   // placeholder until the capture lands
	if (TObjectPtr<UTexture2D>* T = Thumbs.Find(Path))
	{
		B->SetResourceObject(*T);
		B->TintColor = FSlateColor(FLinearColor::White);
	}
	ThumbBrushes.Add(Path, B);
	QueueThumb(Path);
	return B.Get();
}

void UBlockBuildSubsystem::PumpThumbnails()
{
	// Two-phase: the mesh set this tick is captured next tick (a mesh used for the first time has no render proxy yet,
	// which left some thumbnails black in testing). One item per tick; the inventory shows ~15-150 items, so a
	// category fills in a few seconds.
	static FString Pending;          // mesh set last tick, to capture now
	static TWeakObjectPtr<UWorld> PendingWorld;
	if (PendingWorld.Get() != GetWorld())
	{
		Pending.Reset();
		PendingWorld = GetWorld();
	}
	if (ThumbQueue.Num() == 0 && Pending.IsEmpty())
	{
		return;
	}
	EnsureCaptureRig();
	if (!Capture || !CaptureMesh)
	{
		ThumbQueue.Reset();
		Pending.Reset();
		return;
	}
	if (!ThumbRT)
	{
		ThumbRT = NewObject<UTextureRenderTarget2D>(this);
		ThumbRT->RenderTargetFormat = ETextureRenderTargetFormat::RTF_RGBA8;
		ThumbRT->ClearColor = FLinearColor(0.02f, 0.022f, 0.03f, 1.f);
		ThumbRT->bAutoGenerateMips = false;
		ThumbRT->InitAutoFormat(GBBThumbSize, GBBThumbSize);
		ThumbRT->UpdateResourceImmediate(true);
	}

	// phase 2: capture the mesh that has been on the rig since last tick
	if (!Pending.IsEmpty())
	{
		const FString Path = Pending;
		Pending.Reset();
		Capture->TextureTarget = ThumbRT;
		Capture->CaptureScene();
		UTexture2D* Tex = nullptr;
		if (FTextureRenderTargetResource* Res = ThumbRT->GameThread_GetRenderTargetResource())
		{
			TArray<FColor> Px;
			FReadSurfaceDataFlags Flags(RCM_UNorm);
			Flags.SetLinearToGamma(false);
			if (Res->ReadPixels(Px, Flags) && Px.Num() == GBBThumbSize * GBBThumbSize)
			{
				for (FColor& C : Px)
				{
					C.A = 255;
				}
				Tex = UTexture2D::CreateTransient(GBBThumbSize, GBBThumbSize, PF_B8G8R8A8);
				Tex->SRGB = false;
				Tex->Filter = TF_Bilinear;
				void* Data = Tex->GetPlatformData()->Mips[0].BulkData.Lock(LOCK_READ_WRITE);
				FMemory::Memcpy(Data, Px.GetData(), Px.Num() * sizeof(FColor));
				Tex->GetPlatformData()->Mips[0].BulkData.Unlock();
				Tex->UpdateResource();
			}
		}
		if (Tex)
		{
			Thumbs.Add(Path, Tex);
			if (TSharedPtr<FSlateBrush>* B = ThumbBrushes.Find(Path))
			{
				(*B)->SetResourceObject(Tex);
				(*B)->TintColor = FSlateColor(FLinearColor::White);
			}
		}
	}

	// phase 1: put the next mesh on the rig and aim the camera
	while (ThumbQueue.Num() > 0 && Pending.IsEmpty())
	{
		const FString Path = ThumbQueue[0];
		ThumbQueue.RemoveAt(0);
		if (Thumbs.Contains(Path))
		{
			continue;
		}
		UStaticMesh* M = LoadMesh(Path);
		if (!M)
		{
			continue;
		}
		CaptureMesh->SetStaticMesh(M);
		CaptureMesh->SetRelativeLocation(FVector::ZeroVector);
		const FBoxSphereBounds BS = M->GetBounds();
		const FVector Centre = BS.Origin;
		const float Dist = FMath::Max(BS.SphereRadius, 10.f) / FMath::Sin(FMath::DegreesToRadians(Capture->FOVAngle * 0.5f)) * 1.05f;
		const FVector Dir = FVector(-0.62f, -0.62f, 0.48f).GetSafeNormal();   // from -X -Y, 29° up
		const FVector CamPos = Centre + Dir * Dist;
		Capture->SetRelativeLocation(CamPos);
		Capture->SetRelativeRotation((Centre - CamPos).Rotation());
		Pending = Path;
	}
}

// ------------------------------------------------------------------ test hooks ----------------------------------------

void UBlockBuildSubsystem::TestPick(int32 InSlot, const FString& NamePart)
{
	if (!Hotbar.IsValidIndex(InSlot))
	{
		return;
	}
	const FBlockBuildItem* Best = nullptr;
	for (const FBlockBuildItem& It : Catalog)
	{
		if (It.Name.Equals(NamePart, ESearchCase::IgnoreCase))
		{
			Best = &It;
			break;
		}
		if (!Best && It.Name.Contains(NamePart, ESearchCase::IgnoreCase))
		{
			Best = &It;
		}
	}
	if (!Best)
	{
		UE_LOG(LogBlockBuild, Warning, TEXT("blockbuild pick: nothing matches '%s'"), *NamePart);
		return;
	}
	Hotbar[InSlot] = Best->Path;
	Slot = InSlot;
	bSaveDirty = true;
	LastSaveRequest = FPlatformTime::Seconds();
	RefreshHotbar();
	RefreshStatus(TEXT(""));
	UE_LOG(LogBlockBuild, Log, TEXT("blockbuild pick: slot %d = %s"), InSlot + 1, *Best->Path);
}

void UBlockBuildSubsystem::TestSetYaw(float InYaw)
{
	Yaw = FMath::Fmod(FMath::RoundToFloat(InYaw / 45.f) * 45.f + 360.f, 360.f);
	RefreshStatus(TEXT(""));
}

void UBlockBuildSubsystem::TestPlace()
{
	UWorld* World = GetWorld();
	APlayerController* PC = World ? World->GetFirstPlayerController() : nullptr;
	if (!PC)
	{
		return;
	}
	if (!bActive)
	{
		SetActive(true);
	}
	UpdateGhost(PC);
	Place();
	UE_LOG(LogBlockBuild, Log, TEXT("blockbuild place: %s -> %s"), bGhostValid ? TEXT("ok") : TEXT("no ghost"), *GhostTransform.ToString());
}

void UBlockBuildSubsystem::TestRemove()
{
	UWorld* World = GetWorld();
	if (APlayerController* PC = World ? World->GetFirstPlayerController() : nullptr)
	{
		RemoveAimed(PC);
	}
}

void UBlockBuildSubsystem::TestCategory(int32 CatIndex)
{
	if (Categories.IsValidIndex(CatIndex))
	{
		ShowCategory(CatIndex);
	}
}

// ------------------------------------------------------------------ session recording ---------------------------------

void UBlockBuildSubsystem::Rec(const TCHAR* Op, TSharedPtr<FJsonObject> Extra)
{
	const UBlockBuildSettings* S = BBSettings();
	if (!S || !S->bRecordSessions)
	{
		return;
	}
	if (SessionStart < 0.0)
	{
		// a session starts with its first event; keep what was already built so the steps can be replayed from it
		SessionStart = FPlatformTime::Seconds();
		SessionStartUtc = FDateTime::UtcNow();
		SessionStartPieces.Reset();
		for (const FBlockBuildPiece& P : Pieces)
		{
			SessionStartPieces.Add(MakeShared<FJsonValueObject>(BBPieceJson(P.MeshPath, P.Transform)));
		}
	}
	TSharedPtr<FJsonObject> E = Extra;
	if (!E.IsValid())
	{
		E = MakeShared<FJsonObject>();
	}
	E->SetNumberField(TEXT("t"), FMath::RoundToDouble((FPlatformTime::Seconds() - SessionStart) * 100.0) / 100.0);
	E->SetStringField(TEXT("op"), Op);
	SessionEvents.Add(MakeShared<FJsonValueObject>(E));
}

void UBlockBuildSubsystem::RecNum(const TCHAR* Op, const TCHAR* Key, double Value)
{
	TSharedPtr<FJsonObject> E = MakeShared<FJsonObject>();
	E->SetNumberField(Key, Value);
	Rec(Op, E);
}

void UBlockBuildSubsystem::FlushSession()
{
	if (SessionEvents.Num() == 0)
	{
		return;
	}
	// no user name, computer name or absolute paths: only the level name, piece names, positions and timings
	TSharedPtr<FJsonObject> Root = MakeShared<FJsonObject>();
	Root->SetStringField(TEXT("schema"), TEXT("blockbuildkit.session/1"));
	Root->SetStringField(TEXT("plugin_version"), BBPluginVersion());
	Root->SetStringField(TEXT("engine_version"), FEngineVersion::Current().ToString(EVersionComponent::Patch));
	Root->SetStringField(TEXT("started_utc"), SessionStartUtc.ToIso8601());
	Root->SetNumberField(TEXT("duration_s"), FMath::RoundToDouble((FPlatformTime::Seconds() - SessionStart) * 10.0) / 10.0);
	Root->SetStringField(TEXT("level"), MapName);
	Root->SetArrayField(TEXT("start_pieces"), SessionStartPieces);
	Root->SetArrayField(TEXT("events"), SessionEvents);
	Root->SetNumberField(TEXT("end_piece_count"), Pieces.Num());
	FString Out;
	const TSharedRef<TJsonWriter<>> W = TJsonWriterFactory<>::Create(&Out);
	FJsonSerializer::Serialize(Root.ToSharedRef(), W);
	const FString Dir = FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("BlockBuild"), TEXT("Sessions"));
	IFileManager::Get().MakeDirectory(*Dir, true);
	const FString File = FPaths::Combine(Dir, SessionStartUtc.ToString(TEXT("%Y-%m-%d_%H-%M-%S")) + TEXT(".json"));
	if (FFileHelper::SaveStringToFile(Out, *File))
	{
		UE_LOG(LogBlockBuild, Log, TEXT("Block Build: recorded %d steps to %s"), SessionEvents.Num(), *File);
	}
	SessionEvents.Reset();
	SessionStartPieces.Reset();
	SessionStart = -1.0;
}

// ------------------------------------------------------------------ console -------------------------------------------

static FAutoConsoleCommandWithWorldAndArgs GBBBuildCmd(
	TEXT("blockbuild"),
	TEXT("blockbuild export <name> | clear | toggle | on | off | inv | cat <i> | pick <slot> <name> | slot <n> | rot <deg> | place | remove | status"),
	FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
	{
		UBlockBuildSubsystem* B = World ? World->GetSubsystem<UBlockBuildSubsystem>() : nullptr;
		if (!B)
		{
			UE_LOG(LogBlockBuild, Warning, TEXT("blockbuild: no build subsystem in this world (start Play first)."));
			return;
		}
		B->BuildCatalogIfNeeded();
		const FString Sub = Args.Num() > 0 ? Args[0].ToLower() : TEXT("status");
		if (Sub == TEXT("export"))
		{
			FString Path;
			const int32 N = B->ExportJson(Args.Num() > 1 ? Args[1] : TEXT("build"), Path);
			UE_LOG(LogBlockBuild, Log, TEXT("blockbuild: exported %d pieces to %s"), N, *Path);
			if (GEngine)
			{
				GEngine->AddOnScreenDebugMessage(-1, 6.f, FColor::Cyan, FString::Printf(TEXT("Block Build: exported %d pieces to %s"), N, *Path));
			}
		}
		else if (Sub == TEXT("clear"))
		{
			B->ClearAll();
		}
		else if (Sub == TEXT("toggle"))
		{
			B->SetActive(!B->IsActive());
		}
		else if (Sub == TEXT("on") || Sub == TEXT("off"))
		{
			B->SetActive(Sub == TEXT("on"));
		}
		else if (Sub == TEXT("inv"))
		{
			if (Args.Num() > 1 && Args[1] == TEXT("close")) B->CloseInventory(); else B->OpenInventory();
		}
		else if (Sub == TEXT("cat") && Args.Num() > 1)
		{
			B->TestCategory(FCString::Atoi(*Args[1]));
		}
		else if (Sub == TEXT("pick") && Args.Num() > 2)
		{
			// blockbuild pick <slot 1-9> <name substring>
			B->TestPick(FCString::Atoi(*Args[1]) - 1, Args[2]);
		}
		else if (Sub == TEXT("slot") && Args.Num() > 1)
		{
			B->SelectSlot(FCString::Atoi(*Args[1]) - 1);
		}
		else if (Sub == TEXT("rot") && Args.Num() > 1)
		{
			B->TestSetYaw(FCString::Atof(*Args[1]));
		}
		else if (Sub == TEXT("place"))
		{
			B->TestPlace();
		}
		else if (Sub == TEXT("remove"))
		{
			B->TestRemove();
		}
		else
		{
			UE_LOG(LogBlockBuild, Log, TEXT("blockbuild: %s"), *B->DebugState());
		}
	}));
