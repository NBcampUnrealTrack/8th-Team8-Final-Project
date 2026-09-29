#include "SFBuildingDefinition.h"

#include "BuildingAppearance.h"
#include "BuildingTierData/SFBuildingTierData.h"
#include "Synergy/SFBuildingSynergyDefinition.h"

// == AssetManager의 Primary Asset Type FName
const FPrimaryAssetType
USFBuildingDefinition::BuildingAssetType(
    TEXT("Building"));

namespace
{
    /** 유효한 Soft 경로만 중복 없이 추가한다.
     * Definition의 경로 수집 함수에서 호출되며 RequestAsyncLoad 입력 배열을 만든다. */
    void AddSoftPath(
        const FSoftObjectPath& InPath,
        TArray<FSoftObjectPath>& OutPaths)
    {
        if (!InPath.IsNull())
        {
            OutPaths.AddUnique(InPath);
        }
    }
}

FPrimaryAssetId
USFBuildingDefinition::GetPrimaryAssetId() const
{
    /**
     * BuildingId 입력 전에는 에셋 이름을 사용한다.
     *
     * BuildingId를 입력하면:
     * Building:HumanHouse
     *
     * 입력하지 않으면:
     * Building:DA_HumanHouse
     */
    const FName PrimaryAssetName =
        BuildingId.IsNone()
            ? GetFName()
            : BuildingId;

    return FPrimaryAssetId(
        BuildingAssetType,
        PrimaryAssetName);
}

int32
USFBuildingDefinition::GetTierCount() const
{
    /**
     * 부모 Definition은 Tier 배열을 소유하지 않는다.
     *
     * 자식이 override하지 않았다면
     * 유효한 Tier가 없는 것으로 처리한다.
     */
    return 0;
}

const FSFBuildingTierData*
USFBuildingDefinition::FindTier(
    int32 TierIndex) const
{
    /**
     * UObject 부모에서 순수 가상 함수로 만들지 않고
     * 안전한 기본값을 반환한다.
     *
     * 자식 Definition이 반드시 override해야 한다.
     */
    return nullptr;
}

void USFBuildingDefinition::GetUIAssetPaths(TArray<FSoftObjectPath>& OutPaths) const
{
    AddSoftPath(
    Icon.ToSoftObjectPath(),
    OutPaths);

	// 종족별 시너지 뱃지도 다른 건물 UI 텍스처와 함께 미리 로드합니다.
	for (const FSFGrantedBuildingSynergy& GrantedSynergy : GrantedSynergies)
	{
		if (IsValid(GrantedSynergy.Definition))
		{
			AddSoftPath(
				GrantedSynergy.Definition->BadgeIcon.ToSoftObjectPath(),
				OutPaths);
		}
	}
}

bool USFBuildingDefinition::GetRuntimeAssetPaths(
    int32 InTierIndex,
    TArray<FSoftObjectPath>& OutPaths) const
{
    TArray<FSoftObjectPath> RequiredPaths;
    TArray<FSoftObjectPath> OptionalPaths;

    if (!CollectRuntimeAssetPaths(
        InTierIndex, RequiredPaths, OptionalPaths, true))
    {
        return false;
    }

    // 요청할 때는 지정된 필수·선택 에셋을 모두 포함한다.
    for (const FSoftObjectPath& Path : RequiredPaths)
    {
        OutPaths.AddUnique(Path);
    }

    for (const FSoftObjectPath& Path : OptionalPaths)
    {
        OutPaths.AddUnique(Path);
    }

    return true;
}

