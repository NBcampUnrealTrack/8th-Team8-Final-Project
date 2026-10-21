#include "SFGameInstance.h"

// 추가한 부분
#include "Datas/Building/SFBuildingCatalog.h"
#include "Kismet/GameplayStatics.h"
#include "Logging/SFLog.h"
#include "Game/Subsystem/SFDataTableManager.h"
#include "Datas/Stage/SFStageData.h"
#include "Game/Subsystem/SFBuildingAssetSubsystem.h"
#include "Engine/World.h"
#include "Game/Save/SFCampaignSaveGame.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"

void USFGameInstance::Init()
{
    Super::Init();
    // QA can opt into a separate slot without touching the player's campaign.
    FString SlotOverride;
    if (CampaignSaveSlotName == TEXT("SFCampaignProgress") &&
        FParse::Value(FCommandLine::Get(), TEXT("SFCampaignSaveSlot="), SlotOverride) &&
        !SlotOverride.IsEmpty() && !SlotOverride.GetCharArray().ContainsByPredicate([](TCHAR C)
        { return C != 0 && !FChar::IsAlnum(C) && C != TEXT('_') && C != TEXT('-'); }))
    {
        CampaignSaveSlotName = SlotOverride;
    }
    LoadCampaignProgress();
}

void USFGameInstance::Shutdown()
{
    if (bCampaignProgressDirty) SaveCampaignProgress();
    Super::Shutdown();
}

int32 USFGameInstance::GetCampaignStageCount() const
{
    int32 Count = 0;
    if (!IsValid(StageDataTable)) return Count;
    // Only numbered, contiguous campaign rows participate; prototype aliases do not.
    while (const FSFStageData* Stage = StageDataTable->FindRow<FSFStageData>(
        FName(*FString::Printf(TEXT("Stage%02d"), Count + 1)), TEXT("CampaignProgress"), false))
    {
        if (Stage->StageOrder != Count + 1 || Stage->StageLevel.IsNull()) break;
        ++Count;
    }
    return Count;
}

FName USFGameInstance::GetNextCampaignStageRowName() const
{
    return HighestClearedStageOrder < GetCampaignStageCount()
        ? FName(*FString::Printf(TEXT("Stage%02d"), HighestClearedStageOrder + 1)) : NAME_None;
}

void USFGameInstance::LoadCampaignProgress()
{
    HighestClearedStageOrder = 0;
    bUnreadStoryUnlock = false;
    bCampaignProgressDirty = false;
    bCampaignSaveWritable = true;
    for (const FString& Slot : { CampaignSaveSlotName, CampaignSaveSlotName + TEXT("_Backup") })
    {
        if (!UGameplayStatics::DoesSaveGameExist(Slot, 0)) continue;
        const USFCampaignSaveGame* Save = Cast<USFCampaignSaveGame>(UGameplayStatics::LoadGameFromSlot(Slot, 0));
        if (Save && Save->SaveVersion > USFCampaignSaveGame::CurrentVersion)
        {
            // An older build must never overwrite a newer build's progress.
            bCampaignSaveWritable = false;
            SF_LOG(Warning, "[Campaign] Newer save version in %s; keeping the file untouched.", *Slot);
            return;
        }
        if (!Save || Save->SaveVersion != USFCampaignSaveGame::CurrentVersion ||
            Save->HighestClearedStageOrder < 0 || Save->HighestClearedStageOrder > GetCampaignStageCount())
        {
            SF_LOG(Warning, "[Campaign] Invalid save in %s; trying the backup.", *Slot);
            continue;
        }
        HighestClearedStageOrder = Save->HighestClearedStageOrder;
        bUnreadStoryUnlock = HighestClearedStageOrder >= 3 && Save->bUnreadStoryUnlock;
        // A recovered backup is written back on the next save or clean shutdown.
        bCampaignProgressDirty = Slot != CampaignSaveSlotName;
        SF_LOG(Log, "[Campaign] Loaded %s: highest cleared stage=%d", *Slot, HighestClearedStageOrder);
        return;
    }
}

