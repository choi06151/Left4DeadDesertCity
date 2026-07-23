#include "ZombieCharacter.h"

#include "Components/CapsuleComponent.h"
#include "Components/SkeletalMeshComponent.h"
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
	ConfigureActiveCollision();
	SetActorTickEnabled(false);
}

void AZombieCharacter::ConfigureActiveCollision()
{
	if (UCapsuleComponent* Capsule = GetCapsuleComponent())
	{
		// Restore the profile after pooling, then overlap only other zombies so
		// hordes cannot physically deadlock. Players and world geometry still block.
		Capsule->SetCollisionProfileName(TEXT("Pawn"));
		Capsule->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
		Capsule->SetCollisionObjectType(ECC_GameTraceChannel3);
		Capsule->SetCollisionResponseToChannel(ECC_GameTraceChannel3, ECR_Overlap);
	}
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

	if (TryAttackCurrentTarget())
	{
		return;
	}

	UpdateQualityChase(DeltaTime);
}

void AZombieCharacter::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
	Super::SetupPlayerInputComponent(PlayerInputComponent);
}

void AZombieCharacter::SimpleMove(FVector TargetLocation)
{
	if (bIsZombieDeactivated)
	{
		return;
	}

	SimpleMoveInternal(TargetLocation, true);
}

void AZombieCharacter::SimpleMoveInternal(FVector TargetLocation, bool bResetRecoveryAttempt)
{
	if (bIsZombieDeactivated)
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

	if (const AZombieManager* Manager = Cast<AZombieManager>(GetOwner()))
	{
		TargetLocation = Manager->GetSurroundSlotLocation(this, TargetLocation);
	}

	QualityTargetActor = nullptr;
	bIsAttacking = false;
	LastQualityTargetCenterLocation = FVector::ZeroVector;
	LastQualityGoalLocation = FVector::ZeroVector;
	LastQualityRepathTime = 0.0f;
	bHasQualityRepathGoal = false;
	LastSimpleTargetLocation = TargetLocation;
	LastSimpleObservedLocation = GetActorLocation();
	ConsecutiveSimpleStuckCount = 0;
	if (bResetRecoveryAttempt)
	{
		SimpleStuckRecoveryAttemptCount = 0;
	}
	LastSimpleMoveRequestTime = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0f;
	ForcedQualityUntilTime = 0.0f;
	bIsUsingQualityNavLink = false;
	bIgnoreNextQualityMoveFinished = false;
	ResetQualityStuckTracking();
	ResetRepeatedNavLinkTracking();
	ConfigureActiveCollision();
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
	if (bIsZombieDeactivated || !TargetActor)
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
	bIsAttacking = false;
	ConfigureActiveCollision();
	SetActorTickEnabled(true);
	StopSimpleStuckCheck();
	bIsUsingQualityNavLink = false;
	bIgnoreNextQualityMoveFinished = false;
	CachedZombieAI->SetAIMode(EZombieAIMode::Quality);
	CachedZombieAI->SetPerceptionEnabled(false);
	CachedZombieAI->StopMovement();
	ResetQualityStuckTracking();
	QualityStuckGraceUntilTime = (GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0f) + QualityStuckGracePeriod;
	ResetRepeatedNavLinkTracking();
	LastQualityObservedLocation = GetActorLocation();
	RequestQualityRepath(true);
}

void AZombieCharacter::MoveToActor(AActor* TargetActor)
{
	if (bIsZombieDeactivated || !IsValid(TargetActor))
	{
		return;
	}

	SimpleMoveInternal(TargetActor->GetActorLocation(), true);
	QualityTargetActor = TargetActor;
	SetActorTickEnabled(true);
}

void AZombieCharacter::FinishQualityMoveNavLink()
{
	if (bIsZombieDeactivated)
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

	if (UZombieCrowdFollowingComponent* CrowdFollowComp =
		Cast<UZombieCrowdFollowingComponent>(CachedZombieAI->GetPathFollowingComponent()))
	{
		CrowdFollowComp->FinishActiveCustomLink();
	}

	TryRelocateFromNavLinkCollision();

	if (TryRecoverOffNavAfterNavLink())
	{
		return;
	}

	ResumeQualityChaseAfterNavLink();
}

