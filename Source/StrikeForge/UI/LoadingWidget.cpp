#include "LoadingWidget.h"
#include "Components/ProgressBar.h"
#include "Components/TextBlock.h"
#include "Components/Image.h"
#include "CommonVideoPlayer.h"
#include "Animation/WidgetAnimation.h"
#include "TimerManager.h"
#include "Game/GameInstance/SFGameInstance.h"
#include "Game/GameMode/SFGameMode.h"
#include "Game/GameState/SFGameState.h"
#include "Logging/SFLog.h"
#include "Game/Multiplayer/SFPVPSessionSubsystem.h"

void ULoadingWidget::NativeConstruct()
{
	Super::NativeConstruct();

	TargetProgress = 0.f;
	DisplayedProgress = 0.f;
	bWaitingForFullBar = false;
	bFinishing = false;
	ApplyDisplayedProgress();

	StartBackgroundVideo();

	if (Anim_FadeIn)
	{
		PlayAnimation(Anim_FadeIn);
	}
	if (Anim_Loop)
	{
		PlayAnimation(Anim_Loop, 0.f, 0);
	}
	if (Anim_FadeOut)
	{
		FadeOutFinishedEvent.BindDynamic(this, &ThisClass::OnFadeOutFinished);
		BindToAnimationFinished(Anim_FadeOut, FadeOutFinishedEvent);
	}

	ShowNextTip();
	if (TipInterval > 0.f && Tips.Num() > 1 && GetWorld())
	{
		GetWorld()->GetTimerManager().SetTimer(
			TipTimerHandle,
			this,
			&ThisClass::ShowNextTip,
			TipInterval,
			true);
	}

	CachedGameState = GetWorld() ?
		GetWorld()->GetGameState<ASFGameState>() : nullptr;

	if (IsValid(CachedGameState))
	{
		CachedGameState->OnStageLoadingProgressChanged.AddUniqueDynamic(
			this,
			&ThisClass::SetProgress);

		CachedGameState->OnStageStateChanged.AddUniqueDynamic(
			this,
			&ThisClass::HandleStageStateChanged);

		SetProgress(CachedGameState->GetStageLoadingProgress());

		const ESFStageState CurrentState = CachedGameState->GetStageState();

		if (CurrentState == ESFStageState::ReadyToStart ||
			CurrentState == ESFStageState::Playing ||
			CurrentState == ESFStageState::InitializationFailed)
		{
			HandleStageStateChanged(CurrentState);
		}
	}
}

void ULoadingWidget::NativeDestruct()
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(TipTimerHandle);
		World->GetTimerManager().ClearTimer(VideoRetryTimerHandle);
		World->GetTimerManager().ClearTimer(FakeProgressTimerHandle);
	}

	if (IsValid(CachedGameState))
	{
		CachedGameState->OnStageLoadingProgressChanged.RemoveDynamic(
			this,
			&ThisClass::SetProgress);

		CachedGameState->OnStageStateChanged.RemoveDynamic(
			this,
			&ThisClass::HandleStageStateChanged);

		CachedGameState = nullptr;
	}

	StopBackgroundVideo();
	Super::NativeDestruct();
}

void ULoadingWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);

	if (DisplayedProgress < TargetProgress)
	{
		// Ease toward the target, but never slower than MinProgressSpeed so it always arrives
		const float Eased = FMath::FInterpTo(DisplayedProgress, TargetProgress, InDeltaTime, ProgressInterpSpeed);
		const float MinStep = DisplayedProgress + MinProgressSpeed * InDeltaTime;
		DisplayedProgress = FMath::Min(FMath::Max(Eased, MinStep), TargetProgress);
		ApplyDisplayedProgress();
	}

	if (bWaitingForFullBar && DisplayedProgress >= 1.f - KINDA_SMALL_NUMBER)
	{
		bWaitingForFullBar = false;
		GetWorld()->GetTimerManager().SetTimer(
			FakeProgressTimerHandle,
			this,
			&ThisClass::BeginFadeOut,
			DelayStartTime,
			false);
	}
}

