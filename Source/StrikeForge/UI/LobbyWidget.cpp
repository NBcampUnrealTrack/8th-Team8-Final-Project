#include "LobbyWidget.h"
#include "SettingWidget.h"
#include "Components/Button.h"
#include "StrikeForge/Game/GameInstance/SFGameInstance.h"
#include "Datas/Stage/SFStageData.h"
#include "Engine/Texture2D.h"
#include "Game/Subsystem/SFDataTableManager.h"
#include "Styling/SlateTypes.h"

void ULobbyWidget::NativeConstruct()
{
	Super::NativeConstruct();

	if (IsValid(Back_Btn))
	{
		Back_Btn->OnClicked.AddDynamic(this, &ULobbyWidget::OnBackButtonClicked);
	}
	if (IsValid(Setting_Btn))
	{
		Setting_Btn->OnClicked.AddDynamic(this, &ULobbyWidget::OnSettingButtonClicked);
	}
	if (IsValid(Stage1))
	{
		Stage1->OnClicked.AddDynamic(this, &ULobbyWidget::OnStage1Clicked);
	}
	if (IsValid(Stage2))
	{
		Stage2->OnClicked.AddDynamic(this, &ULobbyWidget::OnStage2Clicked);
	}
	if (IsValid(Stage3))
	{
		Stage3->OnClicked.AddDynamic(this, &ULobbyWidget::OnStage3Clicked);
	}
	if (IsValid(Stage4))
	{
		Stage4->OnClicked.AddDynamic(this, &ULobbyWidget::OnStage4Clicked);
	}
	if (IsValid(Stage5))
	{
		Stage5->OnClicked.AddDynamic(this, &ULobbyWidget::OnStage5Clicked);
	}
	if (IsValid(Stage6))
	{
		Stage6->OnClicked.AddDynamic(this, &ULobbyWidget::OnStage6Clicked);
	}
}

void ULobbyWidget::OnBackButtonClicked()
{
	USFGameInstance* GameInstance = GetWorld() ? GetWorld()->GetGameInstance<USFGameInstance>() : nullptr;
	if (!IsValid(GameInstance))
	{
		return;
	}

	GameInstance->MoveToTitle();
}

void ULobbyWidget::OnSettingButtonClicked()
{
	if (IsValid(SettingWidget))
	{
		SettingWidget->OpenSetting();
	}
}

void ULobbyWidget::OnStage1Clicked() { OpenStageLevel(0); }
void ULobbyWidget::OnStage2Clicked() { OpenStageLevel(1); }
void ULobbyWidget::OnStage3Clicked() { OpenStageLevel(2); }
void ULobbyWidget::OnStage4Clicked() { OpenStageLevel(3); }
void ULobbyWidget::OnStage5Clicked() { OpenStageLevel(4); }
void ULobbyWidget::OnStage6Clicked() { OpenStageLevel(5); }

void ULobbyWidget::OpenStageLevel(int32 StageIndex)
{
	if (!StageRowNames.IsValidIndex(StageIndex))
	{
		return;
	}

	USFGameInstance* GameInstance = GetWorld() ? GetWorld()->GetGameInstance<USFGameInstance>() : nullptr;
	if (!IsValid(GameInstance))
	{
		return;
	}

	if (!GameInstance->SelectStage(StageRowNames[StageIndex]))
	{
		return;
	}

	if (GameInstance->GetSelectedRace() == ESFRace::None)
	{
		// 종족 선택 UI를 여는 함수 작성자리
		return;
	}

	GameInstance->MoveToStage();
}

// 26.08.11 추가
void ULobbyWidget::RefreshStageButtons()
{
	const TArray<UButton*> StageButtons =
	{
		Stage1,
		Stage2,
		Stage3,
		Stage4,
		Stage5,
		Stage6
	};

	const int32 StageCount =
		FMath::Min(StageButtons.Num(), StageRowNames.Num());

	for (int32 StageIndex = 0;
		StageIndex < StageCount;
		++StageIndex)
	{
		RefreshStageButton(
			StageButtons[StageIndex],
			StageRowNames[StageIndex]
		);
	}
}

void ULobbyWidget::RefreshStageButton(UButton* StageButton, FName StageRowName)
{
	if (!IsValid(StageButton) || StageRowName.IsNone())
	{
		return;
	}

	USFGameInstance* GameInstance = GetWorld() ? GetWorld()->GetGameInstance<USFGameInstance>() : nullptr;

	if (!IsValid(GameInstance))
	{
		return;
	}

	USFDataTableManager* DataTableManager = GameInstance->GetSubsystem<USFDataTableManager>();

	if (!IsValid(DataTableManager))
	{
		return;
	}

	const FSFStageData* StageData = DataTableManager->GetDataRow<FSFStageData>(EDataTableType::Stage, StageRowName, TEXT("RefreshStageButton"));

	if (!StageData)
	{
		return;
	}

	const ESFStageProgressState StageState = GameInstance->GetStageProgressState(StageRowName);
	const TSoftObjectPtr<UTexture2D>* SelectedImage = nullptr;

	switch (StageState)
	{
	case ESFStageProgressState::Locked:
		SelectedImage = &StageData->LockedImage;
		StageButton->SetIsEnabled(false);
		break;

	case ESFStageProgressState::Unlocked:
		SelectedImage = &StageData->UnlockedImage;
		StageButton->SetIsEnabled(true);
		break;

	case ESFStageProgressState::Cleared:
		SelectedImage = &StageData->ClearedImage;
		StageButton->SetIsEnabled(true);
		break;

	default:
		StageButton->SetIsEnabled(false);
		return;
	}

	if (!SelectedImage || SelectedImage->IsNull())
	{
		return;
	}

	UTexture2D* StageTexture = SelectedImage->LoadSynchronous();

	if (!IsValid(StageTexture))
	{
		return;
	}

	ApplyStageButtonTexture(StageButton, StageTexture);
}

void ULobbyWidget::ApplyStageButtonTexture(UButton* StageButton, UTexture2D* StageTexture)
{
	if (!IsValid(StageButton) || !IsValid(StageTexture))
	{
		return;
	}

	FButtonStyle ButtonStyle = StageButton->GetStyle();

	ButtonStyle.Normal.SetResourceObject(StageTexture);
	ButtonStyle.Hovered.SetResourceObject(StageTexture);
	ButtonStyle.Pressed.SetResourceObject(StageTexture);
	ButtonStyle.Disabled.SetResourceObject(StageTexture);

	StageButton->SetStyle(ButtonStyle);
}
// 여기까지