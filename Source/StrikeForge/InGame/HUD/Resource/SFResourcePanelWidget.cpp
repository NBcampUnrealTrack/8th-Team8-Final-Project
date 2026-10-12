#include "SFResourcePanelWidget.h"
#include "Animation/WidgetAnimation.h"
#include "Components/TextBlock.h"
#include "Engine/World.h"
#include "Game/GameState/SFGameState.h"
#include "InGame/Controller/SFInGamePlayerController.h"
#include "Logging/SFLog.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/ScaleBox.h"
#include "Components/ScaleBoxSlot.h"
#include "Components/SizeBox.h"
#include "Components/HorizontalBoxSlot.h"

void USFResourcePanelWidget::NativeConstruct()
{
    Super::NativeConstruct();

    // Let long numbers take their natural width. The enclosing row does the
    // fitting once, so nested scale boxes cannot make just the numbers tiny.
    for (const TCHAR* Name : { TEXT("MineralValueTextFixedWidth"), TEXT("EtherValueTextFixedWidth") })
        if (auto* Box = Cast<USizeBox>(GetWidgetFromName(Name))) Box->ClearWidthOverride();
    for (const TCHAR* Name : { TEXT("MineralValueTextAutoFit"), TEXT("EtherValueTextAutoFit") })
        if (auto* Box = Cast<UScaleBox>(GetWidgetFromName(Name))) Box->SetStretch(EStretch::None);
    for (const TCHAR* Name : { TEXT("MineralResourceBox"), TEXT("EtherResourceBox"), TEXT("PopulationResourceBox") })
        if (auto* Box = GetWidgetFromName(Name))
            if (auto* GroupSlot = Cast<UHorizontalBoxSlot>(Box->Slot)) GroupSlot->SetPadding(FMargin(10.f, 0.f, 0.f, 0.f));

    // Fit the complete row, including income and population, inside the frame.
    // Fitting only the two resource values left population free to overflow.
    if (auto* Background = Cast<UBorder>(GetWidgetFromName(TEXT("ResourcePanelBackground"))))
    {
        UWidget* Content = GetWidgetFromName(TEXT("ResourceContent"));
        if (Content && Content->GetParent() == Background)
        {
            auto* Fit = WidgetTree->ConstructWidget<UScaleBox>(UScaleBox::StaticClass(), TEXT("ResourceRowFit"));
            Content->RemoveFromParent();
            Fit->SetStretch(EStretch::ScaleToFit);
            Fit->SetStretchDirection(EStretchDirection::DownOnly);
            Fit->SetClipping(EWidgetClipping::ClipToBoundsAlways);
            auto* FitSlot = Cast<UScaleBoxSlot>(Fit->AddChild(Content));
            FitSlot->SetHorizontalAlignment(HAlign_Center);
            FitSlot->SetVerticalAlignment(VAlign_Center);
            Background->SetPadding(FMargin(14.f, 5.f));
            Background->SetContent(Fit);
        }
    }

    BindResourceData();
}

void USFResourcePanelWidget::NativeDestruct()
{
    UnbindResourceData();

    Super::NativeDestruct();
}

void USFResourcePanelWidget::UpdateMineral(
    int32 NewMineralValue,
    float NewMineralRate
)
{
    const bool bMineralIncreased =
        bHasPreviousMineralValue &&
        NewMineralValue > PreviousMineralValue;

    if (IsValid(MineralValueText))
    {
        MineralValueText->SetText(
            FText::AsNumber(
                NewMineralValue
            )
        );
    }

    if (IsValid(MineralRateText))
    {
        MineralRateText->SetText(
            MakeResourceRateText(
                NewMineralRate
            )
        );
    }

    if (
        bMineralIncreased &&
        IsValid(MineralGainPop)
        )
    {
        PlayAnimation(
            MineralGainPop
        );
    }

    PreviousMineralValue =
        NewMineralValue;

    bHasPreviousMineralValue =
        true;
}

