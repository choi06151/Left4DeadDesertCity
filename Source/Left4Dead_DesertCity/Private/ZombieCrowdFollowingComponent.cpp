// Fill out your copyright notice in the Description page of Project Settings.

#include "ZombieCrowdFollowingComponent.h"
#include "ZombieAIController.h"
#include "ZombieCharacter.h"
#include "NavMesh/NavMeshPath.h"
#include "DrawDebugHelpers.h"

void UZombieCrowdFollowingComponent::SetMoveSegment(int32 SegmentStartIndex)
{

	UE_LOG(LogTemp, Warning, TEXT("Segment : %d"), SegmentStartIndex);

	const TArray<FNavPathPoint>& Points = Path->GetPathPoints();

	UE_LOG(LogTemp, Warning, TEXT("Num : %d"), Points.Num());

	UE_LOG(LogTemp, Warning, TEXT("IsNavLink : %d"),
		FNavMeshNodeFlags(Points[SegmentStartIndex].Flags).IsNavLink());
	
	Super::SetMoveSegment(SegmentStartIndex);

	if (Path.IsValid())
	{
		const TArray<FNavPathPoint>& PathPoints = Path->GetPathPoints();

		for (int32 i = 0; i < PathPoints.Num() - 1; i++)
		{
			bool bIsNavLink =
				FNavMeshNodeFlags(PathPoints[i].Flags).IsNavLink();

			DrawDebugLine(
				GetWorld(),
				PathPoints[i].Location,
				PathPoints[i + 1].Location,
				bIsNavLink ? FColor::Yellow : FColor::Green,
				false,
				5.f,
				0,
				8.f
			);
		}
		
		for (int32 i = 0; i < PathPoints.Num() - 1; i++)
		{
			DrawDebugLine(
				GetWorld(),
				PathPoints[i].Location,
				PathPoints[i + 1].Location,
				FColor::Green,
				false,      // Persistent
				2.f,        // Duration
				0,
				5.f         // Thickness
			);

			DrawDebugSphere(
				GetWorld(),
				PathPoints[i].Location,
				20.f,
				12,
				FColor::Red,
				false,
				2.f
			);
		}

		// 마지막 점도 표시
		DrawDebugSphere(
			GetWorld(),
			PathPoints.Last().Location,
			20.f,
			12,
			FColor::Blue,
			false,
			2.f
		);
	}
	
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

	Super::StartUsingCustomLink(CustomNavLink, DestPoint);

	
	const FVector StartLocation = MovementComp ? MovementComp->GetActorFeetLocation() : FVector::ZeroVector;
	NotifyQualityNavLink(StartLocation, DestPoint);

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
