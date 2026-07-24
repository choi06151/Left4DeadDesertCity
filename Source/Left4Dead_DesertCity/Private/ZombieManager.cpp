#include "ZombieManager.h"

#include "Components/CapsuleComponent.h"
#include "Engine/TargetPoint.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/Pawn.h"
#include "Kismet/GameplayStatics.h"
#include "NavigationPath.h"
#include "NavigationSystem.h"
#include "Perception/AISense_Hearing.h"
#include "ZombieAIController.h"
#include "ZombieCharacter.h"

AZombieManager::AZombieManager()
{
	PrimaryActorTick.bCanEverTick = true;
}

void AZombieManager::BeginPlay()
{
	Super::BeginPlay();

	Player = UGameplayStatics::GetPlayerPawn(this, 0);
	CollectInitialSpawnTargetPoints();
	AdoptLevelPlacedZombies();
	InitializeZombies();
	// Initial level placement is a staging state. A wave controller must call
	// SetZombieWaveIndex(1+) before these zombies receive movement commands.
	SetZombieWaveIndex(0);
	SchedulePoolWarmup();
}

void AZombieManager::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	GetWorldTimerManager().ClearTimer(PoolWarmupTimerHandle);
	Super::EndPlay(EndPlayReason);
}

void AZombieManager::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	if (CurrentWaveIndex > 0 && GetActiveZombieCount() < FMath::Min(DesiredActiveZombieCount, Zombies.Num()))
	{
		WaveRefillElapsedTime += DeltaTime;
		if (WaveRefillElapsedTime >= FMath::Max(0.01f, WaveRefillInterval))
		{
			WaveRefillElapsedTime = 0.0f;
			MaintainZombiePopulation(FMath::Max(1, WaveRefillBatchSize), true);
		}
	}
	else
	{
		WaveRefillElapsedTime = 0.0f;
	}

	if (bZombieMoveFocusActive && ZombieMoveFocusActor.IsValid())
	{
		const FVector CurrentFocusLocation = GetMoveFocusLocation();
		if (FVector::DistSquared(CurrentFocusLocation, LastAppliedMoveFocusLocation) >=
			FMath::Square(MoveFocusRefreshDistance))
		{
			ApplyMoveFocusToActiveZombies(false);
		}

		MaintainActorMoveFocus();
	}
}

void AZombieManager::InitializeZombies()
{
	if (!ZombieClassSlot)
	{
		UE_LOG(LogTemp, Error, TEXT("ZombieClassSlot is not assigned on ZombieManager."));
		return;
	}

	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	const int32 InitialPoolCount = FMath::Min(InitialPooledZombieCount, MaxPooledZombieCount);
	while (Zombies.Num() < InitialPoolCount)
	{
		if (!TrySpawnInitialWaitingZombie())
		{
			break;
		}
	}
}

void AZombieManager::SpawnZombie()
{
	if (CurrentWaveIndex <= 0 || DesiredActiveZombieCount <= 0)
	{
		UE_LOG(LogTemp, Verbose,
			TEXT("SpawnZombie ignored because no wave is active. Wave=%d Active=%d Desired=%d"),
			CurrentWaveIndex,
			GetActiveZombieCount(),
			DesiredActiveZombieCount);
		return;
	}

	const int32 TargetActiveZombieCount = FMath::Min(DesiredActiveZombieCount, Zombies.Num());
	if (GetActiveZombieCount() >= TargetActiveZombieCount)
	{
		return;
	}

	const float CurrentTime = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0f;
	const float MinSpawnInterval = FMath::Max(0.01f, WaveRefillInterval);
	if (CurrentTime - LastManualSpawnRequestTime < MinSpawnInterval)
	{
		return;
	}

	LastManualSpawnRequestTime = CurrentTime;
	MaintainZombiePopulation(1, false);
}

void AZombieManager::SpawnZombieInternal(bool bLaunchOnActivation)
{
	if (!Player)
	{
		return;
	}

	if (Zombies.Num() == 0)
	{
		EnsureZombiePoolSize(1);
	}

	AZombieCharacter* TargetZombie = TakeAvailablePooledZombie(CurrentWaveIndex > 0);
	if (!TargetZombie)
	{
		return;
	}

	FVector SpawnLocation = FVector::ZeroVector;
	if (!FindSpawnLocationAroundPlayer(TargetZombie, SpawnLocation))
	{
		UE_LOG(LogTemp, Warning, TEXT("Zombie spawn validation failed after %d attempts."), SpawnMaxAttempts);
		return;
	}

	if (UCapsuleComponent* Capsule = TargetZombie->GetCapsuleComponent())
	{
		// DisableZombieActor clears every response for pooling, so restore the
		// active collision profile before showing or launching the zombie again.
		Capsule->SetCollisionProfileName(TEXT("Pawn"));
		Capsule->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
		Capsule->SetCollisionObjectType(ECC_GameTraceChannel3);
		Capsule->SetCollisionResponseToChannel(ECC_GameTraceChannel3, ECR_Overlap);
		SpawnLocation.Z += Capsule->GetScaledCapsuleHalfHeight();
	}

	const FVector ToPlayer = Player->GetActorLocation() - SpawnLocation;
	FRotator SpawnRotation = ToPlayer.Rotation();
	SpawnRotation.Pitch = 0.0f;
	SpawnRotation.Roll = 0.0f;

	TargetZombie->SetActorLocationAndRotation(SpawnLocation, SpawnRotation);
	ActivateZombie(TargetZombie, bLaunchOnActivation);
	CurrentSpawnedZombieCount = GetActiveZombieCount();
}

