// Copyright (c) 2026 tongchen liu. MIT License, see LICENSE.

#include "Modules/ModuleManager.h"
#include "BlockBuildSubsystem.h"
#include "BlockBuildSettings.h"
#include "Editor.h"
#include "ToolMenus.h"
#include "ScopedTransaction.h"
#include "Engine/World.h"
#include "Engine/Level.h"
#include "Engine/StaticMesh.h"
#include "Engine/StaticMeshActor.h"
#include "Components/StaticMeshComponent.h"
#include "Kismet/GameplayStatics.h"
#include "HAL/IConsoleManager.h"
#include "Misc/MessageDialog.h"
#include "Framework/Notifications/NotificationManager.h"
#include "Widgets/Notifications/SNotificationList.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Misc/EngineVersion.h"
#include "HAL/FileManager.h"
#include "HAL/PlatformProcess.h"
#include "Interfaces/IPluginManager.h"

#define LOCTEXT_NAMESPACE "BlockBuildKitEditor"

namespace BlockBuildKitEditor
{
	static FString SlotName()
	{
		const UBlockBuildSettings* S = GetDefault<UBlockBuildSettings>();
		return (S && !S->SaveSlotName.IsEmpty()) ? S->SaveSlotName : FString(TEXT("BlockBuild"));
	}

	static void Notify(const FText& Message, bool bSuccess)
	{
		FNotificationInfo Info(Message);
		Info.ExpireDuration = 6.f;
		Info.bUseSuccessFailIcons = true;
		TSharedPtr<SNotificationItem> Item = FSlateNotificationManager::Get().AddNotification(Info);
		if (Item.IsValid())
		{
			Item->SetCompletionState(bSuccess ? SNotificationItem::CS_Success : SNotificationItem::CS_Fail);
		}
	}

	/** Turns the saved build into normal Static Mesh Actors in the open level (outliner folder "BlockBuild"; Ctrl+Z undoes it). */
	static void BakeIntoLevel()
	{
		if (!GEditor)
		{
			return;
		}
		if (GEditor->PlayWorld)
		{
			Notify(LOCTEXT("StopPlay", "Stop Play first, then bake."), false);
			return;
		}
		UWorld* World = GEditor->GetEditorWorldContext().World();
		const FString Slot = SlotName();
		if (!World || !UGameplayStatics::DoesSaveGameExist(Slot, 0))
		{
			Notify(FText::Format(LOCTEXT("NoSave", "Nothing to bake yet: no saved build \"{0}\". Build something in Play first."), FText::FromString(Slot)), false);
			return;
		}
		const UBlockBuildSaveGame* Save = Cast<UBlockBuildSaveGame>(UGameplayStatics::LoadGameFromSlot(Slot, 0));
		if (!Save || Save->Pieces.Num() == 0)
		{
			Notify(LOCTEXT("Empty", "The saved build is empty."), false);
			return;
		}

		const FScopedTransaction Transaction(LOCTEXT("BakeTransaction", "Bake Block Build into level"));
		ULevel* Level = World->GetCurrentLevel();
		GEditor->SelectNone(false, true);
		int32 Made = 0;
		int32 Missing = 0;
		for (const FBlockBuildPiece& Piece : Save->Pieces)
		{
			UStaticMesh* Mesh = LoadObject<UStaticMesh>(nullptr, *Piece.MeshPath);
			if (!Mesh)
			{
				++Missing;
				continue;
			}
			AStaticMeshActor* Actor = Cast<AStaticMeshActor>(GEditor->AddActor(Level, AStaticMeshActor::StaticClass(), Piece.Transform, true));
			if (!Actor)
			{
				continue;
			}
			Actor->GetStaticMeshComponent()->SetStaticMesh(Mesh);
			Actor->SetActorLabel(Mesh->GetName());
			Actor->SetFolderPath(FName(TEXT("BlockBuild")));
			Actor->PostEditChange();
			++Made;
		}
		GEditor->RedrawLevelEditingViewports();
		Notify(FText::Format(LOCTEXT("Baked", "Baked {0} pieces into the level (outliner folder \"BlockBuild\"). Skipped {1} with missing meshes. Ctrl+Z undoes it."),
			FText::AsNumber(Made), FText::AsNumber(Missing)), Made > 0);
	}