bool USFGameInstance::SaveCampaignProgress()
{
    if (!bCampaignSaveWritable) return false;
    USFCampaignSaveGame* Save = Cast<USFCampaignSaveGame>(
        UGameplayStatics::CreateSaveGameObject(USFCampaignSaveGame::StaticClass()));
    if (!Save) return false;
    Save->HighestClearedStageOrder = HighestClearedStageOrder;
    Save->bUnreadStoryUnlock = bUnreadStoryUnlock;
    // This tiny save runs at victory/story screens. Finish before level travel or exit.
    if (!UGameplayStatics::SaveGameToSlot(Save, CampaignSaveSlotName, 0))
    {
        SF_LOG(Warning, "[Campaign] Auto-save failed in %s; will retry at the next save or shutdown.", *CampaignSaveSlotName);
        return false;
    }
    bCampaignProgressDirty = false;
    if (!UGameplayStatics::SaveGameToSlot(Save, CampaignSaveSlotName + TEXT("_Backup"), 0))
        SF_LOG(Warning, "[Campaign] Primary save succeeded, but backup save failed.");
    SF_LOG(Log, "[Campaign] Saved highest cleared stage=%d", HighestClearedStageOrder);
    return true;
}

void USFGameInstance::MarkStoryUnlockRead()
{
    if (!bUnreadStoryUnlock) return;
    bUnreadStoryUnlock = false;
    bCampaignProgressDirty = true;
    SaveCampaignProgress();
}

#if WITH_EDITOR
void USFGameInstance::ResolveEditorStageSelection(const UWorld* PlayWorld)
{
    if (!PlayWorld || PlayWorld->WorldType != EWorldType::PIE || !IsValid(StageDataTable))
    {
        return;
    }

    const FString MapPackage = UWorld::RemovePIEPrefix(PlayWorld->GetOutermost()->GetName());
    const FSFStageData* Selected = StageDataTable->FindRow<FSFStageData>(SelectedStageRowName, TEXT("DirectPIE"), false);
    if (Selected && Selected->StageLevel.ToSoftObjectPath().GetLongPackageName() == MapPackage)
    {
        // Keep a valid lobby selection (including its selected race).
        return;
    }

    FName MatchedRow;
    const FSFStageData* MatchedData = nullptr;
    for (const FName RowName : StageDataTable->GetRowNames())
    {
        const FSFStageData* Row = StageDataTable->FindRow<FSFStageData>(RowName, TEXT("DirectPIE"), false);
        if (!Row || Row->StageLevel.ToSoftObjectPath().GetLongPackageName() != MapPackage) continue;
        if (MatchedData)
        {
            // Old prototype aliases can share a map with a numbered campaign stage.
            // Direct PIE should use the campaign row; an explicit lobby selection
            // was already preserved above.
            const bool bCanonicalRow = RowName == FName(*FString::Printf(TEXT("Stage%02d"), Row->StageOrder));
            const bool bCanonicalMatch = MatchedRow == FName(*FString::Printf(TEXT("Stage%02d"), MatchedData->StageOrder));
            if (bCanonicalRow != bCanonicalMatch)
            {
                if (bCanonicalRow)
                {
                    MatchedRow = RowName;
                    MatchedData = Row;
                }
                continue;
            }
            SF_LOG(Warning, "[SFGameInstance] Direct PIE map has multiple stage rows: %s", *MapPackage);
            return;
        }
        MatchedRow = RowName;
        MatchedData = Row;
    }

    if (!MatchedData || (MatchedData->PlayerRace != ESFRace::None && !SelectRace(MatchedData->PlayerRace))) return;
    SelectedStageRowName = MatchedRow;
    SF_LOG(Log, "[SFGameInstance] Direct PIE stage resolved: %s", *SelectedStageRowName.ToString());
}
#endif

UDataTable* USFGameInstance::GetDataTableType(EDataTableType TableType) const
{
	switch (TableType)
	{
	case EDataTableType::UI:
		{
			return UIDataTable;
		}
	case EDataTableType::Unit:
		{
			return UnitsDataTable;
		}
	case EDataTableType::Stage: // 추가부분
		{
			return StageDataTable;
		}
	case EDataTableType::ProductionUI:
	    {
	        return ProductionUIDataTable;
	    }
		default:
		{
			return nullptr;
		}
	}
}

// 추가된 부분
bool USFGameInstance::MoveToTitle()
{
    if (USFBuildingAssetSubsystem* AssetSubsystem =
    GetSubsystem<USFBuildingAssetSubsystem>())
    {
        AssetSubsystem->ReleaseStageAssets();
    }
    
	return TravelToLevel(TitleLevel);
}

