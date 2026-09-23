#include "ResultWidget.h"
#include "Components/Button.h"
#include "Components/Image.h"
#include "Components/Widget.h"
#include "Animation/WidgetAnimation.h"
#include "StrikeForge/Game/GameInstance/SFGameInstance.h"

void UResultWidget::NativeConstruct()
{
	Super::NativeConstruct();

	if (IsValid(Exit_Btn_W))
	{
		Exit_Btn_W->OnClicked.AddDynamic(this, &UResultWidget::OnExitButtonClicked);
	}
	if (IsValid(Exit_Btn_F))
	{
		Exit_Btn_F->OnClicked.AddDynamic(this, &UResultWidget::OnExitButtonClicked);
	}

	if (IsValid(Win_Image)) { Win_Image->SetVisibility(ESlateVisibility::Collapsed); }
	if (IsValid(Loose_Image)) { Loose_Image->SetVisibility(ESlateVisibility::Collapsed); }
	if (IsValid(Exit_Btn_W)) { Exit_Btn_W->SetVisibility(ESlateVisibility::Collapsed); }
	if (IsValid(Exit_Btn_F)) { Exit_Btn_F->SetVisibility(ESlateVisibility::Collapsed); }
	if (IsValid(Win_Glow)) { Win_Glow->SetVisibility(ESlateVisibility::Collapsed); }

	// Flash must never block clicks and starts invisible
	if (IsValid(Flash))
	{
		Flash->SetVisibility(ESlateVisibility::HitTestInvisible);
		Flash->SetRenderOpacity(0.f);
	}

	// Chain the looping glow after the win intro (bind once)
	if (Anim_Win)
	{
		WinAnimFinishedEvent.BindDynamic(this, &UResultWidget::OnWinAnimFinished);
		BindToAnimationFinished(Anim_Win, WinAnimFinishedEvent);
	}
}

void UResultWidget::SetResultState(bool bIsWin)
{
	const ESlateVisibility WinVis = bIsWin ? ESlateVisibility::Visible : ESlateVisibility::Collapsed;
	const ESlateVisibility LoseVis = bIsWin ? ESlateVisibility::Collapsed : ESlateVisibility::Visible;

	if (IsValid(Win_Image)) { Win_Image->SetVisibility(WinVis); }
	if (IsValid(Exit_Btn_W)) { Exit_Btn_W->SetVisibility(WinVis); }
	if (IsValid(Loose_Image)) { Loose_Image->SetVisibility(LoseVis); }
	if (IsValid(Exit_Btn_F)) { Exit_Btn_F->SetVisibility(LoseVis); }
	if (IsValid(Win_Glow))
	{
		Win_Glow->SetVisibility(bIsWin ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
	}

	// Restart cleanly in case this is called more than once
	StopAllAnimations();

	if (bIsWin)
	{
		if (Anim_Win)
		{
			PlayAnimation(Anim_Win);
		}
	}
	else
	{
		if (Anim_Lose)
		{
			PlayAnimation(Anim_Lose);
		}
	}
}

void UResultWidget::OnWinAnimFinished()
{
	// NumLoopsToPlay = 0 -> loop forever
	if (Anim_WinLoop)
	{
		PlayAnimation(Anim_WinLoop, 0.f, 0);
	}
}

void UResultWidget::OnExitButtonClicked()
{
	USFGameInstance* GameInstance = GetWorld() ? GetWorld()->GetGameInstance<USFGameInstance>() : nullptr;
	if (!IsValid(GameInstance))
	{
		return;
	}

	GameInstance->MoveToLobby();
}