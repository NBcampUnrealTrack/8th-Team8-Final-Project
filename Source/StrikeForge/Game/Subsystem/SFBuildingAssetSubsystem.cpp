#include "SFBuildingAssetSubsystem.h"

#include "Datas/Building/SFBuildingCatalog.h"
#include "Datas/Building/SFBuildingDefinition.h"
#include "Engine/AssetManager.h"
#include "Engine/StreamableManager.h"
#include "Engine/Texture2D.h"
#include "Game/GameInstance/SFGameInstance.h"

namespace
{
	/** Definition 경로와 Tier로 중복 요청 식별 키를 만든다.
	 * RequestBuildingAssetsAsync에서 호출되며 Handle·Callback Map 조회에 사용된다. */
	FString MakeBuildingAssetRequestKey(
		const USFBuildingDefinition* InDefinition,
		int32 InTierIndex)
	{
		return FString::Printf(
			TEXT("%s|Tier=%d"),
			*GetPathNameSafe(InDefinition),
			InTierIndex);
	}
}

void USFBuildingAssetSubsystem::Deinitialize()
{
	ReleaseStageAssets();
	
	Super::Deinitialize();
}

void USFBuildingAssetSubsystem::RequestRaceDataAsync(ESFRace InRace, FSFOnRaceBuildingDataReady OnComplete)
{
	if (InRace == ESFRace::None)
	{
		OnComplete.ExecuteIfBound(InRace, false);
		return;
	}

	if (IsRaceDataReady(InRace))
	{
		OnComplete.ExecuteIfBound(InRace, true);
		return;
	}

	PendingRaceCallbacks
		.FindOrAdd(InRace)
		.Add(MoveTemp(OnComplete));

	if (ActiveRaceHandles.Contains(InRace))
	{
		return;
	}

	const USFGameInstance* SFGameInstance =
		Cast<USFGameInstance>(GetGameInstance());

	if (!IsValid(SFGameInstance))
	{
		CompleteRaceRequest(InRace, false);
		return;
	}

	const TSoftObjectPtr<USFBuildingCatalog> CatalogRef =
		SFGameInstance->GetBuildingCatalogAsset(InRace);

	if (CatalogRef.IsNull())
	{
		CompleteRaceRequest(InRace, false);
		return;
	}

	LoadedRaceData.Remove(InRace);

	TSharedPtr<FStreamableHandle> Handle =
		UAssetManager::GetStreamableManager()
		.RequestAsyncLoad(
			CatalogRef.ToSoftObjectPath(),
			FStreamableDelegate::CreateUObject(
				this,
				&ThisClass::HandleCatalogLoaded,
				InRace));

	if (!Handle.IsValid())
	{
		CompleteRaceRequest(InRace, false);
		return;
	}

	ActiveRaceHandles.Add(
		InRace,
		MoveTemp(Handle));
}

void USFBuildingAssetSubsystem::RequestBuildingAssetsAsync(
	USFBuildingDefinition* InDefinition,
	int32 InTierIndex,
	FSFOnBuildingAssetsReady OnComplete)
{
	if (!IsValid(InDefinition) ||
        InTierIndex < 0)
    {
        OnComplete.ExecuteIfBound(
            InDefinition,
            InTierIndex,
            false);

        return;
    }

    TArray<FSoftObjectPath> RuntimeAssetPaths;

    if (!InDefinition->GetRuntimeAssetPaths(
        InTierIndex,
        RuntimeAssetPaths))
    {
        OnComplete.ExecuteIfBound(
            InDefinition,
            InTierIndex,
            false);

        return;
    }

    const FString RequestKey =
        MakeBuildingAssetRequestKey(
            InDefinition,
            InTierIndex);

    if (ActiveBuildingHandles.Contains(RequestKey))
    {
        if (InDefinition->AreRuntimeAssetsLoaded(
            InTierIndex))
        {
            OnComplete.ExecuteIfBound(
                InDefinition,
                InTierIndex,
                true);
        }
        else
        {
            PendingBuildingCallbacks
                .FindOrAdd(RequestKey)
                .Add(MoveTemp(OnComplete));
        }

        return;
    }

    PendingBuildingCallbacks
        .FindOrAdd(RequestKey)
        .Add(MoveTemp(OnComplete));

    const TWeakObjectPtr<USFBuildingDefinition>
        WeakDefinition(InDefinition);

    TSharedPtr<FStreamableHandle> Handle =
        UAssetManager::GetStreamableManager()
        .RequestAsyncLoad(
            RuntimeAssetPaths,
            FStreamableDelegate::CreateWeakLambda(
                this,
                [this,
                 WeakDefinition,
                 InTierIndex,
                 RequestKey]()
                {
                    USFBuildingDefinition* Definition =
                        WeakDefinition.Get();

                    const bool bSuccess =
                        IsValid(Definition) &&
                        Definition->AreRuntimeAssetsLoaded(
                            InTierIndex);

                    CompleteBuildingRequest(
                        RequestKey,
                        Definition,
                        InTierIndex,
                        bSuccess);
                }));

    if (!Handle.IsValid())
    {
        CompleteBuildingRequest(
            RequestKey,
            InDefinition,
            InTierIndex,
            false);

        return;
    }

    ActiveBuildingHandles.Add(
        RequestKey,
        MoveTemp(Handle));
}