void AZombieManager::EnsureZombiePoolSize(int32 DesiredPoolSize)
{
	const int32 ClampedDesiredPoolSize = FMath::Clamp(DesiredPoolSize, 0, MaxPooledZombieCount);
	while (Zombies.Num() < ClampedDesiredPoolSize)
	{
		if (!TrySpawnInitialWaitingZombie())
		{
			break;
		}
	}
}

AZombieCharacter* AZombieManager::TakeAvailablePooledZombie(bool bAllowForceRecycleDeactivated)
{
	for (int32 Index = InitialWaitingZombies.Num() - 1; Index >= 0; --Index)
	{
		AZombieCharacter* Zombie = InitialWaitingZombies[Index].Get();
		if (!Zombie)
		{
			InitialWaitingZombies.RemoveAt(Index);
			continue;
		}

		InitialWaitingZombies.RemoveAt(Index);
		return Zombie;
	}

	for (const TWeakObjectPtr<AZombieCharacter>& ZombiePtr : Zombies)
	{
		AZombieCharacter* Zombie = ZombiePtr.Get();
		if (Zombie && Zombie->IsHidden())
		{
			DeactivatedZombiesAwaitingPool.Remove(Zombie);
			return Zombie;
		}
	}

	if (bAllowForceRecycleDeactivated)
	{
		for (int32 Index = 0; Index < DeactivatedZombiesAwaitingPool.Num();)
		{
			AZombieCharacter* Zombie = DeactivatedZombiesAwaitingPool[Index].Get();
			if (!Zombie)
			{
				DeactivatedZombiesAwaitingPool.RemoveAt(Index);
				continue;
			}

			DeactivatedZombiesAwaitingPool.RemoveAt(Index);
			if (Zombie->IsZombieDeactivated())
			{
				Zombie->ZombieForceReturnToPool();
				return Zombie;
			}
		}
	}

	return nullptr;
}

bool AZombieManager::FindSpawnLocationAroundPlayer(AZombieCharacter* TargetZombie, FVector& OutSpawnLocation) const
{
	if (!Player || !TargetZombie || !GetWorld())
	{
		return false;
	}

	const FVector PlayerLocation = Player->GetActorLocation();
	if (UNavigationSystemV1* NavSys = FNavigationSystem::GetCurrent<UNavigationSystemV1>(GetWorld()))
	{
		for (int32 Try = 0; Try < SpawnMaxAttempts; ++Try)
		{
			const float RandomAngleRadians = FMath::FRandRange(0.0f, 2.0f * PI);
			const float RandomDistance = FMath::FRandRange(SpawnMinDistance, SpawnMaxDistance);
			const FVector2D RandomDirection(FMath::Cos(RandomAngleRadians), FMath::Sin(RandomAngleRadians));
			const FVector DesiredLocation = PlayerLocation + FVector(RandomDirection.X * RandomDistance, RandomDirection.Y * RandomDistance, 0.0f);

			FNavLocation ProjectedLocation;
			if (!NavSys->ProjectPointToNavigation(DesiredLocation, ProjectedLocation, FVector(500.0f, 500.0f, 300.0f)))
			{
				continue;
			}

			FHitResult Hit;
			const FVector TraceStart = ProjectedLocation.Location + FVector(0.0f, 0.0f, 1000.0f);
			const FVector TraceEnd = ProjectedLocation.Location - FVector(0.0f, 0.0f, 50000.0f);

			FCollisionQueryParams Params(SCENE_QUERY_STAT(ZombieSpawnTrace), true);
			Params.AddIgnoredActor(TargetZombie);

			const bool bHit = GetWorld()->LineTraceSingleByChannel(
				Hit,
				TraceStart,
				TraceEnd,
				ECC_WorldStatic,
				Params
			);

			if (!bHit)
			{
				continue;
			}

			if (!IsSpawnLocationValid(Hit.Location, TargetZombie))
			{
				continue;
			}

			OutSpawnLocation = Hit.Location;
			return true;
		}
	}

	return false;
}

void AZombieManager::DeSpawnZombie(AZombieCharacter* Zombie)
{
	if (!Zombie)
	{
		return;
	}

	InitialWaitingZombies.Remove(Zombie);
	DeactivatedZombiesAwaitingPool.Remove(Zombie);
	Zombie->SetActorHiddenInGame(true);
	Zombie->SetActorEnableCollision(false);
	Zombie->SetActorLocation(FVector(0.0f, 0.0f, -10000.0f));
	CurrentSpawnedZombieCount = GetActiveZombieCount();
}

