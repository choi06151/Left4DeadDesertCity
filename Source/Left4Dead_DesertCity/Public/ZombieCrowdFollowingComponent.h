// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Navigation/CrowdFollowingComponent.h"
#include "ZombieCrowdFollowingComponent.generated.h"

UCLASS()
class LEFT4DEAD_DESERTCITY_API UZombieCrowdFollowingComponent : public UCrowdFollowingComponent
{
	GENERATED_BODY()

public:
	virtual void SetMoveSegment(int32 SegmentStartIndex) override;
	virtual void StartUsingCustomLink(INavLinkCustomInterface* CustomNavLink, const FVector& DestPoint) override;

private:
	void NotifyQualityNavLink(const FVector& StartLocation, const FVector& EndLocation);
};
