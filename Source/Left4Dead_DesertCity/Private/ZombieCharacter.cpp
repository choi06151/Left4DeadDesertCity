#include "ZombieCharacter.h"

#include "Components/CapsuleComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Kismet/GameplayStatics.h"
#include "NavigationSystem.h"
#include "TimerManager.h"
#include "ZombieAIController.h"
#include "ZombieCrowdFollowingComponent.h"
#include "ZombieManager.h"

AZombieCharacter::AZombieCharacter()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.SetTickFunctionEnable(false);
	StatsRowHandle.RowName = TEXT("Default");
}

void AZombieCharacter::BeginPlay()
{
	Super::BeginPlay();

	CachedZombieAI = Cast<AZombieAIController>(GetController());
	LoadStatsFromDataTable();
	SetActorTickEnabled(false);
}

void AZombieCharacter::ApplyStatsRow(const FZombieStatsRow& StatsRow)
{
	speed = StatsRow.MoveSpeed;
	timeToDisappear = StatsRow.TimeToDisappear;
	QualityEnterDistance = StatsRow.QualityEnterDistance;
	QualityExitDistance = StatsRow.QualityExitDistance;
	QualityNearDistance = StatsRow.QualityNearDistance;
	QualityMidDistance = StatsRow.QualityMidDistance;
	QualityNearRepathInterval = StatsRow.QualityNearRepathInterval;
	QualityMidRepathInterval = StatsRow.QualityMidRepathInterval;
	QualityFarRepathInterval = StatsRow.QualityFarRepathInterval;
	QualityNearRepathDistance = StatsRow.QualityNearRepathDistance;
	QualityMidRepathDistance = StatsRow.QualityMidRepathDistance;
	QualityFarRepathDistance = StatsRow.QualityFarRepathDistance;
	SimpleFailureCountForQuality = StatsRow.SimpleFailureCountForQuality;
	TemporaryQualityDuration = StatsRow.TemporaryQualityDuration;
	QualityFailureRetryDelay = StatsRow.QualityFailureRetryDelay;
	QualityStuckDistanceThreshold = StatsRow.QualityStuckDistanceThreshold;
	QualityStuckSpeedThreshold = StatsRow.QualityStuckSpeedThreshold;
	QualityStuckDuration = StatsRow.QualityStuckDuration;
	QualityStuckLaunchForwardStrength = StatsRow.QualityStuckLaunchForwardStrength;
	QualityStuckLaunchUpStrength = StatsRow.QualityStuckLaunchUpStrength;
	MaxRepeatedNavLinkCount = StatsRow.MaxRepeatedNavLinkCount;
	RepeatedNavLinkLocationTolerance = StatsRow.RepeatedNavLinkLocationTolerance;

	if (UCharacterMovementComponent* MoveComp = GetCharacterMovement())
	{
		MoveComp->MaxWalkSpeed = speed;
	}

	if (!CachedZombieAI)
	{
		CachedZombieAI = Cast<AZombieAIController>(GetController());
	}

	if (CachedZombieAI)
	{
		CachedZombieAI->ApplyStatsRow(StatsRow);

		if (UZombieCrowdFollowingComponent* CrowdFollowComp =
			Cast<UZombieCrowdFollowingComponent>(CachedZombieAI->GetPathFollowingComponent()))
		{
			CrowdFollowComp->SetClimbMinHeightDelta(StatsRow.ClimbMinHeightDelta);
		}
	}
}

void AZombieCharacter::LoadStatsFromDataTable()
{
	if (!StatsRowHandle.DataTable)
	{
		return;
	}

	if (const FZombieStatsRow* StatsRow = StatsRowHandle.GetRow<FZombieStatsRow>(TEXT("ZombieCharacter::LoadStatsFromDataTable")))
	{
		ApplyStatsRow(*StatsRow);
	}
}

void AZombieCharacter::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	UpdateQualityChase(DeltaTime);
}

void AZombieCharacter::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
	Super::SetupPlayerInputComponent(PlayerInputComponent);
}

