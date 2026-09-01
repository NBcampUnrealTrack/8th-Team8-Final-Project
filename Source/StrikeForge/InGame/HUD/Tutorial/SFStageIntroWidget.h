#pragma once

#include "CoreMinimal.h"
#include "CommonActivatableWidget.h"
// FUIInputConfig / ECommonInputMode 는 CommonActivatableWidget.h 가 끌어온다.
// 예전에 여기 있던 "Input/UIActionRouterTypes.h" 는 CommonUI 의 Private 헤더라
// 모듈 밖에서 열 수 없다 (C1083). 공개 위치는 "Input/UIActionBindingHandle.h" 인데,
// 엔진 버전에 따라 경로가 바뀐 적이 있어 부모 헤더에 맡긴다.
#include "Containers/Ticker.h"
#include "SFStageIntroData.h"
#include "SFStageIntroWidget.generated.h"

class UButton;
class UDataTable;
class UImage;
class UTextBlock;
class UTexture2D;
class USoundBase;
class UWidgetSwitcher;

UCLASS(Abstract, Blueprintable)
class STRIKEFORGE_API USFStageIntroWidget : public UCommonActivatableWidget
{
    GENERATED_BODY()

public:
    /** 행에 지정된 StartNodeIndex 부터 시작한다 */
    UFUNCTION(BlueprintCallable, Category = "SF|Stage Intro")
    bool StartStageIntro(UDataTable* InDataTable, FName InRowName);

    /**
     * 지정한 노드부터 시작한다. 목표 대기 후 이어서 열 때 쓴다.
     *
     * USFTutorialSubsystem::OnResumeRequested 가 넘겨주는 세 값을 그대로
     * 여기 넣으면 멈췄던 자리에서 이어진다.
     */
    UFUNCTION(BlueprintCallable, Category = "SF|Stage Intro")
    bool StartStageIntroAt(UDataTable* InDataTable, FName InRowName, int32 InNodeIndex);

protected:
    /**
     * CommonUI 는 활성 위젯이 요청한 입력 설정을 뷰포트에 적용한다.
     * 아무도 요청하지 않으면 기본값(FUIInputConfig 기본 생성자)으로 되돌리는데,
     * 그 기본값이 CapturePermanently + 캡처 중 커서 숨김이라 RTS 조작이 막힌다.
     * 그래서 이 위젯이 쓸 설정을 명시한다.
     */
    virtual TOptional<FUIInputConfig> GetDesiredInputConfig() const override;

    virtual void NativeConstruct() override;
    virtual void NativeDestruct() override;
    virtual FReply NativeOnMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;

private:
    void ShowNode(int32 NodeIndex);
    void ShowDialogueNode(const FSFStageIntroNode& Node);
    void ShowChoiceNode(const FSFStageIntroNode& Node);
    void ShowControlGuideNode(const FSFStageIntroNode& Node);
    void ShowWaitForObjectiveNode(const FSFStageIntroNode& Node);
    void ShowInteractiveGuideNode(const FSFStageIntroNode& Node);
    void UpdateCharacterImages(const FSFStageIntroNode& Node);
    void SetImageTexture(UImage* TargetImage, const TSoftObjectPtr<UTexture2D>& Texture);
    void UpdateGuidePageText();
    void FinishStageIntro();

    /** 대기·안내로 넘어갈 때 진행 상태를 비우고 위젯을 닫는다 */
    void SuspendStageIntro();

    // ── 타자 효과 ───────────────────────────────────────

    /**
     * 글자를 하나씩 드러내기 시작한다.
     *
     * 효과가 꺼져 있거나 노드가 건너뛰기로 표시돼 있으면 전문을 즉시 넣는다.
     */
    void BeginTypewriter(UTextBlock* InTarget, const FText& InText, const FSFStageIntroNode& InNode);

    /** 한 글자(대리 문자쌍이면 두 칸)를 드러내고 다음 간격을 정한다 */
    void RevealNextCharacter();

    /** 남은 글자를 전부 드러내고 티커를 뗀다 */
    void CompleteTypewriter();

