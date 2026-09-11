#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "SFSelectInfo/SFSelectableInfo.h"
#include "Datas/Race/SFRaceType.h"
#include "SFSelectInfo/SFSelectionUIDataRow.h"
// ESFStageState 를 UFUNCTION 인자로 받으려면 완전한 타입이 필요하다.
// 전방 선언만으로는 UHT 가 델리게이트 시그니처를 만들지 못한다.
#include "Game/GameState/SFGameState.h"
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
class USoundBase;
class ASFProductionBuilding;
class AUnitBase;
class UTexture2D;
struct FSFSelectionUIDataRow;
class ASFBaseBuilding;
class UStaticMeshComponent;
class UStaticMesh;
class UWidget;

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
    void ShowMoveDestinationIndicator(const FVector& WorldLocation, bool bIsAttack);

    UFUNCTION(BlueprintCallable, Category = "SF|HUD|Production")
    bool RequestProductionBySlot(int32 SlotIndex);

    UFUNCTION(BlueprintCallable, Category = "SF|HUD|Sound")
    void PlayBuildStartSound();

    UFUNCTION(BlueprintCallable, Category = "SF|HUD|Sound")
    void PlayResourceWarningSound();

    UFUNCTION()
    void HandleSelectedProductionBuildingTierActivated(
        const TArray<FName>& SpawnableRowNames
    );

    void BindSelectedProductionBuilding(AActor* Actor);

    void UnbindSelectedProductionBuilding();

    UPROPERTY(Transient)
    TWeakObjectPtr<ASFProductionBuilding>
        SelectedProductionBuilding;

    UFUNCTION(BlueprintCallable, Category = "SF|HUD|Rally")
    void RefreshRallyPointIndicator();

    bool IsPointerOverHUDPanel();

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
    void HandleProductionResourceWarningRequested();

    UFUNCTION()
    void HandleSelectedActorDestroyed(
        AActor* DestroyedActor
    );

    void PerformSelectionTrace();

    void ApplySelection(
        AActor* NewSelectedActor,
        const FSFSelectableInfo& NewSelectedInfo
    );


    ESFRace ResolveActorRace(
        AActor* Actor
    ) const;

    UTexture2D* ResolvePortraitImage(
        const FSFSelectionUIDataRow& UIData,
        ESFRace Race,
        int32 Tier
    ) const;

    void ApplySelectionUIData(
        AActor* SourceActor,
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
     * 스테이지가 실제로 시작될 때 인트로를 연다.
     *
     * 예전에는 HUD 가 만들어진 다음 프레임에 무조건 열었는데, 그 시점은
     * 아직 스테이지 에셋을 비동기로 로딩하는 중이다. 로딩 화면과 겹치고,
     * 인트로가 게임을 일시정지시키므로 로딩이 멈출 수도 있다.
     *
     * GameState 가 Playing 으로 바뀌는 순간은 로딩 화면이 이미 사라진
     * 뒤이고 게임은 아직 아무 일도 일어나지 않은 시점이다.
     */
    UFUNCTION()
    void HandleStageStateChanged(ESFStageState NewState);

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

    UFUNCTION()
    void HandleRallyPointRequested();

    bool TryHandleUnitDoubleClick(
        AActor* ClickedActor
    );

    void SelectNearbyUnitsOfSameType(
        AUnitBase* CenterUnit
    );

    UFUNCTION()
    void HandleSelectedBuildingTierActivated(int32 ActivatedTier);

    void HideRallyPointIndicator();

    // ==================== Variables ====================

protected:
    UPROPERTY(
        Transient,
        BlueprintReadOnly,
        Category = "SF|Selection"
    )
    TObjectPtr<AActor> SelectedActor = nullptr;

    TWeakObjectPtr<AActor> LastClickedUnit;

    float LastUnitClickTime = -1.0f;

    UPROPERTY(EditDefaultsOnly, Category = "SF|Selection|Double Click")
    float DoubleClickInterval = 0.3f;

    UPROPERTY(EditDefaultsOnly, Category = "SF|Selection|Double Click")
    float SameUnitSearchRadius = 1000.0f;


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
        Category = "SF|HUD|Resource",
        meta = (BindWidgetOptional)
    )
    TObjectPtr<UWidget> ResourcePanel = nullptr;

    UPROPERTY(
        BlueprintReadOnly,
        Category = "SF|HUD|Minimap",
        meta = (BindWidgetOptional)
    )
    TObjectPtr<UWidget> MinimapPanel = nullptr;

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
    TObjectPtr<UMaterialInterface> AttackDestinationIndicatorMaterial = nullptr;

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

    UPROPERTY(EditDefaultsOnly,BlueprintReadOnly,Category = "SF|HUD|Sound")
    TObjectPtr<USoundBase> BuildModeOpenSound = nullptr;

    UPROPERTY(EditDefaultsOnly,BlueprintReadOnly,Category = "SF|HUD|Sound")
    TObjectPtr<USoundBase> BuildModeCloseSound = nullptr;

    UPROPERTY(EditDefaultsOnly,BlueprintReadOnly,Category = "SF|HUD|Sound")
    TObjectPtr<USoundBase> BuildStartSound = nullptr;

    UPROPERTY(EditDefaultsOnly,BlueprintReadOnly,Category = "SF|HUD|Sound")
    TObjectPtr<USoundBase> ResourceWarningSound = nullptr;

    UPROPERTY(EditDefaultsOnly,BlueprintReadOnly,Category = "SF|Command|Feedback")
    TObjectPtr<UMaterialInterface> RallyPointIndicatorMaterial = nullptr;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "SF|HUD|Rally")
    TObjectPtr<UMaterialInterface> RallyPathIndicatorMaterial = nullptr;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "SF|HUD|Rally")
    float RallyPathIndicatorWidth = 30.f;

    UPROPERTY(Transient)
    TObjectPtr<UStaticMeshComponent> RallyPathIndicator = nullptr;

    UPROPERTY(EditDefaultsOnly,BlueprintReadOnly,Category = "SF|Rally")
    TObjectPtr<UStaticMesh> RallyIndicatorMesh = nullptr;

