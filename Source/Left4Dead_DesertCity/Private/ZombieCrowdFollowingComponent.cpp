// Fill out your copyright notice in the Description page of Project Settings.

#include "ZombieCrowdFollowingComponent.h"
#include "ZombieAIController.h"
#include "ZombieCharacter.h"
#include "NavMesh/NavMeshPath.h"

void UZombieCrowdFollowingComponent::SetMoveSegment(int32 SegmentStartIndex)
{
	Super::SetMoveSegment(SegmentStartIndex);

	if (!Path.IsValid())
	{
		return;
	}

	const TArray<FNavPathPoint>& PathPoints = Path->GetPathPoints();
	if (!PathPoints.IsValidIndex(SegmentStartIndex) || !PathPoints.IsValidIndex(SegmentStartIndex + 1))
	{
		return;
	}

	if (!FNavMeshNodeFlags(PathPoints[SegmentStartIndex].Flags).IsNavLink())
	{
		return;
	}

	NotifyQualityNavLink(PathPoints[SegmentStartIndex].Location, PathPoints[SegmentStartIndex + 1].Location);
}

void UZombieCrowdFollowingComponent::StartUsingCustomLink(INavLinkCustomInterface* CustomNavLink, const FVector& DestPoint)
{
	const FVector StartLocation = MovementComp ? MovementComp->GetActorFeetLocation() : FVector::ZeroVector;
	NotifyQualityNavLink(StartLocation, DestPoint);

	Super::StartUsingCustomLink(CustomNavLink, DestPoint);
}

void UZombieCrowdFollowingComponent::NotifyQualityNavLink(const FVector& StartLocation, const FVector& EndLocation)
{
	AZombieAIController* ZombieAI = Cast<AZombieAIController>(GetOwner());
	if (!ZombieAI || ZombieAI->GetCurrentMode() != EZombieAIMode::Quality)
	{
		return;
	}

	if (AZombieCharacter* ZombieChar = Cast<AZombieCharacter>(ZombieAI->GetPawn()))
	{
		UE_LOG(LogTemp, Log, TEXT("Quality NavLink triggered: %s -> %s"), *StartLocation.ToString(), *EndLocation.ToString());
		ZombieChar->OnQualityMoveNavLink(StartLocation, EndLocation);
	}
}