    /**
     * 엔진 티커가 매 프레임 부른다. false 를 돌려주면 티커가 떨어진다.
     *
     * 월드 타이머가 아니라 엔진 티커인 이유가 중요하다.
     * 튜토리얼이 뜨는 동안 게임은 일시정지 상태인데, 월드 타이머는 일시정지
     * 중에 전혀 진행되지 않는다. 타이머로 만들면 글자가 한 자도 안 나오고
     * 멈춰 있다가 클릭하는 순간 전문이 튀어나온다.
     *
     * UUserWidget 의 NativeTick 도 답이 아니다. TickFrequency 기본값(Auto)은
     * 블루프린트 Tick 이 없으면 틱을 아예 안 돌린다.
     */
    bool HandleTypewriterTicker(float InDeltaTime);

    /** 티커를 떼어낸다. 여러 곳에서 불리므로 한 곳에 모은다 */
    void StopTypewriterTicker();

    bool IsTypewriterRunning() const;

    const FSFStageIntroNode* GetCurrentNode() const;

    UFUNCTION()
    void HandleYesClicked();

    UFUNCTION()
    void HandleNoClicked();

    UFUNCTION()
    void HandlePreviousGuideClicked();

    UFUNCTION()
    void HandleNextGuideClicked();

protected:
    UPROPERTY(BlueprintReadOnly, Category = "SF|Stage Intro", meta = (BindWidget))
    TObjectPtr<UWidgetSwitcher> StageIntroSwitcher = nullptr;

    UPROPERTY(BlueprintReadOnly, Category = "SF|Stage Intro", meta = (BindWidget))
    TObjectPtr<UImage> LeftCharacterImage = nullptr;

    UPROPERTY(BlueprintReadOnly, Category = "SF|Stage Intro", meta = (BindWidget))
    TObjectPtr<UImage> RightCharacterImage = nullptr;

    UPROPERTY(BlueprintReadOnly, Category = "SF|Stage Intro", meta = (BindWidget))
    TObjectPtr<UTextBlock> SpeakerNameText = nullptr;

    UPROPERTY(BlueprintReadOnly, Category = "SF|Stage Intro", meta = (BindWidget))
    TObjectPtr<UTextBlock> DialogueText = nullptr;

    UPROPERTY(BlueprintReadOnly, Category = "SF|Stage Intro", meta = (BindWidget))
    TObjectPtr<UImage> NextIndicatorImage = nullptr;

    UPROPERTY(BlueprintReadOnly, Category = "SF|Stage Intro", meta = (BindWidget))
    TObjectPtr<UTextBlock> ChoiceQuestionText = nullptr;

    UPROPERTY(BlueprintReadOnly, Category = "SF|Stage Intro", meta = (BindWidget))
    TObjectPtr<UButton> YesButton = nullptr;

    UPROPERTY(BlueprintReadOnly, Category = "SF|Stage Intro", meta = (BindWidget))
    TObjectPtr<UTextBlock> YesButtonText = nullptr;

    UPROPERTY(BlueprintReadOnly, Category = "SF|Stage Intro", meta = (BindWidget))
    TObjectPtr<UButton> NoButton = nullptr;

    UPROPERTY(BlueprintReadOnly, Category = "SF|Stage Intro", meta = (BindWidget))
    TObjectPtr<UTextBlock> NoButtonText = nullptr;

    UPROPERTY(BlueprintReadOnly, Category = "SF|Stage Intro", meta = (BindWidget))
    TObjectPtr<UTextBlock> GuideTitleText = nullptr;

    UPROPERTY(BlueprintReadOnly, Category = "SF|Stage Intro", meta = (BindWidget))
    TObjectPtr<UImage> GuideImage = nullptr;

    UPROPERTY(BlueprintReadOnly, Category = "SF|Stage Intro", meta = (BindWidget))
    TObjectPtr<UTextBlock> GuideDescriptionText = nullptr;

    UPROPERTY(BlueprintReadOnly, Category = "SF|Stage Intro", meta = (BindWidget))
    TObjectPtr<UTextBlock> GuidePageText = nullptr;

    UPROPERTY(BlueprintReadOnly, Category = "SF|Stage Intro", meta = (BindWidget))
    TObjectPtr<UButton> PreviousGuideButton = nullptr;

    UPROPERTY(BlueprintReadOnly, Category = "SF|Stage Intro", meta = (BindWidget))
    TObjectPtr<UButton> NextGuideButton = nullptr;