USFBuildingCatalog* USFBuildingAssetSubsystem::GetLoadedCatalog(ESFRace InRace) const
{
	const FSFLoadedRaceBuildingData* LoadedData =
		LoadedRaceData.Find(InRace);

	return LoadedData &&
		   LoadedData->bReady &&
		   IsValid(LoadedData->Catalog)
		? LoadedData->Catalog.Get()
		: nullptr;
}

bool USFBuildingAssetSubsystem::IsRaceDataReady(ESFRace InRace) const
{
	const FSFLoadedRaceBuildingData* LoadedData =
		LoadedRaceData.Find(InRace);

	if (!LoadedData ||
		!LoadedData->bReady ||
		!IsValid(LoadedData->Catalog) ||
		LoadedData->Definitions.IsEmpty())
	{
		return false;
	}

	for (const TObjectPtr<USFBuildingDefinition>& Definition
		: LoadedData->Definitions)
	{
		if (!IsValid(Definition))
		{
			return false;
		}
	}

	return true;
}

void USFBuildingAssetSubsystem::ReleaseStageAssets()
{
	for (TPair<ESFRace, TSharedPtr<FStreamableHandle>>& Pair
		: ActiveRaceHandles)
	{
		if (!Pair.Value.IsValid())
		{
			continue;
		}

		if (Pair.Value->IsLoadingInProgress())
		{
			Pair.Value->CancelHandle();
		}
		else
		{
			Pair.Value->ReleaseHandle();
		}
	}

	for (TPair<FString, TSharedPtr<FStreamableHandle>>& Pair
		: ActiveBuildingHandles)
	{
		if (!Pair.Value.IsValid())
		{
			continue;
		}

		if (Pair.Value->IsLoadingInProgress())
		{
			Pair.Value->CancelHandle();
		}
		else
		{
			Pair.Value->ReleaseHandle();
		}
	}

	PendingRaceCallbacks.Reset();
	PendingBuildingCallbacks.Reset();

	ActiveRaceHandles.Reset();
	ActiveBuildingHandles.Reset();
	LoadedRaceData.Reset();
}

void USFBuildingAssetSubsystem::HandleCatalogLoaded(ESFRace InRace)
{
	const USFGameInstance* SFGameInstance =
		Cast<USFGameInstance>(GetGameInstance());

	if (!IsValid(SFGameInstance))
	{
		CompleteRaceRequest(InRace, false);
		return;
	}

	USFBuildingCatalog* Catalog =
		SFGameInstance
		->GetBuildingCatalogAsset(InRace)
		.Get();

	if (!IsValid(Catalog))
	{
		CompleteRaceRequest(InRace, false);
		return;
	}

	FSFLoadedRaceBuildingData& LoadedData =
		LoadedRaceData.FindOrAdd(InRace);

	LoadedData.Catalog = Catalog;
	LoadedData.Definitions.Reset();
	LoadedData.UIAssets.Reset();
	LoadedData.bReady = false;

	TArray<FSoftObjectPath> DefinitionPaths;
	Catalog->GetDefinitionPaths(DefinitionPaths);

	if (DefinitionPaths.IsEmpty())
	{
		CompleteRaceRequest(InRace, false);
		return;
	}

	TSharedPtr<FStreamableHandle> Handle =
		UAssetManager::GetStreamableManager()
		.RequestAsyncLoad(
			DefinitionPaths,
			FStreamableDelegate::CreateUObject(
				this,
				&ThisClass::HandleDefinitionsLoaded,
				InRace));

	if (!Handle.IsValid())
	{
		CompleteRaceRequest(InRace, false);
		return;
	}

	ActiveRaceHandles.Add(
		InRace,
		MoveTemp(Handle));
}

