#include "SFResourcePanelWidget.h"
#include "Components/TextBlock.h"
#include "Engine/World.h"
#include "Game/GameState/SFGameState.h"
#include "InGame/Controller/SFInGamePlayerController.h"
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