void AZombieManager::SetZombieWaveIndex(int32 WaveIndex)
{
	const int32 PreviousWaveIndex = CurrentWaveIndex;
	CurrentWaveIndex = FMath::Clamp(WaveIndex, 0, 6);
	DesiredActiveZombieCount = ResolveDesiredActiveZombieCount(CurrentWaveIndex);

	if (CurrentWaveIndex > 0)
	{
		EnsureZombiePoolSize(DesiredActiveZombieCount);
	}

	const int32 TrimmedCount = TrimActiveZombiesToDesiredCount();
	const int32 ActiveCountAfterTrim = GetActiveZombieCount();
	if (CurrentWaveIndex > 0 && CurrentWaveIndex == PreviousWaveIndex && ActiveCountAfterTrim > 0)
	{
		CurrentSpawnedZombieCount = ActiveCountAfterTrim;
		UE_LOG(LogTemp, Verbose,
			TEXT("Duplicate zombie wave request ignored: RequestedWave=%d Wave=%d DesiredActive=%d Active=%d Trimmed=%d"),
			WaveIndex,
			CurrentWaveIndex,
			DesiredActiveZombieCount,
			ActiveCountAfterTrim,
			TrimmedCount);
		return;
	}

	WaveRefillElapsedTime = 0.0f;
	LastManualSpawnRequestTime = -FLT_MAX;

	const int32 InitialSpawnCount = CurrentWaveIndex > 0
		? FMath::Min(FMath::Max(0, WaveActivationBurstCount), DesiredActiveZombieCount)
		: 0;
	MaintainZombiePopulation(InitialSpawnCount, CurrentWaveIndex > 0);

	if (CurrentWaveIndex > 0)
	{
		ApplyWaveSpeedBoostToActiveZombies();
	}

	UE_LOG(LogTemp, Log,
		TEXT("Zombie wave set: RequestedWave=%d Wave=%d DesiredActive=%d InitialBurst=%d LegacyBurst=%d RefillBatch=%d RefillInterval=%.2f Active=%d Pool=%d Waiting=%d Deactivated=%d Trimmed=%d"),
		WaveIndex,
		CurrentWaveIndex,
		DesiredActiveZombieCount,
		InitialSpawnCount,
		WaveStartBurstCount,
		WaveRefillBatchSize,
		WaveRefillInterval,
		GetActiveZombieCount(),
		Zombies.Num(),
		InitialWaitingZombies.Num(),
		GetDeactivatedAwaitingPoolCount(),
		TrimmedCount);
}

void AZombieManager::NotifyZombieDeactivated(AZombieCharacter* Zombie)
{
	if (!Zombie || DeactivatedZombiesAwaitingPool.Contains(Zombie))
	{
		return;
	}

	InitialWaitingZombies.Remove(Zombie);
	DeactivatedZombiesAwaitingPool.Add(Zombie);
	CurrentSpawnedZombieCount = GetActiveZombieCount();
}

void AZombieManager::ReportPlayerGunshot(float Loudness)
{
	if (!Player || !GetWorld())
	{
		return;
	}

	UAISense_Hearing::ReportNoiseEvent(
		GetWorld(),
		Player->GetActorLocation(),
		FMath::Max(0.0f, Loudness),
		Player,
		0.0f,
		TEXT("Gunshot"));

	// Staged zombies deliberately have perception disabled, so they cannot
	// receive the hearing stimulus themselves. Wake a limited number of the
	// nearest staged zombies here and let their normal AI chase take over.
	const int32 ResponderLimit = FMath::Max(0, MaxIdleGunshotResponders);
	const float AlertDistanceSquared = FMath::Square(FMath::Max(0.0f, GunshotIdleAlertDistance));
	for (int32 ResponderIndex = 0; ResponderIndex < ResponderLimit; ++ResponderIndex)
	{
		int32 NearestWaitingIndex = INDEX_NONE;
		float NearestDistanceSquared = AlertDistanceSquared;

		for (int32 WaitingIndex = InitialWaitingZombies.Num() - 1; WaitingIndex >= 0; --WaitingIndex)
		{
			AZombieCharacter* WaitingZombie = InitialWaitingZombies[WaitingIndex].Get();
			if (!WaitingZombie)
			{
				InitialWaitingZombies.RemoveAt(WaitingIndex);
				continue;
			}

			const float DistanceSquared = FVector::DistSquared2D(
				WaitingZombie->GetActorLocation(), Player->GetActorLocation());
			if (DistanceSquared <= NearestDistanceSquared)
			{
				NearestDistanceSquared = DistanceSquared;
				NearestWaitingIndex = WaitingIndex;
			}
		}

		if (NearestWaitingIndex == INDEX_NONE)
		{
			break;
		}

		AZombieCharacter* RespondingZombie = InitialWaitingZombies[NearestWaitingIndex].Get();
		InitialWaitingZombies.RemoveAt(NearestWaitingIndex);
		ActivateZombie(RespondingZombie, false);
	}

	CurrentSpawnedZombieCount = GetActiveZombieCount();
}

void AZombieManager::SetZombieMoveFocusLocation(FVector TargetLocation)
{
	bZombieMoveFocusActive = true;
	ZombieMoveFocusActor.Reset();
	ZombieMoveFocusLocation = TargetLocation;
	ApplyMoveFocusToActiveZombies(true);
}