void AZombieCharacter::SimpleMove(FVector TargetLocation)
{
	if (!CachedZombieAI)
	{
		CachedZombieAI = Cast<AZombieAIController>(GetController());
	}

	if (!CachedZombieAI)
	{
		return;
	}

	QualityTargetActor = nullptr;
	LastSimpleTargetLocation = TargetLocation;
	LastSimpleObservedLocation = GetActorLocation();
	ConsecutiveSimpleStuckCount = 0;
	SimpleStuckRecoveryAttemptCount = 0;
	LastSimpleMoveRequestTime = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0f;
	ForcedQualityUntilTime = 0.0f;
	bIsUsingQualityNavLink = false;
	bIgnoreNextQualityMoveFinished = false;
	ResetQualityStuckTracking();
	ResetRepeatedNavLinkTracking();
	SetActorTickEnabled(false);
	CachedZombieAI->SetAIMode(EZombieAIMode::Simple);
	CachedZombieAI->SetPerceptionEnabled(true);
	CachedZombieAI->MoveToLocation(
		TargetLocation,
		30.0f,
		true,
		true,
		false,
		true
	);
	StartSimpleStuckCheck();
}

void AZombieCharacter::QualityMove(AActor* TargetActor)
{
	if (!TargetActor)
	{
		return;
	}

	if (!CachedZombieAI)
	{
		CachedZombieAI = Cast<AZombieAIController>(GetController());
	}

	if (!CachedZombieAI)
	{
		return;
	}

	QualityTargetActor = TargetActor;
	SetActorTickEnabled(true);
	StopSimpleStuckCheck();
	bIsUsingQualityNavLink = false;
	bIgnoreNextQualityMoveFinished = false;
	CachedZombieAI->SetAIMode(EZombieAIMode::Quality);
	CachedZombieAI->SetPerceptionEnabled(false);
	ResetQualityStuckTracking();
	ResetRepeatedNavLinkTracking();
	LastQualityObservedLocation = GetActorLocation();
	RequestQualityRepath(true);
}

void AZombieCharacter::FinishQualityMoveNavLink()
{
	if (!CachedZombieAI)
	{
		CachedZombieAI = Cast<AZombieAIController>(GetController());
	}

	if (!CachedZombieAI)
	{
		return;
	}

	if (UZombieCrowdFollowingComponent* CrowdFollowComp =
		Cast<UZombieCrowdFollowingComponent>(CachedZombieAI->GetPathFollowingComponent()))
	{
		CrowdFollowComp->FinishActiveCustomLink();
	}

	TryRecoverOffNavAfterNavLink();
	ResumeQualityChaseAfterNavLink();
}

void AZombieCharacter::HandleQualityNavLink(const FZombieNavLinkContext& NavLinkContext)
{
	const bool bIsSameLink =
		FVector::DistSquared(NavLinkContext.StartLocation, LastNavLinkStartLocation) <= FMath::Square(RepeatedNavLinkLocationTolerance) &&
		FVector::DistSquared(NavLinkContext.EndLocation, LastNavLinkEndLocation) <= FMath::Square(RepeatedNavLinkLocationTolerance);

	LastNavLinkContext = NavLinkContext;

	if (bIsSameLink)
	{
		++RepeatedNavLinkCount;
	}
	else
	{
		LastNavLinkStartLocation = NavLinkContext.StartLocation;
		LastNavLinkEndLocation = NavLinkContext.EndLocation;
		RepeatedNavLinkCount = 1;
	}

	if (RepeatedNavLinkCount >= MaxRepeatedNavLinkCount)
	{
		UE_LOG(LogTemp, Warning, TEXT("Zombie deactivated after repeated nav link loop: Count=%d Start=%s End=%s"),
			RepeatedNavLinkCount,
			*NavLinkContext.StartLocation.ToString(),
			*NavLinkContext.EndLocation.ToString());
		ZombieForceReturnToPool();
		return;
	}

	const bool bShouldPauseForNavLink = (NavLinkContext.LinkType == EZombieSmartNavLinkType::Climb);
	bIsUsingQualityNavLink = bShouldPauseForNavLink;

	if (!CachedZombieAI)
	{
		CachedZombieAI = Cast<AZombieAIController>(GetController());
	}

	if (CachedZombieAI && bShouldPauseForNavLink)
	{
		bIgnoreNextQualityMoveFinished = true;
		CachedZombieAI->StopMovement();
	}

	OnQualityMoveNavLink(NavLinkContext);
}

