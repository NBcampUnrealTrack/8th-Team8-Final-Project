#pragma once

#include "CoreMinimal.h"
#include "Engine/GameInstance.h"
#include "SFGameInstance.generated.h"

UENUM(BlueprintType)
enum class EDataTableType : uint8
{
	UI,
	Building,
	Unit
};

UCLASS()
class STRIKEFORGE_API USFGameInstance : public UGameInstance
{
	GENERATED_BODY()
public:
	UDataTable* GetDataTableType(EDataTableType TableType) const;
private:
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DataTable", meta = (AllowPrivateAccess = true))
	TObjectPtr<UDataTable> BuildingDataTable;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DataTable", meta = (AllowPrivateAccess = true))
	TObjectPtr<UDataTable> UIDataTable;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DataTable", meta = (AllowPrivateAccess = true))
	TObjectPtr<UDataTable> UnitsDataTable;
};