void USFResourcePanelWidget::UpdateEther(
    int32 NewEtherValue,
    float NewEtherRate
)
{
    const bool bEtherIncreased =
        bHasPreviousEtherValue &&
        NewEtherValue > PreviousEtherValue;

    if (IsValid(EtherValueText))
    {
        EtherValueText->SetText(
            FText::AsNumber(
                NewEtherValue
            )
        );
    }

    if (IsValid(EtherRateText))
    {
        EtherRateText->SetText(
            MakeResourceRateText(
                NewEtherRate
            )
        );
    }

    if (
        bEtherIncreased &&
        IsValid(EtherGainPop)
        )
    {
        PlayAnimation(
            EtherGainPop
        );
    }

    PreviousEtherValue =
        NewEtherValue;

    bHasPreviousEtherValue =
        true;
}

void USFResourcePanelWidget::UpdatePopulation(
    int32 CurrentPopulation,
    int32 MaximumPopulation
)
{
    if (!IsValid(PopulationValueText))
    {
        return;
    }

    PopulationValueText->SetText(
        FText::Format(
            FText::FromString(
                TEXT("{0} / {1}")
            ),
            FText::AsNumber(
                CurrentPopulation
            ),
            FText::AsNumber(
                MaximumPopulation
            )
        )
    );
}

void USFResourcePanelWidget::BindResourceData()
{
    UnbindResourceData();

    UWorld* World = GetWorld();

    if (!IsValid(World))
    {
        SF_LOG(
            Error,
            "[SFResourcePanelWidget] World is invalid."
        );

        return;
    }

    BoundGameState =
        World->GetGameState<ASFGameState>();

    if (!IsValid(BoundGameState))
    {
        SF_LOG(
            Error,
            "[SFResourcePanelWidget] SFGameState is invalid."
        );

        return;
    }
    
    CachedPlayerTeamIndex = 0;
    if (ASFInGamePlayerController* SFPC = Cast<ASFInGamePlayerController>(GetOwningPlayer()))
    {
        CachedPlayerTeamIndex = SFPC->GetMyTeamIndex();
    }
    
    USFTeamState* MyTeam = BoundGameState->FindOrAddTeam(CachedPlayerTeamIndex);
    if (!MyTeam)
    {
        SF_LOG(Warning, "[SFResourcePanelWidget] Team %d 의 상태가 아직 없습니다.", CachedPlayerTeamIndex);
        return;
    }

    MyTeam->OnMineralChanged.AddUniqueDynamic(this, &USFResourcePanelWidget::UpdateMineral);
    MyTeam->OnEtherChanged.AddUniqueDynamic(this, &USFResourcePanelWidget::UpdateEther);
    MyTeam->OnPopulationChanged.AddUniqueDynamic(this, &USFResourcePanelWidget::UpdatePopulation);

    RefreshResourceData();
}

void USFResourcePanelWidget::UnbindResourceData()
{
    if (!IsValid(BoundGameState))
    {
        BoundGameState = nullptr;
        return;
    }

    if (USFTeamState* MyTeam = BoundGameState->GetTeamState(CachedPlayerTeamIndex))
    {
        MyTeam->OnMineralChanged.RemoveDynamic(this, &USFResourcePanelWidget::UpdateMineral);
        MyTeam->OnEtherChanged.RemoveDynamic(this, &USFResourcePanelWidget::UpdateEther);
        MyTeam->OnPopulationChanged.RemoveDynamic(this, &USFResourcePanelWidget::UpdatePopulation);
    }

    BoundGameState = nullptr;
}

void USFResourcePanelWidget::RefreshResourceData()
{
    if (!IsValid(BoundGameState))
    {
        return;
    }

    const USFTeamState* MyTeam = BoundGameState->GetTeamState(CachedPlayerTeamIndex);
    if (!MyTeam) return;

    UpdateMineral(MyTeam->GetMineral(), MyTeam->GetMineralIncomeRate());
    UpdateEther(MyTeam->GetEther(), MyTeam->GetEtherIncomeRate());
    UpdatePopulation(MyTeam->GetCurrentPopulation(), MyTeam->GetMaximumPopulation());
    
}

FText USFResourcePanelWidget::MakeResourceRateText(
    float ResourceRate
) const
{
    const int32 RoundedRate =
        FMath::RoundToInt(
            ResourceRate
        );

    if (RoundedRate > 0)
    {
        return FText::Format(
            FText::FromString(
                TEXT("+{0}/s")
            ),
            FText::AsNumber(
                RoundedRate
            )
        );
    }

    return FText::Format(
        FText::FromString(
            TEXT("{0}/s")
        ),
        FText::AsNumber(
            RoundedRate
        )
    );
}