void AZombieCharacter::ZombieDeActivateSet()
{
	StopSimpleStuckCheck();
	DisableZombieActor();

	GetWorldTimerManager().SetTimer(
		DespawnTimerHandle,
		this,
		&AZombieCharacter::OnDespawnTimerExpired,
		timeToDisappear,
		false
	);
}

void AZombieCharacter::ZombieForceReturnToPool()
{
	GetWorldTimerManager().ClearTimer(DespawnTimerHandle);
	StopSimpleStuckCheck();
	DisableZombieActor();

	if (AZombieManager* Manager = Cast<AZombieManager>(GetOwner()))
	{
		Manager->DeSpawnZombie(this);
	}
}

void AZombieCharacter::LaunchTowardActor(AActor* TargetActor)
{
	if (!TargetActor)
	{
		return;
	}

	FVector LaunchDirection = TargetActor->GetActorLocation() - GetActorLocation();
	LaunchDirection.Z = 0.0f;
	LaunchDirection = LaunchDirection.GetSafeNormal();

	if (LaunchDirection.IsNearlyZero())
	{
		LaunchDirection = GetActorForwardVector().GetSafeNormal2D();
	}

	const FVector LaunchVelocity =
		(LaunchDirection * ActivationLaunchForwardStrength) +
		(FVector::UpVector * ActivationLaunchUpStrength);

	LaunchCharacter(LaunchVelocity, true, true);
}

void AZombieCharacter::OnSimpleMoveTargetReached()
{
	OnSimpleMoveFinished(EPathFollowingResult::Success);
}

void AZombieCharacter::OnSimpleMoveFinished(EPathFollowingResult::Type ResultCode)
{
	StopSimpleStuckCheck();

	if (CachedZombieAI)
	{
		CachedZombieAI->SetPerceptionEnabled(false);
	}

	if (ResultCode == EPathFollowingResult::Success)
	{
		ConsecutiveSimpleFailureCount = 0;
		SimpleStuckRecoveryAttemptCount = 0;
	}
	else
	{
		++ConsecutiveSimpleFailureCount;

		if (ConsecutiveSimpleFailureCount >= SimpleFailureCountForQuality)
		{
			TriggerTemporaryQualityFromSimpleBlock();
			return;
		}
	}

	if (OnMoveCompleted.IsBound())
	{
		TWeakObjectPtr<AZombieCharacter> WeakThis(this);
		GetWorldTimerManager().SetTimerForNextTick(FTimerDelegate::CreateLambda([WeakThis]()
		{
			if (!WeakThis.IsValid())
			{
				return;
			}

			if (WeakThis->OnMoveCompleted.IsBound())
			{
				WeakThis->OnMoveCompleted.Broadcast(WeakThis.Get());
			}
		}));
	}
}

void AZombieCharacter::UpdateQualityChase(float DeltaTime)
{
	if (!CachedZombieAI || CachedZombieAI->GetCurrentMode() != EZombieAIMode::Quality)
	{
		return;
	}

	if (bIsUsingQualityNavLink)
	{
		return;
	}

	AActor* TargetActor = QualityTargetActor.Get();
	if (!TargetActor)
	{
		SetActorTickEnabled(false);
		return;
	}

	const float DistanceToTarget = FVector::Dist(GetActorLocation(), TargetActor->GetActorLocation());

	if (ShouldSwitchToSimpleMode())
	{
		SimpleMove(TargetActor->GetActorLocation());
		return;
	}

	HandleQualityStuck(DeltaTime, DistanceToTarget);
	RequestQualityRepath(false);
}

