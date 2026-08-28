#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "SFSelectInfo/SFSelectableInfo.h"
#include "SFInGameHUDWidget.generated.h"

class AActor;
class UButton;
class UBorder;
class UCommonActivatableWidget;
class UCommonActivatableWidgetStack;
class UResultWidget;
class USFProductionBuildingPanelWidget;
class USFSelectionPanelWidget;
class UHealthComponent;
class UDecalComponent;
class UMaterialInterface;
class USFMultiSelectionPanelWidget;
class UDataTable;
class USFStageIntroWidget;
class ASFUnitPortraitActor;

UCLASS(Blueprintable)
class STRIKEFORGE_API USFInGameHUDWidget
    : public UUserWidget
{
    GENERATED_BODY()

    // ==================== Functions ====================

public:
    UFUNCTION(
        BlueprintCallable,
        Category = "SF|HUD|Building"
    )
    void OpenBuildingMode();

    UFUNCTION(
        BlueprintCallable,
        Category = "SF|HUD|Building"
    )
    void CloseBuildingMode();
    
    UFUNCTION(
        BlueprintCallable,
        Category = "SF|HUD|Settings"
    )
    void ToggleSettings();

    void ShowResultWidget(bool bIsWin);

    void SelectActor(AActor* Actor);

    bool IsPointerOverSelectionAction() const;

    UFUNCTION(BlueprintCallable, Category = "SF|HUD|Building")
    void ToggleBuildingMode();

    /**
     * 현재 다중 선택된 액터 목록.
     *
     * 이동 명령을 선택 전체에 나눠주기 위해 PlayerController 에서 조회한다.
     * 목록이 비어 있으면 단일 선택(SelectedActor)만 있는 상태다.
     *
     * UFUNCTION 을 붙이지 않는 이유:
     * UHT 는 TObjectPtr 을 함수 반환값으로 허용하지 않는다.
     * C++ 에서만 호출하므로 리플렉션 노출이 필요 없다.
     */
    const TArray<TObjectPtr<AActor>>& GetSelectedActors() const { return SelectedActors; }

    UFUNCTION(BlueprintCallable, Category = "SF|Command|Feedback")
    void ShowMoveDestinationIndicator(const FVector& WorldLocation);

    UFUNCTION(BlueprintCallable, Category = "SF|HUD|Production")
    bool RequestProductionBySlot(int32 SlotIndex);

protected:
    virtual void NativeConstruct() override;

    virtual void NativeDestruct() override;

    UFUNCTION()
    void HandleSelectedHealthChanged(float BeforeHP, float CurrentHP, float MaxHP);

    void BindSelectedHealthDelegate(AActor* Actor);

    void UnbindSelectedHealthDelegate();

    virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

private:
    UFUNCTION()
    void HandleSelectInput();

    UFUNCTION()
    void HandleSelectedActorDestroyed(
        AActor* DestroyedActor
    );

    void PerformSelectionTrace();

    void ApplySelection(
        AActor* NewSelectedActor,
        const FSFSelectableInfo& NewSelectedInfo
    );

    void ApplySelectionUIData(
        FSFSelectableInfo& InOutSelectableInfo
    );

    void RefreshSelectionPanels();

    /**
    * 맵에 배치된 Portrait Actor를 저장할 포인터
    * 선택 초상화를 갱신하는 함수
    */
    void RefreshSelectedPortrait();

    void ClearSelection();

    void BindSelectedActorsDestroyed();

    void UnbindSelectedActorsDestroyed();

    void HandleBuildingModeDeactivated();

    void RefreshBuildModeOpenButtonVisibility();

    UFUNCTION()
    void HandleTierUpgradeRequested();

    void HandleSettingsDeactivated();

    UPROPERTY(Transient)
    TObjectPtr<UCommonActivatableWidget> ActiveSettingsWidget = nullptr;

    void ShowSelectionIndicator(AActor* Actor);

    void HideSelectionIndicator(AActor* Actor);

    void HideAllSelectionIndicators();

    UFUNCTION()
    void HandleSelectionDragStarted(FVector2D StartScreenPosition);

    UFUNCTION()
    void HandleSelectionDragCompleted(
        FVector2D StartScreenPosition,
        FVector2D EndScreenPosition
    );

    void UpdateSelectionDragBox(FVector2D CurrentScreenPosition);

    void PerformDragSelection(
        FVector2D StartScreenPosition,
        FVector2D EndScreenPosition
    );

    bool IsOwnedByLocalPlayer(AActor* Actor) const;

    void ApplyMultipleSelection(
        const TArray<AActor*>& NewSelectedActors
    );

    int32 GetSelectionPriority(AActor* Actor) const;

    void RefreshMultiSelectionPanel();

    UFUNCTION()
    void HandlePrimarySelectionRequested(AActor* Actor);

    void TryStartStageIntro();

    /**
     * 스테이지 인트로 위젯을 열고 지정한 노드부터 재생한다.
     *
     * TryStartStageIntro 의 본문을 그대로 옮겨온 것이다. 여는 절차(위젯 생성,
     * HUD 숨김, 일시정지, 입력 모드 전환, OnDeactivated 바인딩)가 한 곳에만
     * 있어야 목표 대기 후 재개할 때 상태가 어긋나지 않는다.
     *
     * @param InNodeIndex INDEX_NONE 이면 행에 지정된 StartNodeIndex 부터 시작한다.
     *                    목표 대기에서 돌아올 때는 멈췄던 노드 번호가 들어온다.
     */
    bool OpenStageIntro(
        UDataTable* InDataTable,
        FName InRowName,
        int32 InNodeIndex
    );

    /**
     * 튜토리얼 목표 대기가 끝났다. 멈췄던 노드부터 인트로를 다시 연다.
     *
     * USFTutorialSubsystem::OnResumeRequested 에 연결된다. 동적 델리게이트라
     * UFUNCTION 이 필요하다.
     */
    UFUNCTION()
    void HandleTutorialResumeRequested(
        UDataTable* InDataTable,
        FName InRowName,
        int32 InNodeIndex
    );

    void HandleStageIntroDeactivated();
    
    FName ResolveStageIntroRowName() const;

    // ==================== Variables ====================

protected:
    UPROPERTY(
        Transient,
        BlueprintReadOnly,
        Category = "SF|Selection"
    )
    TObjectPtr<AActor> SelectedActor = nullptr;

    UPROPERTY(
        Transient,
        BlueprintReadOnly,
        Category = "SF|Selection"
    )
    TArray<TObjectPtr<AActor>> SelectedActors;

    UPROPERTY(
        Transient,
        BlueprintReadOnly,
        Category = "SF|Selection"
    )
    FSFSelectableInfo SelectedInfo;

    // 26.08.27 추가분
    UPROPERTY(
        Transient,
        BlueprintReadOnly,
        Category = "SF|Portrait"
    )
    TObjectPtr<ASFUnitPortraitActor> UnitPortraitActor = nullptr;

    UPROPERTY(
        BlueprintReadOnly,
        Category = "SF|HUD",
        meta = (BindWidgetOptional)
    )
    TObjectPtr<USFSelectionPanelWidget> SelectionPanel = nullptr;

    UPROPERTY(
        BlueprintReadOnly,
        Category = "SF|HUD|Selection",
        meta = (BindWidget)
    )
    TObjectPtr<USFMultiSelectionPanelWidget> MultiSelectionPanel = nullptr;

    UPROPERTY(
        BlueprintReadOnly,
        Category = "SF|HUD|Production",
        meta = (BindWidgetOptional)
    )
    TObjectPtr<USFProductionBuildingPanelWidget>
        ProductionBuildingPanel = nullptr;

    UPROPERTY(
        BlueprintReadOnly,
        Category = "SF|HUD|Building",
        meta = (BindWidget)
    )
    TObjectPtr<UCommonActivatableWidgetStack> BuildingUIStack = nullptr;

    UPROPERTY(
        BlueprintReadOnly,
        Category = "SF|HUD|Building",
        meta = (BindWidget)
    )
    TObjectPtr<UButton> BuildModeOpenButton = nullptr;

    UPROPERTY(
        EditDefaultsOnly,
        BlueprintReadOnly,
        Category = "SF|HUD|Building"
    )
    TSubclassOf<UCommonActivatableWidget> BuildingModeWidgetClass;

    UPROPERTY(
        BlueprintReadOnly,
        Category = "SF|Selection",
        meta = (BindWidget)
    )
    TObjectPtr<UBorder> SelectionDragBox = nullptr;

    UPROPERTY(EditDefaultsOnly,BlueprintReadOnly,Category = "SF|HUD|Result")
    TSubclassOf<UResultWidget> ResultWidgetClass;

    UPROPERTY(BlueprintReadOnly, Category = "SF|HUD|Settings", meta = (BindWidget))
    TObjectPtr<UCommonActivatableWidgetStack> SettingsUIStack = nullptr;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "SF|HUD|Settings")
    TSubclassOf<UCommonActivatableWidget> SettingsWidgetClass;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "SF|Selection|Feedback")
    TObjectPtr<UMaterialInterface> SelectionIndicatorMaterial = nullptr;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "SF|Command|Feedback")
    TObjectPtr<UMaterialInterface> MoveDestinationIndicatorMaterial = nullptr;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "SF|Command|Feedback")
    FVector MoveDestinationIndicatorSize = FVector(24.f, 70.f, 70.f);

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "SF|Command|Feedback")
    float MoveDestinationIndicatorLifeTime = 0.65f;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "SF|Command|Feedback")
    float MoveDestinationIndicatorFadeDuration = 0.3f;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "SF|HUD|Stage Intro")
    TSubclassOf<USFStageIntroWidget> StageIntroWidgetClass;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "SF|HUD|Stage Intro")
    TObjectPtr<UDataTable> StageIntroDataTable = nullptr;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "SF|HUD|Stage Intro")
    FName StageIntroFallbackRowName = NAME_None;

private:
    UPROPERTY(Transient)
    TObjectPtr<UCommonActivatableWidget> ActiveBuildingModeWidget = nullptr;

    UPROPERTY(Transient)
    TObjectPtr<UResultWidget> ResultWidget = nullptr;

    UPROPERTY(Transient)
    TWeakObjectPtr<UHealthComponent> SelectedHealthComponent;

    UPROPERTY(Transient)
    TMap<TObjectPtr<AActor>, TObjectPtr<UDecalComponent>> SelectionIndicatorComponents;

    FVector2D SelectionDragStartPosition = FVector2D::ZeroVector;
    bool bIsSelectionDragging = false;

    UPROPERTY(Transient)
    TObjectPtr<USFStageIntroWidget> ActiveStageIntroWidget = nullptr;

    ESlateVisibility VisibilityBeforeStageIntro = ESlateVisibility::SelfHitTestInvisible;
    bool bWasGamePausedBeforeStageIntro = false;
};