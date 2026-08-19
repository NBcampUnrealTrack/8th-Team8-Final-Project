#include "SFBuildingDefinition.h"
#include "Misc/DataValidation.h"

const FPrimaryAssetType
USFBuildingDefinition::BuildingAssetType(
    TEXT("Building"));

FPrimaryAssetId
USFBuildingDefinition::GetPrimaryAssetId() const
{
    // BuildingId 입력 전에도 에셋이 유효한 ID를 가질 수 있게 한다.
    const FName PrimaryAssetName =
        BuildingId.IsNone()
            ? GetFName()
            : BuildingId;

    return FPrimaryAssetId(
        BuildingAssetType,
        PrimaryAssetName);
}

const FBuildingTier*
USFBuildingDefinition::FindTier(
    int32 TierIndex) const
{
    return Tiers.IsValidIndex(TierIndex)
        ? &Tiers[TierIndex]
        : nullptr;
}

#if WITH_EDITOR

EDataValidationResult
USFBuildingDefinition::IsDataValid(
    FDataValidationContext& Context) const
{
    const EDataValidationResult SuperResult =
        Super::IsDataValid(Context);

    bool bIsValid =
        SuperResult != EDataValidationResult::Invalid;

    if (BuildingId.IsNone())
    {
        Context.AddError(
            FText::FromString(
                TEXT("BuildingId가 비어 있습니다.")));

        bIsValid = false;
    }

    if (DisplayName.IsEmpty())
    {
        Context.AddError(
            FText::FromString(
                TEXT("DisplayName이 비어 있습니다.")));

        bIsValid = false;
    }

    if (SelectionUIRowName.IsNone())
    {
        Context.AddError(
            FText::FromString(
                TEXT("현재 선택 UI 호환을 위해 SelectionUIRowName이 필요합니다.")));

        bIsValid = false;
    }

    if (BuildingClass.IsNull())
    {
        Context.AddError(
            FText::FromString(
                TEXT("BuildingClass가 설정되지 않았습니다.")));

        bIsValid = false;
    }

    if (GridSize.X < 1 || GridSize.Y < 1)
    {
        Context.AddError(
            FText::FromString(
                TEXT("GridSize는 X와 Y 모두 1 이상이어야 합니다.")));

        bIsValid = false;
    }

    if (Tiers.IsEmpty())
    {
        Context.AddError(
            FText::FromString(
                TEXT("최소 하나의 Tier가 필요합니다.")));

        bIsValid = false;
    }

    for (int32 TierIndex = 0;
         TierIndex < Tiers.Num();
         ++TierIndex)
    {
        
        const FBuildingTier& Tier =
            Tiers[TierIndex];

        auto ValidateStages =
    [&Context, &bIsValid, TierIndex](
        const TArray<FBuildingStage>& Stages,
        const TCHAR* StageName,
        bool bAscending)
    {
        for (int32 Index = 0; Index < Stages.Num(); ++Index)
        {
            const FBuildingStage& Stage = Stages[Index];

            if (Stage.Threshold < 0.f || Stage.Threshold > 1.f)
            {
                Context.AddError(
                    FText::FromString(
                        FString::Printf(
                            TEXT("Tier %d %s[%d] Threshold는 0~1 범위여야 합니다."),
                            TierIndex + 1,
                            StageName,
                            Index)));

                bIsValid = false;
            }

            if (Stage.Mesh.IsNull())
            {
                Context.AddError(
                    FText::FromString(
                        FString::Printf(
                            TEXT("Tier %d %s[%d] Mesh가 비어 있습니다."),
                            TierIndex + 1,
                            StageName,
                            Index)));

                bIsValid = false;
            }

            if (Index == 0)
            {
                continue;
            }

            const float PreviousThreshold =
                Stages[Index - 1].Threshold;

            const bool bWrongOrder =
                bAscending
                    ? PreviousThreshold >= Stage.Threshold
                    : PreviousThreshold <= Stage.Threshold;

            if (bWrongOrder)
            {
                Context.AddError(
                    FText::FromString(
                        FString::Printf(
                            TEXT("Tier %d %s의 Threshold 정렬 순서가 잘못되었습니다."),
                            TierIndex + 1,
                            StageName)));

                bIsValid = false;
            }
        }
    };

        ValidateStages(
            Tier.Visual.ConstructionStages,
            TEXT("ConstructionStages"),
            true);

        ValidateStages(
            Tier.Visual.DamageStages,
            TEXT("DamageStages"),
            false);
        
        if (Tier.Construction.RequiredWork <= 0.f)
        {
            Context.AddError(
                FText::FromString(
                    FString::Printf(
                        TEXT(
                            "Tier %d RequiredWork가 "
                            "0 이하입니다."),
                        TierIndex + 1)));

            bIsValid = false;
        }

        if (Tier.Stats.MaxHealth < 1.f)
        {
            Context.AddError(
                FText::FromString(
                    FString::Printf(
                        TEXT(
                            "Tier %d MaxHealth가 "
                            "1 미만입니다."),
                        TierIndex + 1)));

            bIsValid = false;
        }

        if (Tier.Visual.CompletedMesh.IsNull())
        {
            Context.AddError(
                FText::FromString(
                    FString::Printf(
                        TEXT(
                            "Tier %d CompletedMesh가 "
                            "설정되지 않았습니다."),
                        TierIndex + 1)));

            bIsValid = false;
        }

        if (Tier.Visual.DestroyedMesh.IsNull())
        {
            Context.AddError(
                FText::FromString(
                    FString::Printf(
                        TEXT(
                            "Tier %d DestroyedMesh가 "
                            "설정되지 않았습니다."),
                        TierIndex + 1)));

            bIsValid = false;
        }
    }

    return bIsValid
        ? EDataValidationResult::Valid
        : EDataValidationResult::Invalid;
}

#endif