	// ------------------------------------------------------------------------------------------------- sharing export

	/** Rough kind of a piece from its name / folder, so builds made with different packs can be compared (wall, door, ...). */
	static FString TypeTag(const FString& Name, const FString& Category)
	{
		const FString S = (Name + TEXT(" ") + Category).ToLower();
		struct FRule { const TCHAR* Tag; const TCHAR* Words; };
		static const FRule Rules[] = {
			{ TEXT("door"),      TEXT("door|gate") },
			{ TEXT("window"),    TEXT("window|glass|shutter") },
			{ TEXT("stairs"),    TEXT("stair|step|ladder") },
			{ TEXT("roof"),      TEXT("roof|chimney|gutter") },
			{ TEXT("railing"),   TEXT("fence|rail|balcony|balustrade|guard") },
			{ TEXT("column"),    TEXT("column|pillar|pole|post|beam") },
			{ TEXT("wall"),      TEXT("wall|facade|brick|panel") },
			{ TEXT("floor"),     TEXT("floor|tile|paving|pavement|ground|slab|plane") },
			{ TEXT("road"),      TEXT("road|street|curb|kerb|sidewalk|crosswalk|asphalt") },
			{ TEXT("light"),     TEXT("lamp|light|lantern|candle") },
			{ TEXT("seating"),   TEXT("chair|stool|bench|sofa|seat|couch") },
			{ TEXT("table"),     TEXT("table|desk") },
			{ TEXT("furniture"), TEXT("shelf|cabinet|counter|bar_|_bar|bed_|_bed|wardrobe|drawer|cupboard") },
			{ TEXT("plant"),     TEXT("tree|bush|plant|grass|flower|hedge|ivy|planter|flowerpot") },
			{ TEXT("vehicle"),   TEXT("vehicle|car_|_car|sedan|truck|bus_|_bus|bicycle|bike|van_|_van") },
			{ TEXT("sign"),      TEXT("sign|poster|banner|billboard") },
			{ TEXT("block"),     TEXT("cube|box|block|crate") },
			{ TEXT("primitive"), TEXT("cylinder|cone|sphere") },
		};
		for (const FRule& Rule : Rules)
		{
			TArray<FString> Words;
			FString(Rule.Words).ParseIntoArray(Words, TEXT("|"));
			for (const FString& W : Words)
			{
				if (S.Contains(W))
				{
					return Rule.Tag;
				}
			}
		}
		return TEXT("prop");
	}

	/** Same tab name the in-game inventory shows: first sub-folder below the matching piece folder. */
	static FString CategoryFor(const FString& MeshPath)
	{
		if (MeshPath.StartsWith(TEXT("/Engine/BasicShapes/")))
		{
			return TEXT("Basic Shapes");
		}
		const FString PackagePath = FPaths::GetPath(MeshPath);
		if (const UBlockBuildSettings* S = GetDefault<UBlockBuildSettings>())
		{
			for (const FDirectoryPath& D : S->PieceFolders)
			{
				FString Root = D.Path;
				Root.TrimStartAndEndInline();
				while (Root.EndsWith(TEXT("/")))
				{
					Root.LeftChopInline(1);
				}
				if (Root.IsEmpty() || !(PackagePath == Root || PackagePath.StartsWith(Root + TEXT("/"))))
				{
					continue;
				}
				FString Rel = PackagePath.RightChop(Root.Len());
				Rel.RemoveFromStart(TEXT("/"));
				FString First, Rest;
				if (Rel.Split(TEXT("/"), &First, &Rest))
				{
					return First;
				}
				return Rel.IsEmpty() ? FPaths::GetCleanFilename(Root) : Rel;
			}
		}
		return TEXT("Other");
	}

	static TArray<TSharedPtr<FJsonValue>> Vec3(double X, double Y, double Z)
	{
		auto R = [](double V) { return MakeShared<FJsonValueNumber>(FMath::RoundToDouble(V * 10.0) / 10.0); };
		TArray<TSharedPtr<FJsonValue>> A;
		A.Add(R(X));
		A.Add(R(Y));
		A.Add(R(Z));
		return A;
	}