void AZombieManager::SetZombieMoveFocusActor(AActor* TargetActor)
{
	if (!IsValid(TargetActor))
	{
		UE_LOG(LogTemp, Warning, TEXT("SetZombieMoveFocusActor ignored an invalid target actor."));
		return;
	}

	bZombieMoveFocusActive = true;
	ZombieMoveFocusActor = TargetActor;
	ZombieMoveFocusLocation = TargetActor->GetActorLocation();
	ApplyMoveFocusToActiveZombies(true);
}

void AZombieManager::ClearZombieMoveFocus()
{
	if (!bZombieMoveFocusActive)
	{
		return;
	}

	bZombieMoveFocusActive = false;
	ZombieMoveFocusActor.Reset();

	for (const TWeakObjectPtr<AZombieCharacter>& ZombiePtr : Zombies)
	{
		AZombieCharacter* Zombie = ZombiePtr.Get();
		if (IsActiveZombie(Zombie))
		{
			ResumeNormalMovement(Zombie);
		}
	}
}

void AZombieManager::ApplyMoveFocusToActiveZombies(bool bForceRequest)
{
	if (!bZombieMoveFocusActive)
	{
		return;
	}

	const FVector FocusLocation = GetMoveFocusLocation();
	if (!bForceRequest && FVector::DistSquared(FocusLocation, LastAppliedMoveFocusLocation) <
		FMath::Square(MoveFocusRefreshDistance))
	{
		return;
	}

	LastAppliedMoveFocusLocation = FocusLocation;
	ZombieMoveFocusLocation = FocusLocation;

	for (const TWeakObjectPtr<AZombieCharacter>& ZombiePtr : Zombies)
	{
		AZombieCharacter* Zombie = ZombiePtr.Get();
		if (IsActiveZombie(Zombie) && !Zombie->IsZombieAttacking())
		{
			if (const AZombieAIController* AICon = Cast<AZombieAIController>(Zombie->GetController());
				AICon && AICon->GetCurrentMode() == EZombieAIMode::Quality)
			{
				continue;
			}

			if (ZombieMoveFocusActor.IsValid())
			{
				Zombie->MoveToActor(ZombieMoveFocusActor.Get());
			}
			else
			{
				Zombie->SimpleMove(FocusLocation);
			}
		}
	}
}

void AZombieManager::ApplyWaveSpeedBoostToActiveZombies()
{
	if (WaveSpeedBoostMultiplier <= 1.0f || WaveSpeedBoostDuration <= 0.0f)
	{
		return;
	}

	for (const TWeakObjectPtr<AZombieCharacter>& ZombiePtr : Zombies)
	{
		AZombieCharacter* Zombie = ZombiePtr.Get();
		if (IsActiveZombie(Zombie))
		{
			Zombie->ApplyTemporarySpeedBoost(WaveSpeedBoostMultiplier, WaveSpeedBoostDuration);
		}
	}
}

void AZombieManager::MaintainActorMoveFocus()
{
	if (!bZombieMoveFocusActive || !ZombieMoveFocusActor.IsValid())
	{
		return;
	}

	const FVector FocusLocation = GetMoveFocusLocation();
	for (const TWeakObjectPtr<AZombieCharacter>& ZombiePtr : Zombies)
	{
		AZombieCharacter* Zombie = ZombiePtr.Get();
		if (!IsActiveZombie(Zombie) || Zombie->IsZombieAttacking())
		{
			continue;
		}

		AZombieAIController* AICon = Cast<AZombieAIController>(Zombie->GetController());
		if (!AICon || AICon->GetMoveStatus() == EPathFollowingStatus::Moving)
		{
			continue;
		}

		if (AICon->GetCurrentMode() == EZombieAIMode::Quality)
		{
			continue;
		}

		const FVector ZombieGoal = GetSurroundSlotLocation(Zombie, FocusLocation);
		if (FVector::DistSquared(Zombie->GetActorLocation(), ZombieGoal) >
			FMath::Square(MoveFocusResumeDistance))
		{
			Zombie->MoveToActor(ZombieMoveFocusActor.Get());
		}
	}
}

void AZombieManager::ResumeNormalMovement(AZombieCharacter* Zombie)
{
	if (!Zombie || !Player)
	{
		return;
	}

	const float Distance = FVector::Dist(Zombie->GetActorLocation(), Player->GetActorLocation());
	if (Distance <= Zombie->QualityEnterDistance)
	{
		Zombie->QualityMove(Player);
	}
	else
	{
		Zombie->SimpleMove(Player->GetActorLocation());
	}
}

FVector AZombieManager::GetMoveFocusLocation() const
{
	return ZombieMoveFocusActor.IsValid()
		? ZombieMoveFocusActor->GetActorLocation()
		: ZombieMoveFocusLocation;
}

