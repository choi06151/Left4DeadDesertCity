#pragma once

#include "CoreMinimal.h"
#include "Navigation/CrowdFollowingComponent.h"
#include "TimerManager.h"
#include "ZombieSmartNavLinkProxy.h"
#include "ZombieCrowdFollowingComponent.generated.h"

class INavLinkCustomInterface;

UCLASS()
class LEFT4DEAD_DESERTCITY_API UZombieCrowdFollowingComponent : public UCrowdFollowingComponent
{
	GENERATED_BODY()

public:
	virtual void StartUsingCustomLink(INavLinkCustomInterface* CustomNavLink, const FVector& DestPoint) override;
	virtual void FinishUsingCustomLink(INavLinkCustomInterface* CustomNavLink) override;

	void FinishActiveCustomLink();
	void SetClimbMinHeightDelta(float InClimbMinHeightDelta) { ClimbMinHeightDelta = InClimbMinHeightDelta; }

private:
	void NotifyPendingQualityNavLink();
	void NotifyQualityNavLink(const FZombieNavLinkContext& NavLinkContext);
	EZombieSmartNavLinkType ResolveLinkType(const FVector& StartLocation, const FVector& EndLocation) const;
	FZombieNavLinkContext BuildNavLinkContext(const FVector& StartLocation, const FVector& EndLocation) const;

	INavLinkCustomInterface* ActiveCustomNavLink = nullptr;
	INavLinkCustomInterface* PendingQualityNavLink = nullptr;
	FZombieNavLinkContext PendingQualityNavLinkContext;
	FTimerHandle PendingQualityNavLinkTimerHandle;
	float ClimbMinHeightDelta = 60.0f;

	UPROPERTY(EditAnywhere, Category = "Zombie|NavLink", meta = (ClampMin = "0.0"))
	float QualityNavLinkActivationDelay = 1.0f;
};