	static FString PluginVersion()
	{
		const TSharedPtr<IPlugin> Plugin = IPluginManager::Get().FindPlugin(TEXT("BlockBuildKit"));
		return Plugin.IsValid() ? Plugin->GetDescriptor().VersionName : FString(TEXT("unknown"));
	}

	/**
	 * Writes Saved/BlockBuild/Share/build_<time>.json: the saved build (each different mesh once, with its tab, a rough type and
	 * its size) plus any recorded sessions. No user name, computer name, absolute paths or asset files go into it.
	 * Opens the folder and offers a link to the share page; sending it is always the user's own choice.
	 */
	static void ExportForSharing()
	{
		if (GEditor && GEditor->PlayWorld)
		{
			Notify(LOCTEXT("ShareStopPlay", "Stop Play first, then export."), false);
			return;
		}
		const FString Slot = SlotName();
		const UBlockBuildSaveGame* Save = UGameplayStatics::DoesSaveGameExist(Slot, 0) ? Cast<UBlockBuildSaveGame>(UGameplayStatics::LoadGameFromSlot(Slot, 0)) : nullptr;
		if (!Save || Save->Pieces.Num() == 0)
		{
			Notify(LOCTEXT("ShareEmpty", "Nothing to export yet: build something in Play first."), false);
			return;
		}

		TMap<FString, int32> MeshIndex;
		TArray<TSharedPtr<FJsonValue>> Catalog;
		TArray<TSharedPtr<FJsonValue>> PiecesJson;
		TMap<FString, int32> TypeCounts;
		for (const FBlockBuildPiece& Piece : Save->Pieces)
		{
			int32 Index = INDEX_NONE;
			if (const int32* Found = MeshIndex.Find(Piece.MeshPath))
			{
				Index = *Found;
			}
			else
			{
				const FString Name = FPaths::GetBaseFilename(Piece.MeshPath);
				const FString Category = CategoryFor(Piece.MeshPath);
				TSharedPtr<FJsonObject> Item = MakeShared<FJsonObject>();
				Item->SetStringField(TEXT("mesh"), Piece.MeshPath);
				Item->SetStringField(TEXT("name"), Name);
				Item->SetStringField(TEXT("category"), Category);
				Item->SetStringField(TEXT("type"), TypeTag(Name, Category));
				if (const UStaticMesh* Mesh = LoadObject<UStaticMesh>(nullptr, *Piece.MeshPath))
				{
					const FBox Box = Mesh->GetBoundingBox();
					const FVector Size = Box.GetSize();
					Item->SetArrayField(TEXT("size_cm"), Vec3(Size.X, Size.Y, Size.Z));
					Item->SetArrayField(TEXT("min_cm"), Vec3(Box.Min.X, Box.Min.Y, Box.Min.Z));
				}
				else
				{
					Item->SetBoolField(TEXT("missing"), true);
				}
				Index = Catalog.Num();
				MeshIndex.Add(Piece.MeshPath, Index);
				Catalog.Add(MakeShared<FJsonValueObject>(Item));
			}
			const TSharedPtr<FJsonObject>& Item = Catalog[Index]->AsObject();
			TypeCounts.FindOrAdd(Item->GetStringField(TEXT("type"))) += 1;

			const FVector L = Piece.Transform.GetLocation();
			const FRotator Rot = Piece.Transform.Rotator();
			const FVector Sc = Piece.Transform.GetScale3D();
			TSharedPtr<FJsonObject> P = MakeShared<FJsonObject>();
			P->SetNumberField(TEXT("m"), Index);
			P->SetArrayField(TEXT("loc"), Vec3(L.X, L.Y, L.Z));
			P->SetArrayField(TEXT("rot"), Vec3(Rot.Pitch, Rot.Yaw, Rot.Roll));
			if (!Sc.Equals(FVector::OneVector, 0.001))
			{
				P->SetArrayField(TEXT("scale"), Vec3(Sc.X, Sc.Y, Sc.Z));
			}
			PiecesJson.Add(MakeShared<FJsonValueObject>(P));
		}

		// recorded sessions (only there if "Record build sessions" was turned on)
		TArray<TSharedPtr<FJsonValue>> Sessions;
		const FString SessionDir = FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("BlockBuild"), TEXT("Sessions"));
		TArray<FString> SessionFiles;
		IFileManager::Get().FindFiles(SessionFiles, *FPaths::Combine(SessionDir, TEXT("*.json")), true, false);
		SessionFiles.Sort();
		for (const FString& F : SessionFiles)
		{
			FString Text;
			TSharedPtr<FJsonObject> Obj;
			if (FFileHelper::LoadFileToString(Text, *FPaths::Combine(SessionDir, F)))
			{
				const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Text);
				if (FJsonSerializer::Deserialize(Reader, Obj) && Obj.IsValid())
				{
					Sessions.Add(MakeShared<FJsonValueObject>(Obj));
				}
			}
		}

		TSharedPtr<FJsonObject> Types = MakeShared<FJsonObject>();
		for (const TPair<FString, int32>& T : TypeCounts)
		{
			Types->SetNumberField(T.Key, T.Value);
		}
		TSharedPtr<FJsonObject> Root = MakeShared<FJsonObject>();
		Root->SetStringField(TEXT("schema"), TEXT("blockbuildkit.share/1"));
		Root->SetStringField(TEXT("plugin_version"), PluginVersion());
		Root->SetStringField(TEXT("engine_version"), FEngineVersion::Current().ToString(EVersionComponent::Patch));
		Root->SetStringField(TEXT("exported_utc"), FDateTime::UtcNow().ToIso8601());
		Root->SetStringField(TEXT("license"), TEXT("CC0-1.0 (when shared through the Share your build form)"));
		Root->SetStringField(TEXT("contents"), TEXT("Piece names, folders, sizes, positions and recorded build steps only. No user name, computer name or asset files."));
		Root->SetNumberField(TEXT("piece_count"), PiecesJson.Num());
		Root->SetObjectField(TEXT("type_counts"), Types);
		Root->SetArrayField(TEXT("catalog"), Catalog);
		Root->SetArrayField(TEXT("pieces"), PiecesJson);
		Root->SetNumberField(TEXT("session_count"), Sessions.Num());
		Root->SetArrayField(TEXT("sessions"), Sessions);

		FString Out;
		const TSharedRef<TJsonWriter<>> W = TJsonWriterFactory<>::Create(&Out);
		FJsonSerializer::Serialize(Root.ToSharedRef(), W);
		const FString Dir = FPaths::ConvertRelativePathToFull(FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("BlockBuild"), TEXT("Share")));
		IFileManager::Get().MakeDirectory(*Dir, true);
		const FString File = FPaths::Combine(Dir, FString::Printf(TEXT("build_%s.json"), *FDateTime::Now().ToString(TEXT("%Y-%m-%d_%H-%M-%S"))));
		if (!FFileHelper::SaveStringToFile(Out, *File))
		{
			Notify(LOCTEXT("ShareWriteFailed", "Could not write the share file."), false);
			return;
		}
		FPlatformProcess::ExploreFolder(*File);

		const UBlockBuildSettings* Settings = GetDefault<UBlockBuildSettings>();
		const FString Url = Settings ? Settings->ShareSubmitURL : FString();
		FNotificationInfo Info(FText::Format(LOCTEXT("ShareDone", "Exported {0} pieces and {1} recorded sessions to {2}. Sharing it is up to you."),
			FText::AsNumber(PiecesJson.Num()), FText::AsNumber(Sessions.Num()), FText::FromString(FPaths::GetCleanFilename(File))));
		Info.ExpireDuration = 12.f;
		Info.bUseSuccessFailIcons = true;
		if (!Url.IsEmpty())
		{
			Info.HyperlinkText = LOCTEXT("ShareLink", "Share it (opens the form)");
			Info.Hyperlink = FSimpleDelegate::CreateLambda([Url]() { FPlatformProcess::LaunchURL(*Url, nullptr, nullptr); });
		}
		TSharedPtr<SNotificationItem> Item = FSlateNotificationManager::Get().AddNotification(Info);
		if (Item.IsValid())
		{
			Item->SetCompletionState(SNotificationItem::CS_Success);
		}
	}

	/** Deletes the saved build (asks first). Pieces already baked into a level are not touched. */
	static void ClearSave()
	{
		const FString Slot = SlotName();
		if (!UGameplayStatics::DoesSaveGameExist(Slot, 0))
		{
			Notify(LOCTEXT("NothingToClear", "There is no saved build."), true);
			return;
		}
		const EAppReturnType::Type Answer = FMessageDialog::Open(EAppMsgType::YesNo,
			FText::Format(LOCTEXT("ConfirmClear", "Delete the saved build \"{0}\"? Pieces already baked into a level stay."), FText::FromString(Slot)));
		if (Answer != EAppReturnType::Yes)
		{
			return;
		}
		const bool bOk = UGameplayStatics::DeleteGameInSlot(Slot, 0);
		Notify(bOk ? LOCTEXT("Cleared", "Saved build deleted.") : LOCTEXT("ClearFailed", "Could not delete the saved build."), bOk);
	}
}