    // ── 타자 효과 설정 ──────────────────────────────────

    /**
     * 타자 효과를 쓸지. 끄면 예전처럼 전문이 한 번에 나온다.
     *
     * 연출이 답답하다는 의견이 나오면 이 값 하나로 되돌릴 수 있어야 한다.
     */
    UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "SF|Stage Intro|Typewriter")
    bool bUseTypewriter = true;

    /**
     * 초당 글자 수.
     *
     * 한글은 한 글자에 담기는 정보가 많아 라틴 문자보다 느려도 읽힌다.
     * 30 언저리가 "읽어주는 속도" 로 자연스럽다.
     */
    UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "SF|Stage Intro|Typewriter",
        meta = (ClampMin = "1.0"))
    float CharactersPerSecond = 30.f;

    /**
     * 문장부호 뒤에 추가로 쉬는 시간(초).
     *
     * 균일한 속도로 흘리면 기계가 뱉는 느낌이 난다. 쉼표와 마침표에서
     * 살짝 쉬는 것만으로 사람이 읽어주는 것처럼 들린다.
     */
    UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "SF|Stage Intro|Typewriter",
        meta = (ClampMin = "0.0"))
    float PunctuationPauseSeconds = 0.16f;

    /**
     * 타자가 도는 동안 NextIndicatorImage 를 숨길지.
     *
     * 기본값이 꺼짐인 이유가 있다. 이 이름의 위젯에 화살표가 아니라 대사창
     * 배경 전체가 들어가 있는 경우가 있는데, 그러면 이 값을 켜는 순간 글자를
     * 찍는 동안 대사창이 통째로 사라진다.
     *
     * 진짜 "다음" 화살표를 따로 두었을 때만 켜라.
     */
    UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "SF|Stage Intro|Typewriter")
    bool bHideNextIndicatorWhileTyping = false;

    /** 글자가 나올 때 낼 소리. 비워두면 무음 */
    UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "SF|Stage Intro|Typewriter")
    TObjectPtr<USoundBase> TypeSound = nullptr;

    /**
     * 소리를 몇 글자마다 낼지.
     *
     * 매 글자마다 내면 소리가 겹쳐 뭉개진다.
     */
    UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "SF|Stage Intro|Typewriter",
        meta = (ClampMin = "1"))
    int32 SoundEveryNCharacters = 3;

private:
    UPROPERTY(Transient)
    TArray<FSFStageIntroNode> ActiveNodes;

    /**
     * 지금 재생 중인 테이블과 행.
     *
     * 목표 대기에 들어갈 때 "어디로 돌아와야 하는지" 를 서브시스템에 넘겨야
     * 하는데, 노드 배열만으로는 그 출처를 알 수가 없어서 함께 들고 있는다.
     */
    UPROPERTY(Transient)
    TObjectPtr<UDataTable> ActiveDataTable = nullptr;

    FName ActiveRowName = NAME_None;

    int32 CurrentNodeIndex = INDEX_NONE;

    // ── 타자 효과 상태 ──────────────────────────────────

    /**
     * 지금 글자를 채워 넣고 있는 텍스트 위젯.
     *
     * 대사와 조작 설명이 서로 다른 위젯을 쓰므로 대상을 들고 있어야 한다.
     */
    UPROPERTY(Transient)
    TObjectPtr<UTextBlock> TypingTarget = nullptr;

    /** 다 드러났을 때의 전문 */
    FString FullTypingText;

    /** 지금까지 드러낸 글자 수(FString 인덱스) */
    int32 RevealedCharCount = 0;

    /** 이 노드에 적용 중인 한 글자 간격(초) */
    float ActiveTypeInterval = 0.03f;

    /** 다음 글자까지 남은 시간을 재는 누적값 */
    float TypeAccumulator = 0.f;

    /**
     * 다음 한 글자를 내보내기까지 기다릴 시간.
     *
     * 보통은 ActiveTypeInterval 과 같고, 문장부호 뒤에서만 길어진다.
     */
    float NextCharacterDelay = 0.f;

    FTSTicker::FDelegateHandle TypewriterTickerHandle;

    static constexpr int32 DialoguePanelIndex = 0;
    static constexpr int32 ChoicePanelIndex = 1;
    static constexpr int32 ControlGuidePanelIndex = 2;
};