bool USFGameInstance::MoveToLobby()
{
    if (USFBuildingAssetSubsystem* AssetSubsystem =
    GetSubsystem<USFBuildingAssetSubsystem>())
    {
        AssetSubsystem->ReleaseStageAssets();
    }
    
	return TravelToLevel(LobbyLevel);
}

bool USFGameInstance::SelectStage(FName StageRowName)
{
    SelectedStageRowName = NAME_None;

    if (StageRowName.IsNone())
    {
        SF_LOG(Warning, "[SFGameInstance] Stage Row Name is None.");

        return false;
    }

    USFDataTableManager* DataTableManager = GetSubsystem<USFDataTableManager>();

    if (!IsValid(DataTableManager))
    {
        SF_LOG(Error, "[SFGameInstance] DataTableManager is invalid.");

        return false;
    }

    const FSFStageData* StageData = DataTableManager->GetDataRow<FSFStageData>(EDataTableType::Stage, StageRowName, TEXT("SelectStage"));

    if (!StageData)
    {
        SF_LOG(Warning, "[SFGameInstance] Stage Row was not found: %s", *StageRowName.ToString());

        return false;
    }

    if (StageData->StageLevel.IsNull())
    {
        SF_LOG(Warning, "[SFGameInstance] Stage Level is not assigned: %s", *StageRowName.ToString());

        return false;
    }

    // 추가부분 26.08.10
    if (GetStageProgressState(StageRowName)
        == ESFStageProgressState::Locked)
    {
        SF_LOG(Warning, "[SFGameInstance] Locked stage cannot be selected: %s", *StageRowName.ToString());

        return false;
    }
    
    // stage테이블의 default플레이어 종족값 캐싱
    if (StageData->PlayerRace == ESFRace::None)
    {
        // elseif 때문에 7스테이지만 
        // 이전 스테이지의 종족값 제거
        SelectedRace = ESFRace::None;
    }
    else if (!SelectRace(StageData->PlayerRace))
    {
        SF_LOG(
            Error,
            "[SFGameInstance] 플레이어 종족 설정 실패. "
            "Stage=%s, Race=%s",
            *StageRowName.ToString(),
            *UEnum::GetValueAsString(
                StageData->PlayerRace));

        return false;
    }
    SelectedStageRowName = StageRowName;

    SF_LOG(Log, "[SFGameInstance] Stage selected: %s, DisplayName: %s", *SelectedStageRowName.ToString(), *StageData->DisplayName.ToString());

    return true;
}

FName USFGameInstance::GetSelectedStageRowName() const
{
    return SelectedStageRowName;
}

bool USFGameInstance::MoveToStage()
{
    if (SelectedStageRowName.IsNone())
    {
        SF_LOG(Warning, "[SFGameInstance] No stage has been selected.");

        return false;
    }

    USFDataTableManager* DataTableManager = GetSubsystem<USFDataTableManager>();

    if (!IsValid(DataTableManager))
    {
        SF_LOG(Error, "[SFGameInstance] DataTableManager is invalid.");

        return false;
    }

    const FSFStageData* StageData = DataTableManager->GetDataRow<FSFStageData>(EDataTableType::Stage, SelectedStageRowName, TEXT("MoveToStage"));

    if (!StageData)
    {
        SF_LOG(Warning, "[SFGameInstance] Selected Stage Row was not found: %s", *SelectedStageRowName.ToString());

        return false;
    }

    if (SelectedRace == ESFRace::None)
    {
        SF_LOG(
            Warning,
            "[SFGameInstance] 종족을 선택해야 스테이지로 이동할 수 있습니다.");

        return false;
    }
    
    return TravelToLevel(StageData->StageLevel);
}

// 추가부분 26.08.10
ESFStageProgressState USFGameInstance::GetStageProgressState(FName StageRowName) const
{
    if (StageRowName.IsNone())
    {
        return ESFStageProgressState::Locked;
    }

    const USFDataTableManager* DataTableManager =
        GetSubsystem<USFDataTableManager>();

    if (!IsValid(DataTableManager))
    {
        return ESFStageProgressState::Locked;
    }

    const FSFStageData* StageData =
        DataTableManager->GetDataRow<FSFStageData>(
            EDataTableType::Stage,
            StageRowName,
            TEXT("GetStageProgressState")
        );

    if (!StageData || StageData->StageOrder <= 0)
    {
        return ESFStageProgressState::Locked;
    }

    if (StageData->StageOrder <= HighestClearedStageOrder)
    {
        return ESFStageProgressState::Cleared;
    }

    if (StageData->StageOrder == HighestClearedStageOrder + 1)
    {
        return ESFStageProgressState::Unlocked;
    }

    return ESFStageProgressState::Locked;
}

