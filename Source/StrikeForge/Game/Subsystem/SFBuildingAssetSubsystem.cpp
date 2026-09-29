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

	PendingRaceCallbacks.FindOrAdd(InRace).Add(MoveTemp(OnComplete));

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

	// Catalog 비동기 로딩
	TSharedPtr<FStreamableHandle> Handle =
		UAssetManager::GetStreamableManager()
		.RequestAsyncLoad(
			CatalogRef.ToSoftObjectPath(),
			FStreamableDelegate::CreateUObject(
				this,
				&ThisClass::HandleCatalogLoaded,
				InRace));
	// Catalog안에도 Definition들은 soft라 로딩이 필요하다
	if (!Handle.IsValid())
	{
		CompleteRaceRequest(InRace, false);
		return;
	}

	ActiveRaceHandles.Add(InRace, MoveTemp(Handle));
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
	
	// 현재티어에 맞는 SoftRef을 싹다 모아서 RuntimeAssetPahts에 넣는다
    if (!InDefinition->GetRuntimeAssetPaths(InTierIndex,RuntimeAssetPaths))
    {
    	// 경로 모을때 오류나면 false
        OnComplete.ExecuteIfBound(InDefinition, InTierIndex, false);
        return;
    }

	// 문자 + 문자 key를 만들고 바인딩함수 콜백 델리게이트를 pending에 추가
    const FString RequestKey = MakeBuildingAssetRequestKey(InDefinition, InTierIndex);
	
	// Map에 있어서 중복처리
	if (ActiveBuildingHandles.Contains(RequestKey))
	{
		if (TArray<FSFOnBuildingAssetsReady>* PendingCallbacks =
			PendingBuildingCallbacks.Find(RequestKey))
		{
			// 이 요청은 아직 완료 처리 전이다.
			// 선택 에셋까지 로딩 시도가 끝난 뒤 함께 통지한다.
			PendingCallbacks->Add(MoveTemp(OnComplete));
		}
		else
		{
			// 완료된 요청이다. 필수 에셋의 준비 상태를 전달한다.
			const bool bSuccess =
				InDefinition->AreRuntimeAssetsLoaded(InTierIndex);

			OnComplete.ExecuteIfBound(InDefinition,	InTierIndex, bSuccess);
		}

		return;
	}

	// Map에 없어서 신규등록 > 비동기 실행해야함
    PendingBuildingCallbacks.FindOrAdd(RequestKey).Add(MoveTemp(OnComplete));

    const TWeakObjectPtr<USFBuildingDefinition>
        WeakDefinition(InDefinition);

	// 비동기 실행
	// 배열에 10개의 경로가 있으면 → 10개를 모두 로딩.
	// 10개가 전부 완료되면 → 넘겨준 완료 델리게이트가 딱 한 번 호출됨.
    TSharedPtr<FStreamableHandle> Handle =
        UAssetManager::GetStreamableManager()
        .RequestAsyncLoad(
            RuntimeAssetPaths,
            FStreamableDelegate::CreateWeakLambda( // 완료될때 실행되는 콜백함수
                this,
                [this, WeakDefinition, InTierIndex, RequestKey]()
                {
                    USFBuildingDefinition* Definition =
                        WeakDefinition.Get();

                	const bool bSuccess =
						IsValid(Definition) &&
						Definition->AreRuntimeAssetsLoaded(InTierIndex, true);

                    CompleteBuildingRequest(
                        RequestKey, // key와 연결되있는 Map에 델리게이트가 TArray로 존재
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

	// 비동기요청을 하고 Return되는 Handle저장
	// ( DA에셋이름 | Tier01 ) , FStreamableHandle 저장
	// 현재 Key로 로딩중인지 여부 판단. > 완료되면 완료콜백함수에서 remove예정
    ActiveBuildingHandles.Add(RequestKey, MoveTemp(Handle));
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
	const USFGameInstance* SFGameInstance = Cast<USFGameInstance>(GetGameInstance());
	if (!IsValid(SFGameInstance))
	{
		CompleteRaceRequest(InRace, false);
		return;
	}

	USFBuildingCatalog* Catalog = SFGameInstance->GetBuildingCatalogAsset(InRace).Get();
	if (!IsValid(Catalog))
	{
		CompleteRaceRequest(InRace, false);
		return;
	}
	
	// 진행도 구조체 종족key로 찾거나 추가
	FSFLoadedRaceBuildingData& LoadedData = LoadedRaceData.FindOrAdd(InRace);
	
	// 이시점까지 왔으면
	// catalog 로딩완료임 & 이므로 대입하면 바로 value에 저장됨
	LoadedData.Catalog = Catalog;
	LoadedData.Definitions.Reset();
	LoadedData.UIAssets.Reset();
	LoadedData.bReady = false;

	// 카탈로그 object의 내용에는 7개의 건물 softref이 있음
	TArray<FSoftObjectPath> DefinitionPaths;
	// 7개 softref 배열에 저장
	Catalog->GetDefinitionPaths(DefinitionPaths);

	if (DefinitionPaths.IsEmpty())
	{
		CompleteRaceRequest(InRace, false);
		return;
	}

	// 배열로 한꺼번에 비동기
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

	// 현재 종족의 7개 건물 softref 비동기중... handle저장
	ActiveRaceHandles.Add(InRace, MoveTemp(Handle));
	// Animal catalog.handle(1개 참조),   Animal Definitions.handle(7개 참조)
}

void USFBuildingAssetSubsystem::HandleDefinitionsLoaded(ESFRace InRace)
{
	FSFLoadedRaceBuildingData* LoadedData = LoadedRaceData.Find(InRace);

    if (!LoadedData || !IsValid(LoadedData->Catalog))
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

	// 넥서스 로드 됬는지 먼저 확인
    bool bAllDefinitionsLoaded =
        AddLoadedDefinition(
            LoadedData->Catalog->NexusDefinition);
	
	// 이후 buildable건물 확인
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
#pragma region CheckEssentialAssets
	// 선택된 종족의 Nexus와 건설 가능 건물 전체를 검사한다.
	// Anchor 배치 여부나 초기 로딩 대상 티어에 제한하지 않는다.
	bool bAllDefinitionsValid = true;

	for (const TObjectPtr<USFBuildingDefinition>& Definition
		: LoadedData->Definitions)
	{
		if (!Definition->ValidateAllTierAssetSettings())
		{
			bAllDefinitionsValid = false;
		}
	}
	if (!bAllDefinitionsValid)
	{
		UE_LOG(LogTemp, Error,
			TEXT("[SFBuildingAssetSubsystem] 종족 건물중 필수설정이 누락되있습니다. ( 건설중메쉬, 건설완료메쉬, GC )를 체크해주세요. Race=%s"),
			*UEnum::GetValueAsString(InRace));

		CompleteRaceRequest(InRace, false);
		return;
	}
#pragma endregion 
	
    TArray<FSoftObjectPath> UIAssetPaths;

    for (const TObjectPtr<USFBuildingDefinition>& Definition : LoadedData->Definitions)
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

    ActiveRaceHandles.Add(InRace, MoveTemp(Handle));
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
		FSFLoadedRaceBuildingData* LoadedData =	LoadedRaceData.Find(InRace);

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

	for (FSFOnRaceBuildingDataReady& Callback : Callbacks)
	{
		Callback.ExecuteIfBound(InRace,	bSuccess);
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

	// 각 건물 Definition의 기본 아이콘과 시너지 뱃지를 함께 캐시에 등록합니다.
	for (const TObjectPtr<USFBuildingDefinition>& Definition
		: LoadedData->Definitions)
	{
		if (!IsValid(Definition))
		{
			CompleteRaceRequest(InRace, false);
			return;
		}

		TArray<FSoftObjectPath> DefinitionUIPaths;
		Definition->GetUIAssetPaths(DefinitionUIPaths);

		for (const FSoftObjectPath& UIPath : DefinitionUIPaths)
		{
			UTexture2D* LoadedTexture =
				Cast<UTexture2D>(UIPath.ResolveObject());

			if (!IsValid(LoadedTexture))
			{
				CompleteRaceRequest(InRace, false);
				return;
			}

			LoadedData->UIAssets.AddUnique(LoadedTexture);
		}
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

	for (FSFOnBuildingAssetsReady& Callback : Callbacks)
	{
		Callback.ExecuteIfBound(InDefinition, InTierIndex, bSuccess);
	}
}
