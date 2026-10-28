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
		Section.AddMenuEntry("BlockBuildKit_Clear",
			LOCTEXT("Clear", "Clear saved build"),
			LOCTEXT("ClearTip", "Delete the saved build so Play starts empty. Baked actors in levels are not touched."),
			FSlateIcon(),
			FUIAction(FExecuteAction::CreateStatic(&BlockBuildKitEditor::ClearSave)));
	}
};

#undef LOCTEXT_NAMESPACE

IMPLEMENT_MODULE(FBlockBuildKitEditorModule, BlockBuildKitEditor)