// 추가 부분 26.08.10
bool USFGameInstance::MarkStageCleared(FName StageRowName)
{
    if (StageRowName.IsNone())
    {
        SF_LOG(Warning, "[SFGameInstance] Stage Row Name is None.");
        return false;
    }

    USFDataTableManager* DataTableManager =
        GetSubsystem<USFDataTableManager>();

    if (!IsValid(DataTableManager))
    {
        SF_LOG(Error, "[SFGameInstance] DataTableManager is invalid.");
        return false;
    }

    const FSFStageData* StageData =
        DataTableManager->GetDataRow<FSFStageData>(
            EDataTableType::Stage,
            StageRowName,
            TEXT("MarkStageCleared")
        );

    if (!StageData || StageData->StageOrder <= 0)
    {
        SF_LOG(
            Warning,
            "[SFGameInstance] Invalid stage data: %s",
            *StageRowName.ToString()
        );

        return false;
    }

    if (StageData->StageOrder > HighestClearedStageOrder + 1)
    {
        SF_LOG(
            Warning,
            "[SFGameInstance] Locked stage cannot be cleared: %s",
            *StageRowName.ToString()
        );

        return false;
    }

    const int32 PreviousHighest = HighestClearedStageOrder;
    if (HighestClearedStageOrder < 3 && StageData->StageOrder >= 3)
        bUnreadStoryUnlock = true;
    HighestClearedStageOrder =
        FMath::Max(HighestClearedStageOrder, StageData->StageOrder);

    if (HighestClearedStageOrder != PreviousHighest) bCampaignProgressDirty = true;
    if (bCampaignProgressDirty) SaveCampaignProgress();

    if (HighestClearedStageOrder != PreviousHighest) NotifyPVEAchievementUnlocked(StageRowName);

    SF_LOG(
        Log,
        "[SFGameInstance] Stage cleared: %s, HighestClearedOrder: %d",
        *StageRowName.ToString(),
        HighestClearedStageOrder
    );

    return true;
}

bool USFGameInstance::RetryCurrentStage()
{   // 로딩 내용 비우기
    if (USFBuildingAssetSubsystem* AssetSubsystem =
        GetSubsystem<USFBuildingAssetSubsystem>())
    {
        AssetSubsystem->ReleaseStageAssets();
    }
    // 로비로
    return MoveToStage();
}

bool USFGameInstance::TravelToLevel(const TSoftObjectPtr<UWorld>& TargetLevel)
{
	if (TargetLevel.IsNull())
	{
		SF_LOG(Error, "[SFGameInstance] Target level is not assigned.");

		return false;
	}

	if (!IsValid(GetWorld()))
	{
		SF_LOG(Error, "[SFGameInstance] World is invalid");

		return false;
	}

	const FString TargetLevelPath = TargetLevel.ToSoftObjectPath().ToString();

	SF_LOG(Log, "[SFGameInstance] Level travel requested : %s", *TargetLevelPath);

	UGameplayStatics::OpenLevelBySoftObjectPtr(this, TargetLevel);

	return true;
}

bool USFGameInstance::SelectRace(ESFRace InRace)
{   // 간단한 enum저장 함수
    const TSoftObjectPtr<USFBuildingCatalog>* Catalog =
        CatalogsByRace.Find(InRace);

    if (!Catalog || Catalog->IsNull())
    {
        SF_LOG(
            Error,
            "[SFGameInstance] 선택한 종족의 카탈로그가 없습니다. Race=%d",
            static_cast<int32>(InRace));

        return false;
    }

    SelectedRace = InRace;
    return true;
}

TSoftObjectPtr<USFBuildingCatalog> USFGameInstance::GetBuildingCatalogAsset(ESFRace InRace) const
{
    const TSoftObjectPtr<USFBuildingCatalog>* Found = CatalogsByRace.Find(InRace);
    return Found? *Found : TSoftObjectPtr<USFBuildingCatalog>();
}