void ULoadingWidget::StartBackgroundVideo()
{
	if (!IsValid(BackgroundVideo))
	{
		SF_LOG(Warning, "LoadingWidget: BackgroundVideo widget not found. Rename the CommonVideoPlayer to BackgroundVideo.");
		return;
	}

	BackgroundVideo->SetLooping(true);
	BackgroundVideo->Play();

	// The media source opens asynchronously, so an early Play() can be ignored. Retry a few times.
	VideoRetryCount = 0;
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().SetTimer(
			VideoRetryTimerHandle,
			this,
			&ThisClass::RetryPlayVideo,
			0.2f,
			true);
	}
}

void ULoadingWidget::RetryPlayVideo()
{
	if (!IsValid(BackgroundVideo) || BackgroundVideo->IsPlaying() || ++VideoRetryCount > 15)
	{
		GetWorld()->GetTimerManager().ClearTimer(VideoRetryTimerHandle);
		if (IsValid(BackgroundVideo) && !BackgroundVideo->IsPlaying())
		{
			SF_LOG(Warning, "LoadingWidget: background video failed to start. Check the media source and codec.");
		}
		return;
	}

	BackgroundVideo->Play();
}

void ULoadingWidget::StopBackgroundVideo()
{
	if (IsValid(BackgroundVideo))
	{
		BackgroundVideo->Close();
	}
}

void ULoadingWidget::SetProgress(float InPercent01)
{
	TargetProgress = FMath::Max(TargetProgress, FMath::Clamp(InPercent01, 0.f, 1.f));
}

void ULoadingWidget::ApplyDisplayedProgress()
{
	if (IsValid(LoadingBar))
	{
		LoadingBar->SetPercent(DisplayedProgress);
	}
	if (IsValid(PercentText))
	{
		const int32 PercentInt = FMath::FloorToInt(DisplayedProgress * 100.f + KINDA_SMALL_NUMBER);
		PercentText->SetText(FText::FromString(FString::Printf(TEXT("%d%%"), PercentInt)));
	}
}

void ULoadingWidget::HandleStageStateChanged(ESFStageState InGameState)
{
	if (InGameState == ESFStageState::ReadyToStart)
	{
		if(CachedGameState&&CachedGameState->IsPVPMatch())return;
		SetProgress(1.f);
		bWaitingForFullBar = true;
		return;
	}

	if (InGameState == ESFStageState::Playing)
	{
		RemoveFromParent();
		return;
	}

	if (InGameState == ESFStageState::InitializationFailed)
	{
		StopBackgroundVideo();
		if(CachedGameState&&CachedGameState->IsPVPMatch())
		{GetGameInstance()->GetSubsystem<USFPVPSessionSubsystem>()->Leave(false);return;}

		USFGameInstance* SFGameInstance = GetWorld()->GetGameInstance<USFGameInstance>();
		if (!IsValid(SFGameInstance))
		{
			SF_LOG(Error, "Fatal error: GameInstance is null");
			return;
		}
		SFGameInstance->MoveToLobby();
	}
}

void ULoadingWidget::BeginFadeOut()
{
	if (bFinishing)
	{
		return;
	}
	bFinishing = true;

	if (Anim_FadeOut)
	{
		PlayAnimation(Anim_FadeOut);
	}
	else
	{
		FinishLoadingScreen();
	}
}

void ULoadingWidget::OnFadeOutFinished()
{
	FinishLoadingScreen();
}

void ULoadingWidget::FinishLoadingScreen()
{
	UWorld* World = GetWorld();
	ASFGameMode* SFGameMode =
		World ? World->GetAuthGameMode<ASFGameMode>() : nullptr;

	RemoveFromParent();

	if (IsValid(SFGameMode))
	{
		SFGameMode->StartStageAfterLoadingScreen();
	}
}

void ULoadingWidget::SetStageTitle(const FText& InTitle)
{
	if (IsValid(StageTitleText))
	{
		StageTitleText->SetText(InTitle);
	}
}

void ULoadingWidget::ShowNextTip()
{
	if (!IsValid(TipText) || Tips.Num() == 0)
	{
		return;
	}

	int32 Index = FMath::RandRange(0, Tips.Num() - 1);
	if (Tips.Num() > 1 && Index == LastTipIndex)
	{
		Index = (Index + 1) % Tips.Num();
	}
	LastTipIndex = Index;

	TipText->SetText(FText::Format(NSLOCTEXT("Loading", "TipFormat", "TMI: {0}"), Tips[Index]));

	if (Anim_TipFade)
	{
		PlayAnimation(Anim_TipFade);
	}
}