void AZombieCharacter::RequestQualityRepath(bool bForceRepath)
{
	if (!CachedZombieAI)
	{
		return;
	}

	AActor* TargetActor = QualityTargetActor.Get();
	if (!TargetActor)
	{
		return;
	}

	const FVector CurrentGoalLocation = TargetActor->GetActorLocation();
	const float DistanceToTarget = FVector::Dist(GetActorLocation(), CurrentGoalLocation);
	const float CurrentTime = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0f;
	const float RepathDistance = GetQualityRepathDistanceForDistance(DistanceToTarget);
	const float RepathInterval = GetQualityRepathIntervalForDistance(DistanceToTarget);
	const bool bMovedEnough =
		FVector::DistSquared(CurrentGoalLocation, LastQualityGoalLocation) >= FMath::Square(RepathDistance);
	const bool bIntervalElapsed = (CurrentTime - LastQualityRepathTime) >= RepathInterval;

	if (!bForceRepath && !bMovedEnough && !bIntervalElapsed)
	{
		return;
	}

	LastQualityGoalLocation = CurrentGoalLocation;
	LastQualityRepathTime = CurrentTime;

	CachedZombieAI->MoveToActor(
		TargetActor,
		30.0f,
		true,
		true,
		false,
		nullptr,
		true
	);
}

bool AZombieCharacter::ShouldSwitchToSimpleMode() const
{
	const AActor* TargetActor = QualityTargetActor.Get();
	if (!TargetActor)
	{
		return false;
	}

	if (const UWorld* World = GetWorld())
	{
		if (ForcedQualityUntilTime > World->GetTimeSeconds())
		{
			return false;
		}
	}

	return FVector::DistSquared(GetActorLocation(), TargetActor->GetActorLocation()) >=
		FMath::Square(QualityExitDistance);
}

float AZombieCharacter::GetQualityRepathIntervalForDistance(float DistanceToTarget) const
{
	if (DistanceToTarget <= QualityNearDistance)
	{
		return QualityNearRepathInterval;
	}

	if (DistanceToTarget <= QualityMidDistance)
	{
		return QualityMidRepathInterval;
	}

	return QualityFarRepathInterval;
}

float AZombieCharacter::GetQualityRepathDistanceForDistance(float DistanceToTarget) const
{
	if (DistanceToTarget <= QualityNearDistance)
	{
		return QualityNearRepathDistance;
	}

	if (DistanceToTarget <= QualityMidDistance)
	{
		return QualityMidRepathDistance;
	}

	return QualityFarRepathDistance;
}

void AZombieCharacter::TriggerTemporaryQualityFromSimpleBlock()
{
	ConsecutiveSimpleFailureCount = 0;
	ConsecutiveSimpleStuckCount = 0;
	SimpleStuckRecoveryAttemptCount = 0;
	StopSimpleStuckCheck();

	if (!CachedZombieAI)
	{
		CachedZombieAI = Cast<AZombieAIController>(GetController());
	}

	AActor* PlayerActor = UGameplayStatics::GetPlayerPawn(this, 0);
	if (!CachedZombieAI || !PlayerActor)
	{
		return;
	}

	if (const UWorld* World = GetWorld())
	{
		ForcedQualityUntilTime = World->GetTimeSeconds() + TemporaryQualityDuration;
	}

	QualityMove(PlayerActor);
}

void AZombieCharacter::StartSimpleStuckCheck()
{
	if (SimpleStuckCheckInterval <= 0.0f)
	{
		return;
	}

	LastSimpleObservedLocation = GetActorLocation();
	GetWorldTimerManager().ClearTimer(SimpleStuckCheckTimerHandle);
	GetWorldTimerManager().SetTimer(
		SimpleStuckCheckTimerHandle,
		this,
		&AZombieCharacter::CheckSimpleStuck,
		SimpleStuckCheckInterval,
		true
	);
}

void AZombieCharacter::StopSimpleStuckCheck()
{
	GetWorldTimerManager().ClearTimer(SimpleStuckCheckTimerHandle);
}

