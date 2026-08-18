#pragma once

#include "CoreMinimal.h"
#include "Buildings/Resource/ResourceBase.h"
#include "Ether.generated.h"

UCLASS()
class STRIKEFORGE_API AEther : public AResourceBase
{
	GENERATED_BODY()
public:
	AEther();
	// 나중에 채굴량만 반환하게 하고 minebuilding이 처리하게 하는게 좋은구조로 발전할가능성잇음
	virtual bool Digging(const int32 InDigAmount) override;
protected:
	virtual void BeginPlay() override;
};