void USFBuildingAssetSubsystem::HandleDefinitionsLoaded(ESFRace InRace)
{
	FSFLoadedRaceBuildingData* LoadedData =
        LoadedRaceData.Find(InRace);

    if (!LoadedData ||
        !IsValid(LoadedData->Catalog))
    {
        CompleteRaceRequest(InRace, false);
        return;
    }

    LoadedData->Definitions.Reset();

    auto AddLoadedDefinition =
        [LoadedData](
            const TSoftObjectPtr<USFBuildingDefinition>& DefinitionRef)
        {
            if (DefinitionRef.IsNull())
            {
                return false;
            }

            USFBuildingDefinition* Definition =
                DefinitionRef.Get();

            if (!IsValid(Definition))
            {
                return false;
            }

            LoadedData->Definitions.AddUnique(Definition);
            return true;
        };

    bool bAllDefinitionsLoaded =
        AddLoadedDefinition(
            LoadedData->Catalog->NexusDefinition);

    for (const TSoftObjectPtr<USFBuildingDefinition>& DefinitionRef
        : LoadedData->Catalog->BuildableDefinitions)
    {
        bAllDefinitionsLoaded &=
            AddLoadedDefinition(DefinitionRef);
    }

    if (!bAllDefinitionsLoaded ||
        LoadedData->Definitions.IsEmpty())
    {
        CompleteRaceRequest(InRace, false);
        return;
    }

    TArray<FSoftObjectPath> UIAssetPaths;

    for (const TObjectPtr<USFBuildingDefinition>& Definition
        : LoadedData->Definitions)
    {
        Definition->GetUIAssetPaths(UIAssetPaths);
    }

    if (UIAssetPaths.IsEmpty())
    {
        CompleteRaceRequest(InRace, true);
        return;
    }

    TSharedPtr<FStreamableHandle> Handle =
        UAssetManager::GetStreamableManager()
        .RequestAsyncLoad(
            UIAssetPaths,
            FStreamableDelegate::CreateUObject(
                this,
                &ThisClass::HandleUIAssetsLoaded,
                InRace));

    if (!Handle.IsValid())
    {
        CompleteRaceRequest(InRace, false);
        return;
    }

    ActiveRaceHandles.Add(
        InRace,
        MoveTemp(Handle));
}

void USFBuildingAssetSubsystem::CompleteRaceRequest(ESFRace InRace, bool bSuccess)
{
	TArray<FSFOnRaceBuildingDataReady> Callbacks;

	if (TArray<FSFOnRaceBuildingDataReady>* Found =
		PendingRaceCallbacks.Find(InRace))
	{
		Callbacks = MoveTemp(*Found);
	}

	PendingRaceCallbacks.Remove(InRace);
	ActiveRaceHandles.Remove(InRace);

	if (bSuccess)
	{
		FSFLoadedRaceBuildingData* LoadedData =
			LoadedRaceData.Find(InRace);

		bSuccess =
			LoadedData &&
			IsValid(LoadedData->Catalog) &&
			!LoadedData->Definitions.IsEmpty();

		if (bSuccess)
		{
			LoadedData->bReady = true;
		}
	}

	if (!bSuccess)
	{
		LoadedRaceData.Remove(InRace);
	}

	for (FSFOnRaceBuildingDataReady& Callback
		: Callbacks)
	{
		Callback.ExecuteIfBound(
			InRace,
			bSuccess);
	}
}

void USFBuildingAssetSubsystem::HandleUIAssetsLoaded(ESFRace InRace)
{
	FSFLoadedRaceBuildingData* LoadedData =
		LoadedRaceData.Find(InRace);

	if (!LoadedData)
	{
		CompleteRaceRequest(InRace, false);
		return;
	}

	LoadedData->UIAssets.Reset();

	for (const TObjectPtr<USFBuildingDefinition>& Definition
		: LoadedData->Definitions)
	{
		if (!IsValid(Definition))
		{
			CompleteRaceRequest(InRace, false);
			return;
		}

		if (Definition->Icon.IsNull())
		{
			continue;
		}

		UTexture2D* LoadedIcon =
			Definition->Icon.Get();

		if (!IsValid(LoadedIcon))
		{
			CompleteRaceRequest(InRace, false);
			return;
		}

		LoadedData->UIAssets.AddUnique(LoadedIcon);
	}

	CompleteRaceRequest(InRace, true);
}

void USFBuildingAssetSubsystem::CompleteBuildingRequest(const FString& InRequestKey,
	USFBuildingDefinition* InDefinition, int32 InTierIndex, bool bSuccess)
{
	TArray<FSFOnBuildingAssetsReady> Callbacks;

	if (TArray<FSFOnBuildingAssetsReady>* Found =
		PendingBuildingCallbacks.Find(InRequestKey))
	{
		Callbacks = MoveTemp(*Found);
	}

	PendingBuildingCallbacks.Remove(InRequestKey);

	// 성공한 Handle은 스테이지가 끝날 때까지 유지한다.
	if (!bSuccess)
	{
		ActiveBuildingHandles.Remove(InRequestKey);
	}

	for (FSFOnBuildingAssetsReady& Callback
		: Callbacks)
	{
		Callback.ExecuteIfBound(
			InDefinition,
			InTierIndex,
			bSuccess);
	}
}