void AZombieCharacter::CheckSimpleStuck()
{
	if (!CachedZombieAI || CachedZombieAI->GetCurrentMode() != EZombieAIMode::Simple)
	{
		StopSimpleStuckCheck();
		return;
	}

	const float CurrentTime = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0f;
	if ((CurrentTime - LastSimpleMoveRequestTime) < SimpleStuckGracePeriod)
	{
		ConsecutiveSimpleStuckCount = 0;
		LastSimpleObservedLocation = GetActorLocation();
		return;
	}

	const FVector CurrentLocation = GetActorLocation();
	const float MovementSinceLastCheck = FVector::Dist(CurrentLocation, LastSimpleObservedLocation);
	const float CurrentSpeed = GetVelocity().Size();
	LastSimpleObservedLocation = CurrentLocation;

	const float DistanceToGoal = FVector::Dist(CurrentLocation, LastSimpleTargetLocation);
	if (DistanceToGoal <= 100.0f)
	{
		ConsecutiveSimpleStuckCount = 0;
		return;
	}

	if (MovementSinceLastCheck > SimpleStuckDistanceThreshold || CurrentSpeed > SimpleStuckSpeedThreshold)
	{
		ConsecutiveSimpleStuckCount = 0;
		return;
	}

	++ConsecutiveSimpleStuckCount;
	if (ConsecutiveSimpleStuckCount < SimpleStuckCountForRecovery)
	{
		return;
	}

	HandleSimpleStuck();
}

void AZombieCharacter::HandleSimpleStuck()
{
	ConsecutiveSimpleStuckCount = 0;

	AActor* PlayerActor = UGameplayStatics::GetPlayerPawn(this, 0);
	if (PlayerActor)
	{
		LastSimpleTargetLocation = PlayerActor->GetActorLocation();
	}

	// Re-issue the cheap move once before escalating.
	if (PlayerActor && SimpleStuckRecoveryAttemptCount == 0)
	{
		++SimpleStuckRecoveryAttemptCount;
		SimpleMove(PlayerActor->GetActorLocation());
		return;
	}

	SimpleStuckRecoveryAttemptCount = 0;
	TriggerTemporaryQualityFromSimpleBlock();
}

void AZombieCharacter::ResetQualityStuckTracking()
{
	QualityStuckTime = 0.0f;
	LastQualityObservedLocation = GetActorLocation();
}

void AZombieCharacter::ResetRepeatedNavLinkTracking()
{
	RepeatedNavLinkCount = 0;
	LastNavLinkStartLocation = FVector::ZeroVector;
	LastNavLinkEndLocation = FVector::ZeroVector;
	LastNavLinkContext = FZombieNavLinkContext();
}

void AZombieCharacter::DisableZombieActor()
{
	StopSimpleStuckCheck();

	if (AZombieAIController* AICon = Cast<AZombieAIController>(GetController()))
	{
		AICon->StopMovement();
	}

	if (UCharacterMovementComponent* MoveComp = GetCharacterMovement())
	{
		MoveComp->StopMovementImmediately();
		MoveComp->DisableMovement();
		MoveComp->SetComponentTickEnabled(false);
	}

	if (UCapsuleComponent* Capsule = GetCapsuleComponent())
	{
		Capsule->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		Capsule->SetCollisionResponseToAllChannels(ECR_Ignore);
	}

	SetActorTickEnabled(false);
}

void AZombieCharacter::ResumeQualityChaseAfterNavLink()
{
	bIsUsingQualityNavLink = false;
	bIgnoreNextQualityMoveFinished = false;

	if (!CachedZombieAI || CachedZombieAI->GetCurrentMode() != EZombieAIMode::Quality)
	{
		return;
	}

	ResetQualityStuckTracking();
	RequestQualityRepath(true);
}

void AZombieCharacter::HandleQualityStuck(float DeltaTime, float DistanceToTarget)
{
	if (!CachedZombieAI || DistanceToTarget <= QualityStuckMinDistanceToTarget)
	{
		ResetQualityStuckTracking();
		return;
	}

	const float CurrentTime = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0f;
	if ((CurrentTime - LastQualityRepathTime) < QualityStuckGracePeriod)
	{
		ResetQualityStuckTracking();
		return;
	}

	const FVector CurrentLocation = GetActorLocation();
	const float MovementSinceLastTick = FVector::Dist(CurrentLocation, LastQualityObservedLocation);
	const float CurrentSpeed = GetVelocity().Size();

	LastQualityObservedLocation = CurrentLocation;

	if (MovementSinceLastTick > QualityStuckDistanceThreshold || CurrentSpeed > QualityStuckSpeedThreshold)
	{
		QualityStuckTime = 0.0f;
		return;
	}

	QualityStuckTime += DeltaTime;

	if (QualityStuckTime < QualityStuckDuration)
	{
		return;
	}

	QualityStuckTime = 0.0f;
	LaunchOutOfQualityStuck();
	CachedZombieAI->StopMovement();
	RequestQualityRepath(true);
}

