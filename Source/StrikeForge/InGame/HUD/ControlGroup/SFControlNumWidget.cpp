#include "SFControlNumWidget.h"

#include "Animation/WidgetAnimation.h"
#include "Components/Button.h"
#include "Components/Image.h"
#include "Components/TextBlock.h"

void USFControlNumWidget::NativeConstruct()
{
    Super::NativeConstruct();

    if (IsValid(SlotButton))
    {
        SlotButton->OnClicked.AddUniqueDynamic(
            this,
            &USFControlNumWidget::HandleSlotButtonClicked
        );
    }

    if (IsValid(ControlNum_SlideIn))
    {
        FWidgetAnimationDynamicEvent FinishedEvent;

        FinishedEvent.BindDynamic(
            this,
            &USFControlNumWidget::HandleControlNumSlideAnimationFinished
        );

        BindToAnimationFinished(
            ControlNum_SlideIn,
            FinishedEvent
        );
    }

    ClearControlGroupData();
}

void USFControlNumWidget::NativeDestruct()
{
    if (IsValid(SlotButton))
    {
        SlotButton->OnClicked.RemoveDynamic(
            this,
            &USFControlNumWidget::HandleSlotButtonClicked
        );
    }

    Super::NativeDestruct();
}

void USFControlNumWidget::InitializeControlNum(
    int32 InGroupIndex
)
{
    GroupIndex = InGroupIndex;

    if (IsValid(ControlNumberText))
    {
        // 내부 인덱스는 0~5, 화면에는 1~6
        ControlNumberText->SetText(
            FText::AsNumber(GroupIndex + 1)
        );
    }
}

void USFControlNumWidget::SetControlGroupData(
    UTexture2D* InRepresentativeImage,
    int32 InActorCount
)
{
    UE_LOG(
        LogTemp,
        Warning,
        TEXT(
            "[SFControlNumWidget] SetData called | "
            "Index: %d | Count: %d | Animation: %s"
        ),
        GroupIndex,
        InActorCount,
        *GetNameSafe(ControlNum_SlideIn)
    );

    if (InActorCount <= 0)
    {
        ClearControlGroupData();
        return;
    }

    const bool bWasAlreadyAssigned =
        bHasControlGroupData;

    bHasControlGroupData = true;

    if (IsValid(RepresentativeImage))
    {
        RepresentativeImage->SetBrushFromTexture(
            InRepresentativeImage,
            false
        );
    }

    if (IsValid(UnitCountText))
    {
        UnitCountText->SetText(
            FText::AsNumber(InActorCount)
        );
    }

    SetVisibility(
        ESlateVisibility::Visible
    );

    // 처음 지정된 슬롯일 때만 애니메이션 재생
    // Shift로 기존 부대에 추가할 때는 다시 재생하지 않음
    if (!bWasAlreadyAssigned &&
        IsValid(ControlNum_SlideIn))
    {
        PlayAnimation(
            ControlNum_SlideIn,
            0.0f,
            1,
            EUMGSequencePlayMode::Forward,
            1.0f
        );
    }
}

void USFControlNumWidget::ClearControlGroupData()
{
    bHasControlGroupData = false;

    if (IsValid(ControlNum_SlideIn))
    {
        StopAnimation(ControlNum_SlideIn);
    }

    if (IsValid(RepresentativeImage))
    {
        RepresentativeImage->SetBrushFromTexture(
            nullptr,
            false
        );
    }

    if (IsValid(UnitCountText))
    {
        UnitCountText->SetText(
            FText::GetEmpty()
        );
    }

    SetVisibility(
        ESlateVisibility::Collapsed
    );
}

void USFControlNumWidget::HandleSlotButtonClicked()
{
    if (!bHasControlGroupData ||
        GroupIndex == INDEX_NONE)
    {
        return;
    }

    OnControlNumClicked.Broadcast(GroupIndex);
}

void USFControlNumWidget::PlaySlideOutAndHide()
{
    if (!IsValid(ControlNum_SlideIn))
    {
        ClearControlGroupData();

        SetVisibility(
            ESlateVisibility::Collapsed
        );

        return;
    }

    bIsPlayingSlideOut = true;

    SetVisibility(
        ESlateVisibility::Visible
    );

    PlayAnimationReverse(
        ControlNum_SlideIn,
        1.0f
    );
}

void USFControlNumWidget::HandleControlNumSlideAnimationFinished()
{
    if (!bIsPlayingSlideOut)
    {
        return;
    }

    bIsPlayingSlideOut = false;

    ClearControlGroupData();

    SetVisibility(
        ESlateVisibility::Collapsed
    );
}

void USFControlNumWidget::SetActorCount(int32 NewCount)
{
    if (!IsValid(UnitCountText))
    {
        return;
    }

    UnitCountText->SetText(
        FText::AsNumber(
            FMath::Max(0, NewCount)
        )
    );
}