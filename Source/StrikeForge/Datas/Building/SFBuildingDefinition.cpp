#include "SFBuildingDefinition.h"

#include "BuildingAppearance.h"
#include "BuildingTierData/SFBuildingTierData.h"

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
}

bool USFBuildingDefinition::GetRuntimeAssetPaths(int32 InTierIndex, TArray<FSoftObjectPath>& OutPaths) const
{
    const FSFBuildingTierData* Tier = FindTier(InTierIndex);

    if (!Tier ||
        BuildingClass.IsNull() ||
        Tier->Visual.CompletedMesh.IsNull())// ||
        //Tier->Visual.DestroyedMesh.IsNull())
    {
        return false;
    }

    // 건물 클래스와 공통 리소스를 OutPath에 몰아넣기
    AddSoftPath(BuildingClass.ToSoftObjectPath(), OutPaths);
    AddSoftPath(PreviewMaterial.ToSoftObjectPath(), OutPaths);
    AddSoftPath(UnderConstructionSelectionSound.ToSoftObjectPath(),OutPaths);
    AddSoftPath(CompletedSelectionSound.ToSoftObjectPath(),OutPaths);
    AddSoftPath(DestroyedSound.ToSoftObjectPath(),OutPaths);
    AddSoftPath(ConstructEffect.ToSoftObjectPath(),OutPaths);
    AddSoftPath(DestroyEffect.ToSoftObjectPath(),OutPaths);
    AddSoftPath(DamagedBurningEffect.ToSoftObjectPath(),OutPaths);

    // 티어별 메시
    for (const FBuildingStage& Stage : Tier->Visual.ConstructionStages)
    {
        if (Stage.Mesh.IsNull())
        {
            return false;
        }

        AddSoftPath(Stage.Mesh.ToSoftObjectPath(),OutPaths);
    }

    // for (const FBuildingStage& Stage : Tier->Visual.DamageStages)
    // {
    //     if (Stage.Mesh.IsNull())
    //     {
    //         return false;
    //     }
    //
    //     AddSoftPath(Stage.Mesh.ToSoftObjectPath(),OutPaths);
    // }

    AddSoftPath(Tier->Visual.CompletedMesh.ToSoftObjectPath(),OutPaths);
    //AddSoftPath(Tier->Visual.DestroyedMesh.ToSoftObjectPath(),OutPaths);

    // 선택 사항
    AddSoftPath(Tier->Visual.DestructionCollection.ToSoftObjectPath(),OutPaths);
    AddSoftPath(Tier->Audio.ConstructionStart.ToSoftObjectPath(),OutPaths);
    AddSoftPath(Tier->Audio.ConstructionComplete.ToSoftObjectPath(),OutPaths);

    return true;
}

bool USFBuildingDefinition::AreRuntimeAssetsLoaded(int32 InTierIndex) const
{
    TArray<FSoftObjectPath> RuntimeAssetPaths;

    if (!GetRuntimeAssetPaths(InTierIndex, RuntimeAssetPaths))
    {
        return false;
    }

    // 전부 로딩을 마쳤는지 + 에셋이 존재하는지 검사
    for (const FSoftObjectPath& AssetPath : RuntimeAssetPaths)
    {   // soft에서 로딩을 마치고 Object로 저장소에 등록이 되어있다면 주소를 반환, 없다면 Null
        // 쉽게말해 로딩됬으면 주소반환, 안됐으면 Null
        // 경로에 대해 전체 로딩이 성공해야함 true를 반환하기때문에
        // 하나라도 에셋지정을 안한다면 콜백함수가 불리지않는다.
        if (!IsValid(AssetPath.ResolveObject()))
        {
            return false;
        }
    }

    return true;
}