void AZombieManager::OnZombieArrived(AZombieCharacter* ArrivedZombie)
{
	if (!ArrivedZombie || ArrivedZombie->IsZombieDeactivated() || !Player)
	{
		return;
	}

	if (bZombieMoveFocusActive)
	{
		return;
	}

	const float Distance = FVector::Dist(ArrivedZombie->GetActorLocation(), Player->GetActorLocation());

	if (Distance <= ArrivedZombie->QualityEnterDistance)
	{
		if (AZombieAIController* AICon = Cast<AZombieAIController>(ArrivedZombie->GetController()))
		{
			AICon->SetAIMode(EZombieAIMode::Quality);
		}

		ArrivedZombie->QualityMove(Player);
		return;
	}

	ArrivedZombie->SimpleMove(Player->GetActorLocation());
}

bool AZombieManager::IsSpawnLocationValid(const FVector& CandidateLocation, AZombieCharacter* TargetZombie) const
{
	if (!Player)
	{
		return false;
	}

	const FVector PlayerLocation = Player->GetActorLocation();
	const FVector ToSpawn = CandidateLocation - PlayerLocation;
	const float DistanceToPlayer = ToSpawn.Size2D();
	if (DistanceToPlayer < SpawnMinDistance || DistanceToPlayer > SpawnMaxDistance)
	{
		return false;
	}

	return IsSpawnLocationReachableToPlayer(CandidateLocation);
}

bool AZombieManager::IsSpawnLocationReachableToPlayer(const FVector& CandidateLocation) const
{
	if (!Player || !GetWorld())
	{
		return false;
	}

	const UNavigationPath* NavPath = UNavigationSystemV1::FindPathToLocationSynchronously(
		GetWorld(),
		CandidateLocation,
		Player->GetActorLocation(),
		Player
	);

	return NavPath && NavPath->IsValid() && !NavPath->IsPartial();
}

FVector AZombieManager::GetSurroundSlotLocation(
	const AZombieCharacter* Zombie,
	const FVector& TargetCenter) const
{
	if (!Zombie)
	{
		return TargetCenter;
	}

	int32 ZombieIndex = INDEX_NONE;
	for (int32 Index = 0; Index < Zombies.Num(); ++Index)
	{
		if (Zombies[Index].Get() == Zombie)
		{
			ZombieIndex = Index;
			break;
		}
	}

	if (ZombieIndex == INDEX_NONE)
	{
		return TargetCenter;
	}

	const int32 SlotsPerRing = FMath::Max(1, SurroundSlotsPerRing);
	const int32 RingIndex = ZombieIndex / SlotsPerRing;
	const float Radius = FMath::Max(0.0f, SurroundBaseRadius + RingIndex * SurroundRingSpacing);

	// Golden-angle distribution keeps even the first few active pool entries
	// spread around the full circle while preserving a stable slot per zombie.
	constexpr float GoldenAngleRadians = PI * (3.0f - 2.2360679775f);
	const float Angle = ZombieIndex * GoldenAngleRadians;
	const FVector DesiredLocation = TargetCenter + FVector(FMath::Cos(Angle), FMath::Sin(Angle), 0.0f) * Radius;

	if (UNavigationSystemV1* NavSys = FNavigationSystem::GetCurrent<UNavigationSystemV1>(GetWorld()))
	{
		FNavLocation ProjectedLocation;
		if (NavSys->ProjectPointToNavigation(DesiredLocation, ProjectedLocation, SurroundNavProjectionExtent))
		{
			return ProjectedLocation.Location;
		}
	}

	return DesiredLocation;
}

void AZombieManager::MaintainZombiePopulation(int32 MaxSpawnCount, bool bLaunchOnActivation)
{
	if (MaxSpawnCount <= 0)
	{
		return;
	}

	const int32 TargetActiveZombieCount = FMath::Min(DesiredActiveZombieCount, Zombies.Num());
	CurrentSpawnedZombieCount = GetActiveZombieCount();

	int32 SpawnCount = 0;
	while (CurrentSpawnedZombieCount < TargetActiveZombieCount && SpawnCount < MaxSpawnCount)
	{
		const int32 ActiveCountBeforeSpawn = CurrentSpawnedZombieCount;
		SpawnZombieInternal(bLaunchOnActivation);

		if (CurrentSpawnedZombieCount <= ActiveCountBeforeSpawn)
		{
			break;
		}

		++SpawnCount;
	}
}

bool AZombieManager::IsActiveZombie(const AZombieCharacter* Zombie) const
{
	return Zombie &&
		!Zombie->IsHidden() &&
		!Zombie->IsZombieDeactivated() &&
		!InitialWaitingZombies.Contains(Zombie);
}

bool AZombieManager::IsAvailablePooledZombie(const AZombieCharacter* Zombie) const
{
	return Zombie && (Zombie->IsHidden() || InitialWaitingZombies.Contains(Zombie));
}

