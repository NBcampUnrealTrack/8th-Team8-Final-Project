#pragma once
#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Styling/SlateTypes.h"
#include "SFPVPLobbyWidget.generated.h"

UCLASS()
class STRIKEFORGE_API USFPVPLobbyWidget : public UUserWidget
{
    GENERATED_BODY()
public:
    USFPVPLobbyWidget(const FObjectInitializer& Initializer);
    virtual TSharedRef<SWidget> RebuildWidget() override;
    virtual void ReleaseSlateResources(bool bReleaseChildren) override;
    virtual void NativeTick(const FGeometry& Geometry, float DeltaTime) override;
private:
    class USFPVPSessionSubsystem* Sessions() const;
    bool InRoom() const;
    TSharedRef<SWidget> Label(const FString& Text, int32 Size, FLinearColor Color) const;
    TSharedRef<SWidget> PlayerCard(int32 Seat);
    void RefreshRooms();
    UPROPERTY() TObjectPtr<class UFont> Font;
    UPROPERTY() TObjectPtr<class UTexture2D> EtherPortrait;
    UPROPERTY() TObjectPtr<UTexture2D> BeastPortrait;
    UPROPERTY() TObjectPtr<UTexture2D> CampArt;
    UPROPERTY() TObjectPtr<UTexture2D> Wordmark;
    FSlateBrush EtherBrush, BeastBrush, PanelBrush, InsetBrush, CardBrush, CampBrush, LogoBrush;
    FButtonStyle ButtonStyle, PrimaryButtonStyle, TabButtonStyle, SmallButtonStyle;
    FEditableTextBoxStyle InputStyle;
    TSharedPtr<class SSFPVPAmbience> Ambience;
    TSharedPtr<class SEditableTextBox> RoomInput, AddressInput;
    TSharedPtr<class SVerticalBox> RoomList;
    FString RoomSignature;
    FString PlayerSignature;
    TSet<uint8> WarmedRaces;
};
