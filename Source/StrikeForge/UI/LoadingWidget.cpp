#include "LoadingWidget.h"
#include "Components/ProgressBar.h"
#include "Components/TextBlock.h"
#include "Components/Image.h"
#include "MediaPlayer.h"
#include "MediaSource.h"
#include "TimerManager.h"
#include "Game/GameState/SFGameState.h"

void ULoadingWidget::NativeConstruct()
{
	Super::NativeConstruct();

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

		// SetProgress(0)을 대신함. progress 0또한 로딩시작시 설정함.
		SetProgress(CachedGameState->GetStageLoadingProgress());
		
		// 타이밍으로 UI가 늦어 이미 플레이중이라면
		// 근데 이럴일 거의 없음 진짜 없음
		if (CachedGameState->IsStagePlaying())
		{
			HandleStageStateChanged(ESFStageState::Playing);
			return;
		}
	}
	
	//CurrentProgress = 0.f;
	//SetProgress(0.f);
	
	// 현재 비동기 작업중에 있습니다. 이 타이머는 비동기가 완료될때 없어질 예정입니다.
	// 데이터를 비동기해서 로딩하면 완료될때마다 수신을 받을수있습니다.
	// 그때마다 진행도를 올리는 방식을 채택할 예정입니다. 따라서 타이머를 쓰지않게됩니다.
	// GetWorld()->GetTimerManager().SetTimer(
	// 	FakeProgressTimerHandle, this,
	// 	&ULoadingWidget::TickFakeProgress, 0.05f, true);

	// ��� ���� ���
	if (IsValid(BackgroundMediaPlayer) && IsValid(BackgroundVideoSource))
	{
		BackgroundMediaPlayer->OpenSource(BackgroundVideoSource);
		BackgroundMediaPlayer->SetLooping(true);
		BackgroundMediaPlayer->Play();
	}
}

void ULoadingWidget::NativeDestruct()
{
	// 델리게이트 unbind + 캐싱state 제거
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
	
	if (IsValid(BackgroundMediaPlayer))
	{
		BackgroundMediaPlayer->Close();
	}
	Super::NativeDestruct();
}
//
// void ULoadingWidget::TickFakeProgress()
// {
// 	CurrentProgress = FMath::Min(CurrentProgress + 0.02f, 1.f);
// 	SetProgress(CurrentProgress);
//
// 	if (CurrentProgress >= 1.f)
// 	{
// 		GetWorld()->GetTimerManager().ClearTimer(FakeProgressTimerHandle);
// 	}
// }

void ULoadingWidget::SetProgress(float InPercent01)
{
	if (!IsValid(LoadingBar)) return;
	if (!IsValid(PercentText)) return;
	
	InPercent01 = FMath::Clamp(InPercent01, 0.f, 1.f);
	
	LoadingBar->SetPercent(InPercent01);

	const int32 PercentInt = FMath::RoundToInt(InPercent01 * 100.f);
	PercentText->SetText(FText::FromString(FString::Printf(TEXT("%d%%"), PercentInt)));
}

void ULoadingWidget::HandleStageStateChanged(ESFStageState InGameState)
{
	// GameState가 바뀌었을때.. 현재의 게임스테이트 종류
	// enum class ESFStageState : uint8
	// {
	// 	Initializing UMETA(DisplayName = "Initializing"),
	// 	Playing      UMETA(DisplayName = "Playing"),
	// 	Finished     UMETA(DisplayName = "Finished")
	// };
	
	// 게임상태가 시작일때 100%만들고 UI제거
	if (InGameState == ESFStageState::Playing)
	{
		SetProgress(1.f);
		RemoveFromParent();
	}
}