bool AZombieCharacter::IsCapsuleOverlappingBlockingGeometry(const FVector& Location) const
{
	UWorld* World = GetWorld();
	const UCapsuleComponent* Capsule = GetCapsuleComponent();
	if (!World || !Capsule)
	{
		return false;
	}

	// Shrink slightly so touching the floor or a wall is not treated as penetration.
	const float Radius = FMath::Max(1.0f, Capsule->GetScaledCapsuleRadius() - 2.0f);
	const float HalfHeight = FMath::Max(Radius, Capsule->GetScaledCapsuleHalfHeight() - 2.0f);
	const FCollisionShape CapsuleShape = FCollisionShape::MakeCapsule(Radius, HalfHeight);

	FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(ZombieNavLinkCollisionRecovery), false, this);
	return World->OverlapBlockingTestByChannel(
		Location,
		GetActorQuat(),
		Capsule->GetCollisionObjectType(),
		CapsuleShape,
		QueryParams);
}

bool AZombieCharacter::TryRelocateFromNavLinkCollision()
{
	if (!IsCapsuleOverlappingBlockingGeometry(GetActorLocation()))
	{
		return false;
	}

	UNavigationSystemV1* NavSys = FNavigationSystem::GetCurrent<UNavigationSystemV1>(GetWorld());
	UCapsuleComponent* Capsule = GetCapsuleComponent();
	if (!NavSys || !Capsule)
	{
		return false;
	}

	const FVector SearchOrigin = bHasQualityNavLinkContext
		? LastNavLinkContext.EndLocation
		: GetActorLocation();
	const int32 AttemptCount = FMath::Max(1, NavLinkCollisionRecoveryAttempts);

	for (int32 Attempt = 0; Attempt < AttemptCount; ++Attempt)
	{
		FNavLocation ReachableLocation;
		if (!NavSys->GetRandomReachablePointInRadius(SearchOrigin, NavLinkCollisionRecoveryRadius, ReachableLocation))
		{
			continue;
		}

		FVector CandidateLocation = ReachableLocation.Location;
		CandidateLocation.Z += Capsule->GetScaledCapsuleHalfHeight();
		if (IsCapsuleOverlappingBlockingGeometry(CandidateLocation))
		{
			continue;
		}

		SetActorLocation(CandidateLocation, false, nullptr, ETeleportType::TeleportPhysics);
		if (UCharacterMovementComponent* MoveComp = GetCharacterMovement())
		{
			MoveComp->StopMovementImmediately();
		}

		UE_LOG(LogTemp, Warning, TEXT("Zombie relocated after NavLink collision: %s"),
			*CandidateLocation.ToString());
		return true;
	}

	UE_LOG(LogTemp, Warning, TEXT("Zombie NavLink collision recovery could not find a clear NavMesh location."));
	return false;
}

void AZombieCharacter::HandleQualityNavLink(const FZombieNavLinkContext& NavLinkContext)
{
	if (bIsZombieDeactivated)
	{
		return;
	}

	bHasQualityNavLinkContext = true;
	bQualityNavLinkRecoveryTriggered = false;

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
	if (bIsZombieDeactivated)
	{
		return;
	}

	StopSimpleStuckCheck();
	DisableZombieActor();

	// The corpse mesh uses the Ragdoll object type. Explicitly ignore both the
	// player Pawn channel and the custom Zombie channel while keeping world
	// collision enabled so the body can still settle on the floor.
	if (USkeletalMeshComponent* MeshComp = GetMesh())
	{
		MeshComp->SetCollisionResponseToChannel(ECC_Pawn, ECR_Ignore);
		MeshComp->SetCollisionResponseToChannel(ECC_GameTraceChannel3, ECR_Ignore);
	}

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
	GetWorldTimerManager().ClearTimer(QualityFailureRetryTimerHandle);
	StopSimpleStuckCheck();
	DisableZombieActor();

	if (AZombieManager* Manager = Cast<AZombieManager>(GetOwner()))
	{
		Manager->DeSpawnZombie(this);
	}
}

void AZombieCharacter::PrepareZombieActivation()
{
	GetWorldTimerManager().ClearTimer(DespawnTimerHandle);
	GetWorldTimerManager().ClearTimer(QualityFailureRetryTimerHandle);
	StopSimpleStuckCheck();
	if (USkeletalMeshComponent* MeshComp = GetMesh())
	{
		MeshComp->bPauseAnims = false;
	}
	bIsZombieDeactivated = false;
	bIsAttacking = false;
	QualityTargetActor.Reset();
	ForcedQualityUntilTime = 0.0f;
	LastQualityTargetCenterLocation = FVector::ZeroVector;
	LastQualityGoalLocation = FVector::ZeroVector;
	LastQualityRepathTime = 0.0f;
	bHasQualityRepathGoal = false;
	speed = FMath::FRandRange(400.0f, 600.0f);
	if (UCharacterMovementComponent* MoveComp = GetCharacterMovement())
	{
		MoveComp->MaxWalkSpeed = speed;
	}
	ConsecutiveSimpleFailureCount = 0;
	ConsecutiveSimpleStuckCount = 0;
	SimpleStuckRecoveryAttemptCount = 0;
	ResetQualityStuckTracking();
	ResetRepeatedNavLinkTracking();
}

