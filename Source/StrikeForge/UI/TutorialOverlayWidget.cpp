#include "TutorialOverlayWidget.h"

#include "CommonButtonBase.h"
#include "CommonTextBlock.h"
#include "Components/Button.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/Image.h"

void UTutorialOverlayWidget::NativeOnInitialized()
{
	Super::NativeOnInitialized();
	ClickCatcher->OnClicked.AddDynamic(this, &ThisClass::HandleCatcherClicked);
}

void UTutorialOverlayWidget::StartTutorial(UUserWidget* InHost)
{
	HostWidget = InHost;
	if (!HostWidget.IsValid() || Steps.Num() == 0)
	{
		FinishTutorial();
		return;
	}
	EnterStep(0);
}

void UTutorialOverlayWidget::EnterStep(int32 Index)
{
	UnbindTarget();
	CurrentIndex = Index;

	const FTutorialStep& Step = Steps[Index];
	CurrentTarget = HostWidget->GetWidgetFromName(Step.TargetWidgetName);
	ensureMsgf(CurrentTarget.IsValid(), TEXT("Tutorial target '%s' not found"), *Step.TargetWidgetName.ToString());

	MessageText->SetText(Step.Message);

	// Explanation step: catcher covers the whole screen (including the hole)
	// Action step: catcher hidden, only the 4 blockers block input -> hole is clickable
	ClickCatcher->SetVisibility(Step.bRequireTargetClick ? ESlateVisibility::Collapsed : ESlateVisibility::Visible);

	if (Step.bRequireTargetClick)
	{
		if (UCommonButtonBase* CommonButton = Cast<UCommonButtonBase>(CurrentTarget.Get()))
		{
			TargetClickedHandle = CommonButton->OnClicked().AddUObject(this, &ThisClass::HandleTargetClicked);
		}
		else if (UButton* Button = Cast<UButton>(CurrentTarget.Get()))
		{
			Button->OnClicked.AddUniqueDynamic(this, &ThisClass::HandleTargetClicked);
		}
	}

	BP_OnStepEntered(Index);
}

void UTutorialOverlayWidget::AdvanceStep()
{
	if (CurrentIndex + 1 < Steps.Num())
	{
		EnterStep(CurrentIndex + 1);
	}
	else
	{
		FinishTutorial();
	}
}

void UTutorialOverlayWidget::FinishTutorial()
{
	UnbindTarget();
	CurrentIndex = INDEX_NONE;
	OnTutorialFinished.Broadcast(); // save "tutorial done" flag in the listener
	RemoveFromParent();
}

void UTutorialOverlayWidget::HandleCatcherClicked()
{
	AdvanceStep();
}

void UTutorialOverlayWidget::HandleTargetClicked()
{
	AdvanceStep();
}

void UTutorialOverlayWidget::UnbindTarget()
{
	if (UCommonButtonBase* CommonButton = Cast<UCommonButtonBase>(CurrentTarget.Get()))
	{
		CommonButton->OnClicked().Remove(TargetClickedHandle);
	}
	else if (UButton* Button = Cast<UButton>(CurrentTarget.Get()))
	{
		Button->OnClicked.RemoveDynamic(this, &ThisClass::HandleTargetClicked);
	}
	TargetClickedHandle.Reset();
	CurrentTarget.Reset();
}

void UTutorialOverlayWidget::NativeDestruct()
{
	UnbindTarget();
	Super::NativeDestruct();
}

void UTutorialOverlayWidget::SetRect(UWidget* Widget, const FVector2D& Pos, const FVector2D& Size)
{
	if (UCanvasPanelSlot* CanvasSlot = Cast<UCanvasPanelSlot>(Widget->Slot))
	{
		CanvasSlot->SetPosition(Pos);
		CanvasSlot->SetSize(FVector2D(FMath::Max(Size.X, 0.0), FMath::Max(Size.Y, 0.0)));
	}
}

void UTutorialOverlayWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);

	if (CurrentIndex == INDEX_NONE || !CurrentTarget.IsValid())
	{
		return;
	}

	// Recomputed every tick so resolution changes / menu animations are followed
	const FGeometry& TargetGeo = CurrentTarget->GetCachedGeometry();
	if (TargetGeo.GetLocalSize().IsNearlyZero())
	{
		return; // target not laid out yet (first frame after menu opens)
	}

	// Target rect converted into this overlay's local space
	const FVector2D TL = MyGeometry.AbsoluteToLocal(TargetGeo.LocalToAbsolute(FVector2D::ZeroVector));
	const FVector2D BR = MyGeometry.AbsoluteToLocal(TargetGeo.LocalToAbsolute(TargetGeo.GetLocalSize()));

	const FMargin& Pad = Steps[CurrentIndex].HolePadding;
	const FVector2D HoleMin(TL.X - Pad.Left, TL.Y - Pad.Top);
	const FVector2D HoleMax(BR.X + Pad.Right, BR.Y + Pad.Bottom);
	const FVector2D HoleSize = HoleMax - HoleMin;
	const FVector2D Screen = MyGeometry.GetLocalSize();

	SetRect(BlockerTop, FVector2D(0.0, 0.0), FVector2D(Screen.X, HoleMin.Y));
	SetRect(BlockerBottom, FVector2D(0.0, HoleMax.Y), FVector2D(Screen.X, Screen.Y - HoleMax.Y));
	SetRect(BlockerLeft, FVector2D(0.0, HoleMin.Y), FVector2D(HoleMin.X, HoleSize.Y));
	SetRect(BlockerRight, FVector2D(HoleMax.X, HoleMin.Y), FVector2D(Screen.X - HoleMax.X, HoleSize.Y));
	SetRect(HighlightFrame, HoleMin, HoleSize);

	// Message box: below the hole, flip above if it would go off screen
	if (UCanvasPanelSlot* MsgSlot = Cast<UCanvasPanelSlot>(MessageBox->Slot))
	{
		constexpr double Gap = 16.0;
		const FVector2D MsgSize = MessageBox->GetDesiredSize();

		double Y = HoleMax.Y + Gap;
		if (Y + MsgSize.Y > Screen.Y)
		{
			Y = HoleMin.Y - Gap - MsgSize.Y;
		}
		const double X = FMath::Clamp(HoleMin.X + HoleSize.X * 0.5 - MsgSize.X * 0.5,
			Gap, FMath::Max(Gap, Screen.X - MsgSize.X - Gap));
		MsgSlot->SetPosition(FVector2D(X, Y));
	}
}