static FAutoConsoleCommand GBlockBuildShareCmd(
	TEXT("blockbuild.share"),
	TEXT("Export the saved Block Build (and recorded sessions) to Saved/BlockBuild/Share for sharing (editor, not during Play)."),
	FConsoleCommandDelegate::CreateStatic(&BlockBuildKitEditor::ExportForSharing));

static FAutoConsoleCommand GBlockBuildBakeCmd(
	TEXT("blockbuild.bake"),
	TEXT("Bake the saved Block Build into the open level as Static Mesh Actors (editor, not during Play)."),
	FConsoleCommandDelegate::CreateStatic(&BlockBuildKitEditor::BakeIntoLevel));

class FBlockBuildKitEditorModule : public IModuleInterface
{
public:
	virtual void StartupModule() override
	{
		UToolMenus::RegisterStartupCallback(FSimpleMulticastDelegate::FDelegate::CreateRaw(this, &FBlockBuildKitEditorModule::RegisterMenus));
	}

	virtual void ShutdownModule() override
	{
		UToolMenus::UnRegisterStartupCallback(this);
		UToolMenus::UnregisterOwner(this);
	}

private:
	void RegisterMenus()
	{
		FToolMenuOwnerScoped OwnerScoped(this);
		UToolMenu* Menu = UToolMenus::Get()->ExtendMenu("LevelEditor.MainMenu.Tools");
		if (!Menu)
		{
			return;
		}
		FToolMenuSection& Section = Menu->FindOrAddSection("BlockBuildKit", LOCTEXT("Section", "Block Build Kit"));
		Section.AddMenuEntry("BlockBuildKit_Bake",
			LOCTEXT("Bake", "Bake build into level"),
			LOCTEXT("BakeTip", "Turn everything you built in Play (build mode) into normal Static Mesh Actors in the open level."),
			FSlateIcon(),
			FUIAction(FExecuteAction::CreateStatic(&BlockBuildKitEditor::BakeIntoLevel)));
		Section.AddMenuEntry("BlockBuildKit_Share",
			LOCTEXT("Share", "Export build for sharing"),
			LOCTEXT("ShareTip", "Write the saved build (and recorded sessions, if recording is on) to one .json file you can choose to share. No personal info is included."),
			FSlateIcon(),
			FUIAction(FExecuteAction::CreateStatic(&BlockBuildKitEditor::ExportForSharing)));
		Section.AddMenuEntry("BlockBuildKit_Clear",
			LOCTEXT("Clear", "Clear saved build"),
			LOCTEXT("ClearTip", "Delete the saved build so Play starts empty. Baked actors in levels are not touched."),
			FSlateIcon(),
			FUIAction(FExecuteAction::CreateStatic(&BlockBuildKitEditor::ClearSave)));
	}
};

#undef LOCTEXT_NAMESPACE

IMPLEMENT_MODULE(FBlockBuildKitEditorModule, BlockBuildKitEditor)
