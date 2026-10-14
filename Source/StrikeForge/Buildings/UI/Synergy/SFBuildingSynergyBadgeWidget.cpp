#include "SFBuildingSynergyBadgeWidget.h"

#include "Animation/WidgetAnimation.h"
#include "Components/Image.h"
#include "Components/SizeBox.h"
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
	const int32 ActiveLevelIndex =
		Definition->FindLevelIndex(SourceBuildingCount);
	const FSFBuildingSynergyLevel* ActiveLevel =
		Definition->Levels.IsValidIndex(ActiveLevelIndex)
			? &Definition->Levels[ActiveLevelIndex]
			: nullptr;
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

	// 현재 적용 중인 단계 번호(Levels 인덱스 + 1)를 표시하고, 활성 단계가 없는 클러스터는 0으로 표시합니다.
	if (IsValid(SynergyLevelText))
	{
		const int32 DisplayLevel =
			ActiveLevelIndex != INDEX_NONE ? ActiveLevelIndex + 1 : 0;
		SynergyLevelText->SetText(FText::AsNumber(DisplayLevel));
	}

	SetVisibility(ESlateVisibility::SelfHitTestInvisible);
}

// OverlapIconSizebox의 Slate 위젯에 마우스 진입·이탈 핸들러를 연결합니다.
// SizeBox는 기본이 SelfHitTestInvisible이라 마우스를 감지하도록 Visible로 바꿉니다(클릭은 처리하지 않아 게임으로 전달됩니다).
void USFBuildingSynergyBadgeWidget::NativeConstruct()
{
	Super::NativeConstruct();

	if (!IsValid(OverlapIconSizebox))
	{
		return;
	}

	OverlapIconSizebox->SetVisibility(ESlateVisibility::Visible);

	if (const TSharedPtr<SWidget> SizeBoxWidget = OverlapIconSizebox->GetCachedWidget())
	{
		SizeBoxWidget->SetOnMouseEnter(FNoReplyPointerEventHandler::CreateUObject(
			this, &ThisClass::HandleOverlapIconMouseEnter));
		SizeBoxWidget->SetOnMouseLeave(FSimpleNoReplyPointerEventHandler::CreateUObject(
			this, &ThisClass::HandleOverlapIconMouseLeave));
	}
}

// 아이콘에 마우스가 올라왔음을 기록하고, 멈춰 있으면 정방향 재생을 시작합니다.
// NativeConstruct에서 OverlapIconSizebox의 Slate 마우스 진입 핸들러로 연결됩니다.
void USFBuildingSynergyBadgeWidget::HandleOverlapIconMouseEnter(
	const FGeometry& Geometry,
	const FPointerEvent& MouseEvent)
{
	bOverlapIconHovered = true;
	UpdateOverlapAnimation();
}

// 아이콘에서 마우스가 나갔음을 기록하고, 끝에 멈춰 있으면 역방향 재생을 시작합니다.
// NativeConstruct에서 OverlapIconSizebox의 Slate 마우스 이탈 핸들러로 연결됩니다.
void USFBuildingSynergyBadgeWidget::HandleOverlapIconMouseLeave(const FPointerEvent& MouseEvent)
{
	bOverlapIconHovered = false;
	UpdateOverlapAnimation();
}

// 재생 중에는 끝까지 두고, 멈춰 있을 때만 호버 상태에 맞춰 정방향(0→끝) 또는 역방향(끝→0)을 시작합니다.
// 마우스 진입·이탈 핸들러와 OnAnimationFinished_Implementation이 호출합니다.
void USFBuildingSynergyBadgeWidget::UpdateOverlapAnimation()
{
	if (!IsValid(OverlapAnimation) || IsAnimationPlaying(OverlapAnimation))
	{
		return;
	}

	if (bOverlapIconHovered && !bOverlapAnimationAtEnd)
	{
		bOverlapAnimationPlayingForward = true;
		PlayAnimation(OverlapAnimation, 0.f, 1, EUMGSequencePlayMode::Forward);
	}
	else if (!bOverlapIconHovered && bOverlapAnimationAtEnd)
	{
		bOverlapAnimationPlayingForward = false;
		PlayAnimation(OverlapAnimation, 0.f, 1, EUMGSequencePlayMode::Reverse);
	}
}

// 한 방향을 끝까지 재생하면 도착한 끝을 기록하고, 재생 중 바뀐 호버 상태를 반영해 다음 재생을 정합니다.
// 엔진은 재생 상태를 Stopped로 바꾼 뒤 이 함수를 호출하므로 여기서 바로 반대 방향을 재생할 수 있습니다.
void USFBuildingSynergyBadgeWidget::OnAnimationFinished_Implementation(const UWidgetAnimation* Animation)
{
	Super::OnAnimationFinished_Implementation(Animation);

	if (Animation != OverlapAnimation)
	{
		return;
	}

	bOverlapAnimationAtEnd = bOverlapAnimationPlayingForward;
	UpdateOverlapAnimation();
}

#undef LOCTEXT_NAMESPACE
