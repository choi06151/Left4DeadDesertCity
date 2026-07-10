#include "ZombieCrowdFollowingComponent.h"

#include "NavLinkCustomInterface.h"
#include "ZombieAIController.h"
#include "ZombieCharacter.h"

void UZombieCrowdFollowingComponent::StartUsingCustomLink(INavLinkCustomInterface* CustomNavLink, const FVector& DestPoint)
{
	ActiveCustomNavLink = CustomNavLink;

	Super::StartUsingCustomLink(CustomNavLink, DestPoint);

	FVector StartLocation = FVector::ZeroVector;

	if (const AZombieAIController* ZombieAI = Cast<AZombieAIController>(GetOwner()))
	{
		if (const APawn* ZombiePawn = ZombieAI->GetPawn())
		{
			StartLocation = ZombiePawn->GetActorLocation();
		}
	}

	NotifyQualityNavLink(BuildNavLinkContext(StartLocation, DestPoint));
}

void UZombieCrowdFollowingComponent::FinishUsingCustomLink(INavLinkCustomInterface* CustomNavLink)
{
	Super::FinishUsingCustomLink(CustomNavLink);

	if (ActiveCustomNavLink == CustomNavLink)
	{
		ActiveCustomNavLink = nullptr;
	}
}

void UZombieCrowdFollowingComponent::FinishActiveCustomLink()
{
	if (ActiveCustomNavLink)
	{
		FinishUsingCustomLink(ActiveCustomNavLink);
	}
}

EZombieSmartNavLinkType UZombieCrowdFollowingComponent::ResolveLinkType(
	const FVector& StartLocation,
	const FVector& EndLocation) const
{
	const float HeightDelta = EndLocation.Z - StartLocation.Z;

	if (HeightDelta >= ClimbMinHeightDelta)
	{
		return EZombieSmartNavLinkType::Climb;
	}

	return EZombieSmartNavLinkType::Jump;
}

FZombieNavLinkContext UZombieCrowdFollowingComponent::BuildNavLinkContext(
	const FVector& StartLocation,
	const FVector& EndLocation) const
{
	FZombieNavLinkContext NavLinkContext;
	NavLinkContext.StartLocation = StartLocation;
	NavLinkContext.EndLocation = EndLocation;
	NavLinkContext.MidLocation = (StartLocation + EndLocation) * 0.5f;

	FVector HorizontalDirection = EndLocation - StartLocation;
	HorizontalDirection.Z = 0.0f;
	NavLinkContext.HorizontalDistance = HorizontalDirection.Size();
	NavLinkContext.MoveDirection = HorizontalDirection.GetSafeNormal();
	NavLinkContext.HeightDelta = EndLocation.Z - StartLocation.Z;
	NavLinkContext.LinkType = ResolveLinkType(StartLocation, EndLocation);

	return NavLinkContext;
}

void UZombieCrowdFollowingComponent::NotifyQualityNavLink(const FZombieNavLinkContext& NavLinkContext)
{
	AZombieAIController* ZombieAI = Cast<AZombieAIController>(GetOwner());
	if (!ZombieAI || ZombieAI->GetCurrentMode() != EZombieAIMode::Quality)
	{
		return;
	}

	if (AZombieCharacter* ZombieChar = Cast<AZombieCharacter>(ZombieAI->GetPawn()))
	{
		UE_LOG(LogTemp, Log, TEXT("Quality SmartLink triggered: %s -> %s"),
			*NavLinkContext.StartLocation.ToString(),
			*NavLinkContext.EndLocation.ToString());
		ZombieChar->HandleQualityNavLink(NavLinkContext);
	}
}
