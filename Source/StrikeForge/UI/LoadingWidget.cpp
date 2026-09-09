#include "LoadingWidget.h"
#include "Components/ProgressBar.h"
#include "Components/TextBlock.h"
#include "Components/Image.h"
#include "MediaPlayer.h"
#include "MediaSource.h"
#include "TimerManager.h"
#include "Game/GameInstance/SFGameInstance.h"
#include "Game/GameMode/SFGameMode.h"
#include "Game/GameState/SFGameState.h"
#include "Logging/SFLog.h"

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
		
		// 타이밍으로 UI가 늦어 이미 플레이중이라면 + 로딩실패시
		// 근데 이럴일 거의 없음 진짜 없음
		const ESFStageState CurrentState = CachedGameState->GetStageState();

		if (CurrentState == ESFStageState::ReadyToStart ||
			CurrentState == ESFStageState::Playing ||
			CurrentState == ESFStageState::InitializationFailed)
		{
			HandleStageStateChanged(CurrentState);
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
	// Initializing                 UMETA(DisplayName = "Initializing"),
	// InitializationFailed         UMETA(DisplayName = "Initialization Failed"),
	// LoadingRaceData              UMETA(DisplayName = "Loading Race Data"),
	// LoadingInitialBuildingAssets UMETA(DisplayName = "Loading Initial Building Assets"),
	// ReadyToStart                 UMETA(DisplayName = "Ready To Start"),
	// Playing                      UMETA(DisplayName = "Playing"),
	// Finished                     UMETA(DisplayName = "Finished"),
	// };
	
	// 로딩을 성공적으로 끝마치고 로딩이 100%가 되는 시점
	if (InGameState == ESFStageState::ReadyToStart)
	{
		// 로딩 100% set
		SetProgress(1.f);
		// TODO:: FadeAnimation Play, On Animation Finished에 FinishLoadingScreen바인딩
		// 필요하다면 1초 Delay
		
		
		// Not Animation After Loading Finished.
		// For now, Delay 1.0 Time Before StartStage Using Timer
		GetWorld()->GetTimerManager().SetTimer(
			FakeProgressTimerHandle,
			this,
			&ThisClass::FinishLoadingScreen,
			DelayStartTime,
			false);
		return;
	}

	if (InGameState == ESFStageState::Playing)
	{	// 이경우도 거의 없다. 로딩 UI가 로딩이 끝난 GameMode, GameState보다 늦는경우
		RemoveFromParent();
		return;
	}

	if (InGameState == ESFStageState::InitializationFailed)
	{
		if (IsValid(BackgroundMediaPlayer))
		{
			BackgroundMediaPlayer->Close();
		}

		
		// TODO:: 로딩이 실패했습니다 작은 UI 생성, 재시도버튼, 로비버튼
		// 재시도(현재스테이지 재진입) == SFGameInstance의 RetryCurrentStage()호출
		// 로비(로비이동) == SFGameInstance의 MoveToLobby()
		// 사실 이 흐름이 발생하면 안돼는것이긴함.
		// 로딩이 실패하는경우는 에셋설정이 비어있거나,
		// 팀인덱스를 잘못입력했다던가하는
		// 휴먼에러에 기반하는 것이기 때문임.
		USFGameInstance* SFGameInstance = GetWorld()->GetGameInstance<USFGameInstance>();
		if (!IsValid(SFGameInstance))
		{
			SF_LOG(Error, "치명적 오류발생 GameInstance is Null");
			// TODO:: 게임종료
			
			return;
		}
		SFGameInstance->MoveToLobby();
	}
}

void ULoadingWidget::FinishLoadingScreen()
{
	UWorld* World = GetWorld();
	ASFGameMode* SFGameMode =
		World ? World->GetAuthGameMode<ASFGameMode>() : nullptr;

	RemoveFromParent();

	if (IsValid(SFGameMode))
	{	// Notify Finished_Loading to GameMode.
		SFGameMode->StartStageAfterLoadingScreen();
		// GameMode StartStage -> GameState StartStage
	}
}