bool USFBuildingDefinition::AreRuntimeAssetsLoaded(int32 InTierIndex, bool bLogFailures) const
{
    // TArray<FSoftObjectPath> RuntimeAssetPaths;
    //
    // if (!GetRuntimeAssetPaths(InTierIndex, RuntimeAssetPaths))
    // {
    //     return false;
    // }
    //
    // // 전부 로딩을 마쳤는지 + 에셋이 존재하는지 검사
    // for (const FSoftObjectPath& AssetPath : RuntimeAssetPaths)
    // {   // soft에서 로딩을 마치고 Object로 저장소에 등록이 되어있다면 주소를 반환, 없다면 Null
    //     // 쉽게말해 로딩됬으면 주소반환, 안됐으면 Null
    //     // 경로에 대해 전체 로딩이 성공해야함 true를 반환하기때문에
    //     // 하나라도 에셋지정을 안한다면 콜백함수가 불리지않는다.
    //     if (!IsValid(AssetPath.ResolveObject()))
    //     {
    //         return false;
    //     }
    // }
    //
    // return true;
    TArray<FSoftObjectPath> RequiredPaths;
    TArray<FSoftObjectPath> OptionalPaths;

    // 경로 수집 시 출력한 미지정 경고를 반복하지 않는다.
    if (!CollectRuntimeAssetPaths(
        InTierIndex, RequiredPaths, OptionalPaths, false))
    {
        if (bLogFailures)
        {
            UE_LOG(LogTemp, Error,
                TEXT("[%s] Tier %d 필수 에셋 설정이 유효하지 않습니다."),
                *GetPathName(), InTierIndex);
        }
        return false;
    }

    bool bRequiredAssetsLoaded = true;

    for (const FSoftObjectPath& Path : RequiredPaths)
    {
        if (!IsValid(Path.ResolveObject()))
        {
            bRequiredAssetsLoaded = false;

            if (bLogFailures)
            {
                UE_LOG(LogTemp, Error,
                    TEXT("[%s] Tier %d 필수 에셋 로드 실패: %s"),
                    *GetPathName(), InTierIndex, *Path.ToString());
            }
        }
    }

    if (bLogFailures)
    {
        for (const FSoftObjectPath& Path : OptionalPaths)
        {
            if (!IsValid(Path.ResolveObject()))
            {
                UE_LOG(LogTemp, Warning,
                    TEXT("[%s] Tier %d 선택 에셋 로드 실패: %s. 생략합니다."),
                    *GetPathName(), InTierIndex, *Path.ToString());
            }
        }
    }

    // 선택 에셋 실패는 전체 성공 여부에 영향을 주지 않는다.
    return bRequiredAssetsLoaded;
    
}

bool USFBuildingDefinition::ValidateAllTierAssetSettings() const
{
    const int32 TierCount = GetTierCount();

    if (TierCount <= 0)
    {
        UE_LOG(LogTemp, Error,
            TEXT("[%s] 건물 Tier 데이터가 비어 있습니다."),
            *GetPathName());

        return false;
    }

    bool bAllTiersValid = true;

    TArray<FSoftObjectPath> RequiredPaths;
    TArray<FSoftObjectPath> OptionalPaths;

    for (int32 TierIndex = 0; TierIndex < TierCount; ++TierIndex)
    {
        // 기존 필수·선택 분류와 검증 규칙을 그대로 사용한다.
        // 이 함수는 경로만 수집하며 에셋을 로딩하지 않는다.
        const bool bTierValid = CollectRuntimeAssetPaths(
            TierIndex,
            RequiredPaths,
            OptionalPaths,
            true);

        if (!bTierValid)
        {
            bAllTiersValid = false;
        }
    }

    return bAllTiersValid;
}