private:
    UPROPERTY(Transient)
    TObjectPtr<UCommonActivatableWidget> ActiveBuildingModeWidget = nullptr;

    // ── 스테이지 시작 대기 ──────────────────────────────

    /** OnStageStateChanged 를 구독한 대상. NativeDestruct 에서 떼어낸다 */
    UPROPERTY(Transient)
    TObjectPtr<ASFGameState> BoundStageGameState = nullptr;

    /**
     * 인트로를 이미 요청했는가.
     *
     * Playing 은 한 판에 한 번뿐이지만, 델리게이트가 두 번 오는 경로가
     * 생겨도 인트로 위젯이 두 개 열리지 않도록 막아둔다.
     */
    bool bStageIntroRequested = false;

    /**
     * 로딩이 끝날 때까지 HUD 를 숨길지.
     *
     * 로딩 화면이 불투명하면 꺼도 티가 안 나지만, 배경 영상에 투명도가
     * 있거나 페이드를 넣으면 그 아래로 HUD 가 비친다.
     *
     * 문제가 생기면 이 값 하나로 예전 동작으로 되돌릴 수 있다.
     */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "SF|HUD|Stage Intro",
        meta = (AllowPrivateAccess = "true"))
    bool bHideHUDUntilStagePlaying = true;

    /**
     * 숨기기 전의 가시성.
     *
     * SelfHitTestInvisible 로 고정해서 되돌리면 WBP 가 설정한 값과
     * 달라져 클릭 판정이 바뀐다. 원래 값을 그대로 돌려준다.
     */
    ESlateVisibility VisibilityBeforeStageLoading = ESlateVisibility::SelfHitTestInvisible;

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

    UPROPERTY(Transient)
    TWeakObjectPtr<ASFBaseBuilding>
        SelectedBuildingForTierDelegate;

    UPROPERTY(Transient)
    TObjectPtr<UStaticMeshComponent> RallyPointIndicator = nullptr;
};