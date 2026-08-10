#include "SFResourcePanelWidget.h"

#include "Components/TextBlock.h"
#include "Engine/World.h"
#include "Game/GameState/SFGameState.h"
#include "Logging/SFLog.h"

void USFResourcePanelWidget::NativeConstruct()
{
    Super::NativeConstruct();

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
}

void USFResourcePanelWidget::UpdateEther(
    int32 NewEtherValue,
    float NewEtherRate
)
{
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

    BoundGameState->OnMineralChanged.AddUniqueDynamic(
        this,
        &USFResourcePanelWidget::UpdateMineral
    );

    BoundGameState->OnEtherChanged.AddUniqueDynamic(
        this,
        &USFResourcePanelWidget::UpdateEther
    );

    BoundGameState->OnPopulationChanged.AddUniqueDynamic(
        this,
        &USFResourcePanelWidget::UpdatePopulation
    );

    RefreshResourceData();
}

void USFResourcePanelWidget::UnbindResourceData()
{
    if (!IsValid(BoundGameState))
    {
        BoundGameState = nullptr;
        return;
    }

    BoundGameState->OnMineralChanged.RemoveDynamic(
        this,
        &USFResourcePanelWidget::UpdateMineral
    );

    BoundGameState->OnEtherChanged.RemoveDynamic(
        this,
        &USFResourcePanelWidget::UpdateEther
    );

    BoundGameState->OnPopulationChanged.RemoveDynamic(
        this,
        &USFResourcePanelWidget::UpdatePopulation
    );

    BoundGameState = nullptr;
}

void USFResourcePanelWidget::RefreshResourceData()
{
    if (!IsValid(BoundGameState))
    {
        return;
    }

    UpdateMineral(
        BoundGameState->GetMineral(),
        BoundGameState->GetMineralIncomeRate()
    );

    UpdateEther(
        BoundGameState->GetEther(),
        BoundGameState->GetEtherIncomeRate()
    );

    UpdatePopulation(
        BoundGameState->GetCurrentPopulation(),
        BoundGameState->GetMaximumPopulation()
    );
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