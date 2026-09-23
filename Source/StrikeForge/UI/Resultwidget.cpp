#include "ResultWidget.h"
#include "Components/Button.h"
#include "Components/Widget.h"
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
}

void UResultWidget::SetResultState(bool bIsWin)
{
	const ESlateVisibility WinVis = bIsWin ? ESlateVisibility::Visible : ESlateVisibility::Collapsed;
	const ESlateVisibility LoseVis = bIsWin ? ESlateVisibility::Collapsed : ESlateVisibility::Visible;

	if (IsValid(Win_Image)) { Win_Image->SetVisibility(WinVis); }
	if (IsValid(Exit_Btn_W)) { Exit_Btn_W->SetVisibility(WinVis); }
	if (IsValid(Loose_Image)) { Loose_Image->SetVisibility(LoseVis); }
	if (IsValid(Exit_Btn_F)) { Exit_Btn_F->SetVisibility(LoseVis); }
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