int32 AZombieManager::TrimActiveZombiesToDesiredCount()
{
	const int32 TargetActiveZombieCount = FMath::Clamp(DesiredActiveZombieCount, 0, Zombies.Num());
	int32 ActiveZombieCount = GetActiveZombieCount();
	int32 TrimmedZombieCount = 0;

	if (ActiveZombieCount <= TargetActiveZombieCount)
	{
		return TrimmedZombieCount;
	}

	const FVector ReferenceLocation = Player ? Player->GetActorLocation() : GetActorLocation();
	while (ActiveZombieCount > TargetActiveZombieCount)
	{
		AZombieCharacter* FarthestZombie = nullptr;
		float FarthestDistanceSquared = -1.0f;

		for (const TWeakObjectPtr<AZombieCharacter>& ZombiePtr : Zombies)
		{
			AZombieCharacter* Zombie = ZombiePtr.Get();
			if (!IsActiveZombie(Zombie))
			{
				continue;
			}

			const float DistanceSquared = FVector::DistSquared(Zombie->GetActorLocation(), ReferenceLocation);
			if (DistanceSquared > FarthestDistanceSquared)
			{
				FarthestDistanceSquared = DistanceSquared;
				FarthestZombie = Zombie;
			}
		}

		if (!FarthestZombie)
		{
			break;
		}

		FarthestZombie->ZombieForceReturnToPool();
		++TrimmedZombieCount;
		ActiveZombieCount = GetActiveZombieCount();
	}

	return TrimmedZombieCount;
}

int32 AZombieManager::GetActiveZombieCount() const
{
	int32 ActiveZombieCount = 0;

	for (const TWeakObjectPtr<AZombieCharacter>& ZombiePtr : Zombies)
	{
		const AZombieCharacter* Zombie = ZombiePtr.Get();
		if (IsActiveZombie(Zombie))
		{
			++ActiveZombieCount;
		}
	}

	return ActiveZombieCount;
}

int32 AZombieManager::GetTotalPooledZombieCount() const
{
	int32 TotalPooledZombieCount = 0;

	for (const TWeakObjectPtr<AZombieCharacter>& ZombiePtr : Zombies)
	{
		if (ZombiePtr.IsValid())
		{
			++TotalPooledZombieCount;
		}
	}

	return TotalPooledZombieCount;
}

int32 AZombieManager::GetWaitingPooledZombieCount() const
{
	int32 WaitingPooledZombieCount = 0;

	for (const TWeakObjectPtr<AZombieCharacter>& ZombiePtr : InitialWaitingZombies)
	{
		if (ZombiePtr.IsValid())
		{
			++WaitingPooledZombieCount;
		}
	}

	return WaitingPooledZombieCount;
}

int32 AZombieManager::GetHiddenPooledZombieCount() const
{
	int32 HiddenPooledZombieCount = 0;

	for (const TWeakObjectPtr<AZombieCharacter>& ZombiePtr : Zombies)
	{
		const AZombieCharacter* Zombie = ZombiePtr.Get();
		if (Zombie && Zombie->IsHidden())
		{
			++HiddenPooledZombieCount;
		}
	}

	return HiddenPooledZombieCount;
}

int32 AZombieManager::GetAvailablePooledZombieCount() const
{
	int32 AvailablePooledZombieCount = 0;

	for (const TWeakObjectPtr<AZombieCharacter>& ZombiePtr : Zombies)
	{
		const AZombieCharacter* Zombie = ZombiePtr.Get();
		if (IsAvailablePooledZombie(Zombie))
		{
			++AvailablePooledZombieCount;
		}
	}

	return AvailablePooledZombieCount;
}

int32 AZombieManager::GetDeactivatedAwaitingPoolCount() const
{
	int32 DeactivatedAwaitingPoolCount = 0;

	for (const TWeakObjectPtr<AZombieCharacter>& ZombiePtr : DeactivatedZombiesAwaitingPool)
	{
		const AZombieCharacter* Zombie = ZombiePtr.Get();
		if (Zombie && Zombie->IsZombieDeactivated() && !Zombie->IsHidden())
		{
			++DeactivatedAwaitingPoolCount;
		}
	}

	return DeactivatedAwaitingPoolCount;
}

FZombieManagerDebugStats AZombieManager::GetZombieDebugStats() const
{
	FZombieManagerDebugStats Stats;
	Stats.CurrentWaveIndex = CurrentWaveIndex;
	Stats.DesiredActiveZombieCount = DesiredActiveZombieCount;
	Stats.WaveStartBurstCount = WaveStartBurstCount;
	Stats.WaveActivationBurstCount = WaveActivationBurstCount;
	Stats.WaveRefillBatchSize = WaveRefillBatchSize;
	Stats.WaveRefillInterval = WaveRefillInterval;
	Stats.ActiveZombieCount = GetActiveZombieCount();
	Stats.CurrentSpawnedZombieCount = CurrentSpawnedZombieCount;
	Stats.TotalPooledZombieCount = GetTotalPooledZombieCount();
	Stats.MaxPooledZombieCount = MaxPooledZombieCount;
	Stats.WaitingPooledZombieCount = GetWaitingPooledZombieCount();
	Stats.HiddenPooledZombieCount = GetHiddenPooledZombieCount();
	Stats.AvailablePooledZombieCount = GetAvailablePooledZombieCount();
	Stats.DeactivatedAwaitingPoolCount = GetDeactivatedAwaitingPoolCount();
	return Stats;
}

int32 AZombieManager::ResolveDesiredActiveZombieCount(int32 WaveIndex) const
{
	return WaveIndex > 0 ? WaveIndex * 15 : 0;
}

