#include "ProductComponent.h"
#include "Net/UnrealNetwork.h"

void UProductComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
    Super::GetLifetimeReplicatedProps(OutLifetimeProps);
    DOREPLIFETIME(UProductComponent,ProductLine);DOREPLIFETIME(UProductComponent,NetworkRatio);
    DOREPLIFETIME(UProductComponent,AssemblePoint);DOREPLIFETIME(UProductComponent,bSetAssemblePoint);
}
void UProductComponent::PreReplication(IRepChangedPropertyTracker& ChangedPropertyTracker)
{
    Super::PreReplication(ChangedPropertyTracker);NetworkRatio=Tracker.GetRatio();
}
float UProductComponent::GetCurrentProductRatio() const
{return GetOwner()&&!GetOwner()->HasAuthority()?NetworkRatio:Tracker.GetRatio();}
void UProductComponent::OnRep_NetworkQueue(){OnProductLineChanged.Broadcast(ProductLine);}
void UProductComponent::OnRep_NetworkRatio(){OnProductRatioChanged.Broadcast(NetworkRatio);}
