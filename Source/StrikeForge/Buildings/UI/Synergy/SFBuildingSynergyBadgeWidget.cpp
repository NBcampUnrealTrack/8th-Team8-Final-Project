#include "SFBuildingSynergyBadgeWidget.h"

#include "Components/Image.h"
#include "Components/TextBlock.h"
#include "Datas/Building/Synergy/SFBuildingSynergyDefinition.h"
#include "Game/Subsystem/SFBuildingSynergySubsystem.h"

#define LOCTEXT_NAMESPACE "SFBuildingSynergyBadgeWidget"

void USFBuildingSynergyBadgeWidget::SetProgressInfo(
	const FSFBuildingSynergyClusterProgressInfo& ProgressInfo)
{
	const USFBuildingSynergyDefinition* Definition = ProgressInfo.Definition;
	if (!IsValid(Definition))
	{
		BadgeWorldLocation = FVector::ZeroVector;
		SetVisibility(ESlateVisibility::Collapsed);
		return;
	}

	const int32 MaxRequiredBuildingCount =
		Definition->GetMaxRequiredBuildingCount();
	if (MaxRequiredBuildingCount <= 0)
	{
		BadgeWorldLocation = FVector::ZeroVector;
		SetVisibility(ESlateVisibility::Collapsed);
		return;
	}

	BadgeWorldLocation = ProgressInfo.BadgeWorldLocation;

	// Definition이 가진 시너지 표시 이름을 배지 제목에 그대로 반영합니다.
	if (IsValid(SynergyDisplayText))
	{
		SynergyDisplayText->SetText(Definition->DisplayName);
	}

	// Definition의 Soft Texture를 그대로 사용해 종족별 배지 아이콘을 표시합니다.
	if (IsValid(BadgeIconImage))
	{
		const bool bHasBadgeIcon = !Definition->BadgeIcon.IsNull();
		BadgeIconImage->SetVisibility(
			bHasBadgeIcon
				? ESlateVisibility::SelfHitTestInvisible
				: ESlateVisibility::Collapsed);
		if (bHasBadgeIcon)
		{
			BadgeIconImage->SetBrushFromSoftTexture(
				Definition->BadgeIcon,
				false);
		}
		else
		{
			BadgeIconImage->SetBrushFromTexture(nullptr, false);
		}
	}

	const int32 SourceBuildingCount =
		FMath::Max(0, ProgressInfo.SourceBuildingCount);
	const FSFBuildingSynergyLevel* ActiveLevel =
		Definition->FindLevel(SourceBuildingCount);
	const FSFBuildingSynergyLevel* NextLevel =
		Definition->FindNextLevel(SourceBuildingCount);
	const int32 TargetBuildingCount =
		NextLevel
			? NextLevel->MinSourceBuildingCount
			: MaxRequiredBuildingCount;

	// 최대 단계에서는 실제 건물 수가 요구 수를 넘어도 '최대 / 최대'로 고정해 표시합니다.
	if (IsValid(ProgressText))
	{
		const int32 DisplayBuildingCount =
			FMath::Min(SourceBuildingCount, TargetBuildingCount);
		ProgressText->SetText(FText::Format(
			LOCTEXT("ProgressFormat", "{0} / {1}"),
			FText::AsNumber(DisplayBuildingCount),
			FText::AsNumber(TargetBuildingCount)));
	}

	// 활성 단계가 있을 때만 현재 생산 보너스를 표시하고, 1개짜리 대기 클러스터에서는 숨깁니다.
	if (IsValid(BonusText))
	{
		const bool bHasProductionBonus =
			ActiveLevel && ActiveLevel->ProductionBonusPercent > 0.f;
		BonusText->SetVisibility(
			bHasProductionBonus
				? ESlateVisibility::SelfHitTestInvisible
				: ESlateVisibility::Collapsed);

		if (bHasProductionBonus)
		{
			FNumberFormattingOptions NumberFormat;
			NumberFormat.MinimumFractionalDigits = 0;
			NumberFormat.MaximumFractionalDigits = 1;
			BonusText->SetText(FText::Format(
				LOCTEXT("BonusFormat", "+{0}%"),
				FText::AsNumber(
					ActiveLevel->ProductionBonusPercent,
					&NumberFormat)));
		}
		else
		{
			BonusText->SetText(FText::GetEmpty());
		}
	}

	SetVisibility(ESlateVisibility::SelfHitTestInvisible);
}

#undef LOCTEXT_NAMESPACE