void AZombieManager::SchedulePoolWarmup()
{
	if (Zombies.Num() >= MaxPooledZombieCount || PoolWarmupInterval <= 0.0f)
	{
		return;
	}

	GetWorldTimerManager().SetTimer(
		PoolWarmupTimerHandle,
		this,
		&AZombieManager::WarmupZombiePool,
		PoolWarmupInterval,
		true);
}

void AZombieManager::WarmupZombiePool()
{
	if (Zombies.Num() >= MaxPooledZombieCount)
	{
		GetWorldTimerManager().ClearTimer(PoolWarmupTimerHandle);
		return;
	}

	const int32 BatchCount = FMath::Max(1, PoolWarmupBatchSize);
	for (int32 Index = 0; Index < BatchCount && Zombies.Num() < MaxPooledZombieCount; ++Index)
	{
		if (!TrySpawnInitialWaitingZombie())
		{
			GetWorldTimerManager().ClearTimer(PoolWarmupTimerHandle);
			return;
		}
	}

	// Pool warmup only increases available pooled actors. Active wave refill is
	// throttled by Tick through WaveRefillInterval/WaveRefillBatchSize.
}

void AZombieManager::CollectInitialSpawnTargetPoints()
{
	InitialSpawnTargetPoints.Reset();
	TArray<AActor*> FoundActors;
	UGameplayStatics::GetAllActorsOfClass(this, ATargetPoint::StaticClass(), FoundActors);

	FoundActors.Sort([](const AActor& Left, const AActor& Right)
	{
		return Left.GetName() < Right.GetName();
	});

	for (AActor* Actor : FoundActors)
	{
		if (ATargetPoint* TargetPoint = Cast<ATargetPoint>(Actor))
		{
			InitialSpawnTargetPoints.Add(TargetPoint);
		}
	}

	NextInitialSpawnTargetPointIndex = 0;
	UE_LOG(LogTemp, Log, TEXT("ZombieManager collected %d initial spawn TargetPoints."),
		InitialSpawnTargetPoints.Num());
}

void AZombieManager::AdoptLevelPlacedZombies()
{
	TArray<AActor*> FoundActors;
	UGameplayStatics::GetAllActorsOfClass(this, AZombieCharacter::StaticClass(), FoundActors);

	int32 AdoptedCount = 0;
	int32 HiddenOverflowCount = 0;
	for (AActor* Actor : FoundActors)
	{
		AZombieCharacter* Zombie = Cast<AZombieCharacter>(Actor);
		if (!Zombie || Zombies.Contains(Zombie))
		{
			continue;
		}

		Zombie->SetOwner(this);
		Zombie->PrepareZombiePoolIdleState();

		if (Zombies.Num() < MaxPooledZombieCount)
		{
			Zombies.Add(Zombie);
			InitialWaitingZombies.Add(Zombie);
			Zombie->OnMoveCompleted.AddUniqueDynamic(this, &AZombieManager::OnZombieArrived);
			++AdoptedCount;
		}
		else
		{
			++HiddenOverflowCount;
		}
	}

	if (AdoptedCount > 0 || HiddenOverflowCount > 0)
	{
		UE_LOG(LogTemp, Log,
			TEXT("ZombieManager adopted level-placed zombies: Adopted=%d HiddenOverflow=%d Pool=%d MaxPool=%d"),
			AdoptedCount,
			HiddenOverflowCount,
			Zombies.Num(),
			MaxPooledZombieCount);
	}
}

bool AZombieManager::TrySpawnInitialWaitingZombie()
{
	if (!ZombieClassSlot)
	{
		return false;
	}

	UWorld* World = GetWorld();
	if (!World)
	{
		return false;
	}

	FActorSpawnParameters SpawnParams;
	SpawnParams.Owner = this;
	SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

	FVector SpawnLocation(0.0f, 0.0f, -10000.0f - static_cast<float>(Zombies.Num()) * 100.0f);
	FRotator SpawnRotation = FRotator::ZeroRotator;
	while (InitialSpawnTargetPoints.IsValidIndex(NextInitialSpawnTargetPointIndex))
	{
		ATargetPoint* SpawnPoint = InitialSpawnTargetPoints[NextInitialSpawnTargetPointIndex];
		++NextInitialSpawnTargetPointIndex;

		if (IsValid(SpawnPoint))
		{
			SpawnLocation = SpawnPoint->GetActorLocation();
			SpawnLocation.Z += InitialSpawnHeightOffset;
			SpawnRotation = SpawnPoint->GetActorRotation();
			break;
		}
	}

	AZombieCharacter* NewZombie = World->SpawnActor<AZombieCharacter>(
		ZombieClassSlot,
		SpawnLocation,
		SpawnRotation,
		SpawnParams);

	if (!NewZombie)
	{
		return false;
	}
	Zombies.Add(NewZombie);
	InitialWaitingZombies.Add(NewZombie);
	NewZombie->OnMoveCompleted.AddUniqueDynamic(this, &AZombieManager::OnZombieArrived);
	NewZombie->PrepareZombiePoolIdleState();
	return true;
}