void AZombieCharacter::LaunchOutOfQualityStuck()
{
	FVector LaunchDirection = GetActorForwardVector();
	LaunchDirection.Z = 0.0f;
	LaunchDirection = LaunchDirection.GetSafeNormal();

	const FVector LaunchVelocity =
		(LaunchDirection * QualityStuckLaunchForwardStrength) +
		(FVector::UpVector * QualityStuckLaunchUpStrength);

	LaunchCharacter(LaunchVelocity, true, true);
}

void AZombieCharacter::TryRecoverOffNavAfterNavLink()
{
	UNavigationSystemV1* NavSys = FNavigationSystem::GetCurrent<UNavigationSystemV1>(GetWorld());
	if (!NavSys)
	{
		return;
	}

	FNavLocation ProjectedLocation;
	const bool bIsOnNav = NavSys->ProjectPointToNavigation(GetActorLocation(), ProjectedLocation, NavLinkOffNavProjectionExtent);

	FVector ForwardDirection = GetActorForwardVector();
	ForwardDirection.Z = 0.0f;
	ForwardDirection = ForwardDirection.GetSafeNormal();

	if (ForwardDirection.IsNearlyZero())
	{
		return;
	}

	const bool bNeedsJumpFinishNudge =
		LastNavLinkContext.LinkType == EZombieSmartNavLinkType::Jump &&
		FVector::DistSquared2D(GetActorLocation(), LastNavLinkContext.EndLocation) > FMath::Square(JumpNavLinkEndLocationTolerance);

	if (bIsOnNav && !bNeedsJumpFinishNudge)
	{
		return;
	}

	float ForwardStrength = NavLinkOffNavRecoveryForwardStrength;
	float UpStrength = NavLinkOffNavRecoveryUpStrength;

	if (bNeedsJumpFinishNudge)
	{
		ForwardStrength = FMath::Max(ForwardStrength, JumpNavLinkFinishForwardNudge);
		UpStrength = 0.0f;
	}

	const FVector RecoveryLaunchVelocity =
		(ForwardDirection * ForwardStrength) +
		(FVector::UpVector * UpStrength);

	LaunchCharacter(RecoveryLaunchVelocity, true, true);
}

void AZombieCharacter::OnQualityMoveFinished(EPathFollowingResult::Type ResultCode)
{
	if (bIsUsingQualityNavLink)
	{
		return;
	}

	if (bIgnoreNextQualityMoveFinished)
	{
		bIgnoreNextQualityMoveFinished = false;
		return;
	}

	if (ResultCode == EPathFollowingResult::Success)
	{
		ResetQualityStuckTracking();
		return;
	}

	if (ResultCode == EPathFollowingResult::Aborted)
	{
		return;
	}

	TWeakObjectPtr<AZombieCharacter> WeakThis(this);
	FTimerDelegate RetryDelegate = FTimerDelegate::CreateLambda([WeakThis]()
	{
		if (!WeakThis.IsValid())
		{
			return;
		}

		AZombieCharacter* ZombieCharacter = WeakThis.Get();
		if (!ZombieCharacter->CachedZombieAI ||
			ZombieCharacter->CachedZombieAI->GetCurrentMode() != EZombieAIMode::Quality)
		{
			return;
		}

		ZombieCharacter->CachedZombieAI->StopMovement();
		ZombieCharacter->RequestQualityRepath(true);
	});

	if (QualityFailureRetryDelay <= 0.0f)
	{
		GetWorldTimerManager().SetTimerForNextTick(RetryDelegate);
		return;
	}

	FTimerHandle RetryHandle;
	GetWorldTimerManager().SetTimer(RetryHandle, RetryDelegate, QualityFailureRetryDelay, false);
}

void AZombieCharacter::OnDespawnTimerExpired()
{
	if (AZombieManager* Manager = Cast<AZombieManager>(GetOwner()))
	{
		Manager->DeSpawnZombie(this);
	}
}
