#include "LoadingWidget.h"
#include "Components/ProgressBar.h"
#include "Components/TextBlock.h"
#include "Components/Image.h"
#include "MediaPlayer.h"
#include "MediaSource.h"
#include "TimerManager.h"

void ULoadingWidget::NativeConstruct()
{
	Super::NativeConstruct();

	CurrentProgress = 0.f;
	SetProgress(0.f);

	GetWorld()->GetTimerManager().SetTimer(
		FakeProgressTimerHandle, this,
		&ULoadingWidget::TickFakeProgress, 0.05f, true);

	// 배경 영상 재생
	if (IsValid(BackgroundMediaPlayer) && IsValid(BackgroundVideoSource))
	{
		BackgroundMediaPlayer->OpenSource(BackgroundVideoSource);
		BackgroundMediaPlayer->SetLooping(true);
		BackgroundMediaPlayer->Play();
	}
}

void ULoadingWidget::NativeDestruct()
{
	if (IsValid(BackgroundMediaPlayer))
	{
		BackgroundMediaPlayer->Close();
	}

	GetWorld()->GetTimerManager().ClearTimer(FakeProgressTimerHandle);

	Super::NativeDestruct();
}

void ULoadingWidget::TickFakeProgress()
{
	CurrentProgress = FMath::Min(CurrentProgress + 0.02f, 1.f);
	SetProgress(CurrentProgress);

	if (CurrentProgress >= 1.f)
	{
		GetWorld()->GetTimerManager().ClearTimer(FakeProgressTimerHandle);
	}
}

void ULoadingWidget::SetProgress(float InPercent01)
{
	InPercent01 = FMath::Clamp(InPercent01, 0.f, 1.f);

	if (IsValid(LoadingBar))
	{
		LoadingBar->SetPercent(InPercent01);
	}
	if (IsValid(PercentText))
	{
		const int32 PercentInt = FMath::RoundToInt(InPercent01 * 100.f);
		PercentText->SetText(FText::FromString(FString::Printf(TEXT("%d%%"), PercentInt)));
	}
}