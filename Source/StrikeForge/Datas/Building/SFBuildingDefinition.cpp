#include "SFBuildingDefinition.h"

const FPrimaryAssetType
USFBuildingDefinition::BuildingAssetType(
    TEXT("Building"));

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