bool AZombieManager::ActivateInitialWaitingZombie(bool bIgnoreActivationDistance)
{
	if (!Player)
	{
		return false;
	}

	const FVector PlayerLocation = Player->GetActorLocation();
	const float ActivationDistanceSquared = FMath::Square(FMath::Max(0.0f, InitialIdleActivationDistance));
	int32 SelectedWaitingIndex = INDEX_NONE;
	float SelectedDistanceSquared = TNumericLimits<float>::Max();

	for (int32 Index = InitialWaitingZombies.Num() - 1; Index >= 0; --Index)
	{
		AZombieCharacter* Zombie = InitialWaitingZombies[Index].Get();
		if (!Zombie)
		{
			InitialWaitingZombies.RemoveAt(Index);
			continue;
		}

		const float DistanceSquared = FVector::DistSquared2D(Zombie->GetActorLocation(), PlayerLocation);
		if ((bIgnoreActivationDistance || DistanceSquared <= ActivationDistanceSquared) &&
			DistanceSquared < SelectedDistanceSquared)
		{
			SelectedDistanceSquared = DistanceSquared;
			SelectedWaitingIndex = Index;
		}
	}

	if (SelectedWaitingIndex == INDEX_NONE)
	{
		return false;
	}

	AZombieCharacter* Zombie = InitialWaitingZombies[SelectedWaitingIndex].Get();
	InitialWaitingZombies.RemoveAt(SelectedWaitingIndex);
	return ActivateZombie(Zombie, bIgnoreActivationDistance);
}

bool AZombieManager::ActivateZombie(AZombieCharacter* Zombie, bool bLaunchOnActivation)
{
	if (!Zombie || !Player)
	{
		return false;
	}

	Zombie->PrepareZombieActivation();
	Zombie->SetActorHiddenInGame(false);
	Zombie->SetActorEnableCollision(true);

	if (UCapsuleComponent* Capsule = Zombie->GetCapsuleComponent())
	{
		Capsule->SetCollisionProfileName(TEXT("Pawn"));
		Capsule->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
		Capsule->SetCollisionObjectType(ECC_GameTraceChannel3);
		Capsule->SetCollisionResponseToChannel(ECC_GameTraceChannel3, ECR_Overlap);
	}

	if (UCharacterMovementComponent* MoveComp = Zombie->GetCharacterMovement())
	{
		MoveComp->Velocity = FVector::ZeroVector;
		MoveComp->SetMovementMode(MOVE_Walking);
		MoveComp->SetComponentTickEnabled(true);
	}

	Zombie->ZombieActivateSet();
	AActor* MoveTargetActor = bZombieMoveFocusActive && ZombieMoveFocusActor.IsValid()
		? ZombieMoveFocusActor.Get()
		: Player;
	if (bLaunchOnActivation)
	{
		Zombie->LaunchTowardActor(MoveTargetActor);
	}

	if (AZombieAIController* AICon = Cast<AZombieAIController>(Zombie->GetController()))
	{
		AICon->SetAIMode(EZombieAIMode::Simple);
		if (bZombieMoveFocusActive && ZombieMoveFocusActor.IsValid())
		{
			Zombie->MoveToActor(ZombieMoveFocusActor.Get());
		}
		else if (!bZombieMoveFocusActive && Player)
		{
			Zombie->MoveToActor(Player);
		}
		else
		{
			Zombie->SimpleMove(bZombieMoveFocusActive
				? GetMoveFocusLocation()
				: Player->GetActorLocation());
		}
	}
	else
	{
		TWeakObjectPtr<AZombieManager> WeakManager(this);
		TWeakObjectPtr<AZombieCharacter> WeakZombie(Zombie);
		TWeakObjectPtr<AActor> WeakMoveFocusActor(ZombieMoveFocusActor.Get());
		const bool bUseMoveFocus = bZombieMoveFocusActive;
		const FVector DeferredMoveFocusLocation = GetMoveFocusLocation();

		GetWorldTimerManager().SetTimerForNextTick(FTimerDelegate::CreateLambda(
			[WeakManager, WeakZombie, WeakMoveFocusActor, bUseMoveFocus, DeferredMoveFocusLocation]()
		{
			if (!WeakManager.IsValid() || !WeakZombie.IsValid())
			{
				return;
			}

			AZombieManager* ZombieManager = WeakManager.Get();
			AZombieCharacter* DeferredZombie = WeakZombie.Get();
			if (DeferredZombie->IsZombieDeactivated())
			{
				return;
			}

			if (AZombieAIController* DeferredAI = Cast<AZombieAIController>(DeferredZombie->GetController()))
			{
				DeferredAI->SetAIMode(EZombieAIMode::Simple);
			}

			if (bUseMoveFocus)
			{
				if (WeakMoveFocusActor.IsValid())
				{
					DeferredZombie->MoveToActor(WeakMoveFocusActor.Get());
				}
				else
				{
					DeferredZombie->SimpleMove(DeferredMoveFocusLocation);
				}
			}
			else if (ZombieManager->Player)
			{
				DeferredZombie->MoveToActor(ZombieManager->Player);
			}
		}));
	}

	return true;
}