void AZombieCharacter::PauseZombieAnimation()
{
	if (USkeletalMeshComponent* MeshComp = GetMesh())
	{
		MeshComp->bPauseAnims = true;
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

	if (bIsZombieDeactivated)
	{
		return;
	}

	if (CachedZombieAI)
	{
		CachedZombieAI->SetPerceptionEnabled(false);
	}

	if (ResultCode == EPathFollowingResult::Success)
	{
		ConsecutiveSimpleFailureCount = 0;
		SimpleStuckRecoveryAttemptCount = 0;
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
		return;
	}

	++ConsecutiveSimpleFailureCount;

	if (ConsecutiveSimpleFailureCount >= SimpleFailureCountForQuality)
	{
		TriggerTemporaryQualityFromSimpleBlock();
	}
}

void AZombieCharacter::UpdateQualityChase(float DeltaTime)
{
	if (bIsZombieDeactivated || !CachedZombieAI || CachedZombieAI->GetCurrentMode() != EZombieAIMode::Quality)
	{
		return;
	}

	if (bIsUsingQualityNavLink)
	{
		return;
	}

	if (bIsAttacking)
	{
		return;
	}

	AActor* TargetActor = QualityTargetActor.Get();
	if (!TargetActor)
	{
		SetActorTickEnabled(false);
		return;
	}

	const float DistanceToTargetSquared = FVector::DistSquared(GetActorLocation(), TargetActor->GetActorLocation());

	if (ShouldSwitchToSimpleMode(DistanceToTargetSquared))
	{
		MoveToActor(TargetActor);
		return;
	}

	HandleQualityStuck(DeltaTime, DistanceToTargetSquared);
	RequestQualityRepath(false);
}

bool AZombieCharacter::TryAttackCurrentTarget()
{
	if (bIsZombieDeactivated || bIsAttacking || bIsUsingQualityNavLink)
	{
		return bIsAttacking;
	}

	AActor* TargetActor = QualityTargetActor.Get();
	if (!IsValid(TargetActor))
	{
		QualityTargetActor.Reset();
		return false;
	}

	if (FVector::DistSquared(GetActorLocation(), TargetActor->GetActorLocation()) > FMath::Square(AttackRange))
	{
		return false;
	}

	StartZombieAttack(TargetActor);
	return true;
}

void AZombieCharacter::StartZombieAttack(AActor* TargetActor)
{
	if (bIsAttacking || !TargetActor || bIsUsingQualityNavLink)
	{
		return;
	}

	bIsAttacking = true;
	QualityTargetActor = TargetActor;
	ResetQualityStuckTracking();

	if (CachedZombieAI)
	{
		CachedZombieAI->StopMovement();
	}

	if (UCharacterMovementComponent* MoveComp = GetCharacterMovement())
	{
		MoveComp->StopMovementImmediately();
	}

	OnZombieAttack(TargetActor);
}

void AZombieCharacter::FinishZombieAttack()
{
	if (bIsZombieDeactivated || !bIsAttacking)
	{
		return;
	}

	bIsAttacking = false;
	SetActorTickEnabled(true);

	if (!CachedZombieAI)
	{
		CachedZombieAI = Cast<AZombieAIController>(GetController());
	}

	AActor* TargetActor = QualityTargetActor.Get();
	if (!IsValid(TargetActor))
	{
		TargetActor = UGameplayStatics::GetPlayerPawn(this, 0);
	}

	if (!IsValid(TargetActor) || !CachedZombieAI)
	{
		QualityTargetActor = nullptr;
		SetActorTickEnabled(false);
		return;
	}

	QualityTargetActor = TargetActor;
	CachedZombieAI->SetAIMode(EZombieAIMode::Quality);
	CachedZombieAI->SetPerceptionEnabled(false);

	if (FVector::DistSquared(GetActorLocation(), TargetActor->GetActorLocation()) <= FMath::Square(AttackRange))
	{
		return;
	}

	QualityMove(TargetActor);
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

	const FVector CurrentActorLocation = GetActorLocation();
	const FVector CurrentTargetCenterLocation = TargetActor->GetActorLocation();
	const float DistanceToTarget = FVector::Dist(CurrentActorLocation, CurrentTargetCenterLocation);
	const float CurrentTime = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0f;
	const float RepathDistance = GetQualityRepathDistanceForDistance(DistanceToTarget);
	const float RepathDistanceSquared = FMath::Square(RepathDistance);
	const float RepathInterval = GetQualityRepathIntervalForDistance(DistanceToTarget);
	const bool bTargetMovedEnough =
		!bHasQualityRepathGoal ||
		FVector::DistSquared(CurrentTargetCenterLocation, LastQualityTargetCenterLocation) >= RepathDistanceSquared;
	const bool bIntervalElapsed = (CurrentTime - LastQualityRepathTime) >= RepathInterval;

	if (!bForceRepath && !bTargetMovedEnough && !bIntervalElapsed)
	{
		return;
	}

	FVector CurrentGoalLocation = CurrentTargetCenterLocation;
	if (const AZombieManager* Manager = Cast<AZombieManager>(GetOwner()))
	{
		CurrentGoalLocation = Manager->GetSurroundSlotLocation(this, CurrentGoalLocation);
	}
	const bool bGoalMovedEnough =
		!bHasQualityRepathGoal ||
		FVector::DistSquared(CurrentGoalLocation, LastQualityGoalLocation) >= RepathDistanceSquared;

	if (!bForceRepath && !bGoalMovedEnough && CachedZombieAI->GetMoveStatus() == EPathFollowingStatus::Moving)
	{
		LastQualityRepathTime = CurrentTime;
		return;
	}

	LastQualityTargetCenterLocation = CurrentTargetCenterLocation;
	LastQualityGoalLocation = CurrentGoalLocation;
	LastQualityRepathTime = CurrentTime;
	bHasQualityRepathGoal = true;

	CachedZombieAI->MoveToLocation(
		CurrentGoalLocation,
		30.0f,
		true,
		true,
		false,
		true
	);
}

bool AZombieCharacter::ShouldSwitchToSimpleMode(float DistanceToTargetSquared) const
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

	return DistanceToTargetSquared >= FMath::Square(QualityExitDistance);
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

	if (const AZombieManager* Manager = Cast<AZombieManager>(GetOwner());
		Manager && Manager->IsZombieMoveFocusActive())
	{
		if (AActor* FocusActor = Manager->GetCurrentZombieMoveFocusActor())
		{
			MoveToActor(FocusActor);
		}
		else
		{
			SimpleMoveInternal(Manager->GetCurrentZombieMoveFocusLocation(), false);
		}
		return;
	}

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
	if (bIsZombieDeactivated || !CachedZombieAI || CachedZombieAI->GetCurrentMode() != EZombieAIMode::Simple)
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
	const float MovementSinceLastCheckSquared = FVector::DistSquared(CurrentLocation, LastSimpleObservedLocation);
	const float CurrentSpeedSquared = GetVelocity().SizeSquared();
	LastSimpleObservedLocation = CurrentLocation;

	const float DistanceToGoalSquared = FVector::DistSquared(CurrentLocation, LastSimpleTargetLocation);
	if (DistanceToGoalSquared <= FMath::Square(100.0f))
	{
		ConsecutiveSimpleStuckCount = 0;
		return;
	}

	if (MovementSinceLastCheckSquared > FMath::Square(SimpleStuckDistanceThreshold) ||
		CurrentSpeedSquared > FMath::Square(SimpleStuckSpeedThreshold))
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

	if (const AZombieManager* Manager = Cast<AZombieManager>(GetOwner());
		Manager && Manager->IsZombieMoveFocusActive())
	{
		if (AActor* FocusActor = Manager->GetCurrentZombieMoveFocusActor())
		{
			MoveToActor(FocusActor);
		}
		else
		{
			SimpleMoveInternal(Manager->GetCurrentZombieMoveFocusLocation(), false);
		}
		return;
	}

	AActor* PlayerActor = UGameplayStatics::GetPlayerPawn(this, 0);
	if (PlayerActor)
	{
		LastSimpleTargetLocation = PlayerActor->GetActorLocation();
	}

	// Re-issue the cheap move once before escalating.
	if (PlayerActor && SimpleStuckRecoveryAttemptCount == 0)
	{
		++SimpleStuckRecoveryAttemptCount;
		SimpleMoveInternal(PlayerActor->GetActorLocation(), false);
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
	bHasQualityNavLinkContext = false;
	bQualityNavLinkRecoveryTriggered = false;
}

void AZombieCharacter::DisableZombieActor()
{
	// Set this before StopMovement: stopping an active request can synchronously
	// dispatch OnMoveCompleted, which must not restart a dead zombie.
	bIsZombieDeactivated = true;
	GetWorldTimerManager().ClearTimer(QualityFailureRetryTimerHandle);
	StopSimpleStuckCheck();
	bIsAttacking = false;
	QualityTargetActor.Reset();

	if (AZombieAIController* AICon = Cast<AZombieAIController>(GetController()))
	{
		AICon->SetPerceptionEnabled(false);
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
	QualityStuckGraceUntilTime = (GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0f) + QualityStuckGracePeriod;
	RequestQualityRepath(true);
}

void AZombieCharacter::HandleQualityStuck(float DeltaTime, float DistanceToTargetSquared)
{
	if (!CachedZombieAI || DistanceToTargetSquared <= FMath::Square(QualityStuckMinDistanceToTarget))
	{
		ResetQualityStuckTracking();
		return;
	}

	const float CurrentTime = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0f;
	if (CurrentTime < QualityStuckGraceUntilTime)
	{
		ResetQualityStuckTracking();
		return;
	}

	const FVector CurrentLocation = GetActorLocation();
	const float MovementSinceLastTickSquared = FVector::DistSquared(CurrentLocation, LastQualityObservedLocation);
	const float CurrentSpeedSquared = GetVelocity().SizeSquared();

	LastQualityObservedLocation = CurrentLocation;

	if (MovementSinceLastTickSquared > FMath::Square(QualityStuckDistanceThreshold) ||
		CurrentSpeedSquared > FMath::Square(QualityStuckSpeedThreshold))
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

	if (bHasQualityNavLinkContext &&
		FVector::DistSquared2D(GetActorLocation(), LastNavLinkContext.EndLocation) <=
		FMath::Square(RepeatedNavLinkLocationTolerance))
	{
		bIsUsingQualityNavLink = true;
		bIgnoreNextQualityMoveFinished = true;
		if (TriggerQualityNavLinkRecovery())
		{
			return;
		}

		bIsUsingQualityNavLink = false;
		bIgnoreNextQualityMoveFinished = false;
	}

	LaunchOutOfQualityStuck();
	CachedZombieAI->StopMovement();
	QualityStuckGraceUntilTime = CurrentTime + QualityStuckGracePeriod;
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

bool AZombieCharacter::TryRecoverOffNavAfterNavLink()
{
	UNavigationSystemV1* NavSys = FNavigationSystem::GetCurrent<UNavigationSystemV1>(GetWorld());
	if (!NavSys)
	{
		return false;
	}

	FNavLocation ProjectedLocation;
	const bool bIsOnNav = NavSys->ProjectPointToNavigation(GetActorLocation(), ProjectedLocation, NavLinkOffNavProjectionExtent);

	FVector ForwardDirection = GetActorForwardVector();
	ForwardDirection.Z = 0.0f;
	ForwardDirection = ForwardDirection.GetSafeNormal();

	if (ForwardDirection.IsNearlyZero())
	{
		return false;
	}

	const bool bNeedsJumpFinishNudge =
		LastNavLinkContext.LinkType == EZombieSmartNavLinkType::Jump &&
		FVector::DistSquared2D(GetActorLocation(), LastNavLinkContext.EndLocation) > FMath::Square(JumpNavLinkEndLocationTolerance);

	if (bIsOnNav && !bNeedsJumpFinishNudge)
	{
		return false;
	}

	if (TriggerQualityNavLinkRecovery())
	{
		return true;
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
	return false;
}

bool AZombieCharacter::TriggerQualityNavLinkRecovery()
{
	if (!bHasQualityNavLinkContext || bQualityNavLinkRecoveryTriggered)
	{
		return false;
	}

	bQualityNavLinkRecoveryTriggered = true;

	if (CachedZombieAI)
	{
		CachedZombieAI->StopMovement();
	}

	OnQualityMoveNavLinkRecovery(LastNavLinkContext);
	return true;
}

void AZombieCharacter::OnQualityMoveFinished(EPathFollowingResult::Type ResultCode)
{
	if (bIsZombieDeactivated)
	{
		return;
	}

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
		if (ZombieCharacter->IsZombieDeactivated() ||
			!ZombieCharacter->CachedZombieAI ||
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

	GetWorldTimerManager().ClearTimer(QualityFailureRetryTimerHandle);
	GetWorldTimerManager().SetTimer(QualityFailureRetryTimerHandle, RetryDelegate, QualityFailureRetryDelay, false);
}

void AZombieCharacter::OnDespawnTimerExpired()
{
	if (AZombieManager* Manager = Cast<AZombieManager>(GetOwner()))
	{
		Manager->DeSpawnZombie(this);
	}
}