bool USFBuildingDefinition::CollectRuntimeAssetPaths(int32 InTierIndex, TArray<FSoftObjectPath>& OutRequiredPaths,
                                                     TArray<FSoftObjectPath>& OutOptionalPaths, bool bLogIssues) const
{
    OutRequiredPaths.Reset();
    OutOptionalPaths.Reset();

    const FSFBuildingTierData* Tier = FindTier(InTierIndex);
    if (!Tier)
    {
        if (bLogIssues)
        {
            UE_LOG(LogTemp, Error,
                TEXT("[%s] Tier %d 데이터가 없습니다."),
                *GetPathName(), InTierIndex);
        }
        return false;
    }

    bool bValid = true;

    auto AddRequired = [&](
        const FSoftObjectPath& Path,
        const TCHAR* FieldName)
    {
        if (Path.IsNull())
        {
            bValid = false;

            if (bLogIssues)
            {
                UE_LOG(LogTemp, Error,
                    TEXT("[%s] Tier %d 필수 에셋 미지정: %s"),
                    *GetPathName(), InTierIndex, FieldName);
            }
            return;
        }

        OutRequiredPaths.AddUnique(Path);
    };

    auto AddOptional = [&](
        const FSoftObjectPath& Path,
        const TCHAR* FieldName)
    {
        if (Path.IsNull())
        {
            if (bLogIssues)
            {
                UE_LOG(LogTemp, Warning,
                    TEXT("[%s] Tier %d 선택 에셋 미지정: %s. 생략합니다."),
                    *GetPathName(), InTierIndex, FieldName);
            }
            return;
        }

        OutOptionalPaths.AddUnique(Path);
    };

    // 필수 에셋
    AddRequired(
        BuildingClass.ToSoftObjectPath(),
        TEXT("BuildingClass"));

    AddRequired(
        Tier->Visual.CompletedMesh.ToSoftObjectPath(),
        TEXT("CompletedMesh"));

    AddRequired(
        Tier->Visual.DestructionCollection.ToSoftObjectPath(),
        TEXT("DestructionCollection"));

    if (Tier->Visual.ConstructionStages.IsEmpty())
    {
        bValid = false;

        if (bLogIssues)
        {
            UE_LOG(LogTemp, Error,
                TEXT("[%s] Tier %d ConstructionStages가 비어 있습니다."),
                *GetPathName(), InTierIndex);
        }
    }

    for (int32 Index = 0;
         Index < Tier->Visual.ConstructionStages.Num();
         ++Index)
    {
        const FString FieldName = FString::Printf(
            TEXT("ConstructionStages[%d].Mesh"), Index);

        AddRequired(
            Tier->Visual.ConstructionStages[Index].Mesh.ToSoftObjectPath(),
            *FieldName);
    }

    // 선택 프리뷰 머티리얼
    AddOptional(
        PreviewMaterial.ToSoftObjectPath(),
        TEXT("PreviewMaterial"));

    // 선택 사운드
    AddOptional(
        UnderConstructionSelectionSound.ToSoftObjectPath(),
        TEXT("UnderConstructionSelectionSound"));

    AddOptional(
        CompletedSelectionSound.ToSoftObjectPath(),
        TEXT("CompletedSelectionSound"));

    AddOptional(
        DestroyedSound.ToSoftObjectPath(),
        TEXT("DestroyedSound"));

    AddOptional(
        Tier->Audio.ConstructionStart.ToSoftObjectPath(),
        TEXT("Audio.ConstructionStart"));

    AddOptional(
        Tier->Audio.ConstructionComplete.ToSoftObjectPath(),
        TEXT("Audio.ConstructionComplete"));

    // 선택 이펙트
    AddOptional(
        ConstructEffect.ToSoftObjectPath(),
        TEXT("ConstructEffect"));

    AddOptional(
        DestroyEffect.ToSoftObjectPath(),
        TEXT("DestroyEffect"));

    AddOptional(
        DamagedBurningEffect.ToSoftObjectPath(),
        TEXT("DamagedBurningEffect"));

	// 시너지 경계는 표현 전용 선택 에셋입니다. 미지정 상태도 정상입니다.
	for (const FSFGrantedBuildingSynergy& GrantedSynergy : GrantedSynergies)
	{
		if (!IsValid(GrantedSynergy.Definition))
		{
			continue;
		}

		const FSoftObjectPath BoundaryPath =
			GrantedSynergy.Definition->BoundaryEffect.ToSoftObjectPath();

		if (!BoundaryPath.IsNull())
		{
			OutOptionalPaths.AddUnique(BoundaryPath);
		}
	}

    return bValid;
}
