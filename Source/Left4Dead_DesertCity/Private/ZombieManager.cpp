#include "ZombieManager.h"

#include "Components/CapsuleComponent.h"
#include "Engine/TargetPoint.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/Pawn.h"
#include "Kismet/GameplayStatics.h"
#include "NavigationPath.h"
#include "NavigationSystem.h"
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
	for (int32 i = 0; i < InitialPoolCount; i++)
	{
		if (!TrySpawnInitialWaitingZombie())
		{
			break;
		}
	}
}

void AZombieManager::SpawnZombie()
{
	if (!Player || Zombies.Num() == 0)
	{
		return;
	}

	// Zombies created at level start wait visibly on TargetPoints. Consume them
	// in that same order before reusing zombies that died and entered the pool.
	if (ActivateInitialWaitingZombie())
	{
		CurrentSpawnedZombieCount = GetActiveZombieCount();
		return;
	}

	AZombieCharacter* TargetZombie = nullptr;
	for (const TWeakObjectPtr<AZombieCharacter>& ZombiePtr : Zombies)
	{
		AZombieCharacter* Zombie = ZombiePtr.Get();
		if (Zombie && Zombie->IsHidden())
		{
			TargetZombie = Zombie;
			break;
		}
	}

	if (!TargetZombie)
	{
		return;
	}

	const FVector PlayerLocation = Player->GetActorLocation();
	FVector SpawnLocation = FVector::ZeroVector;
	bool bSpatialQuerySuccess = false;

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

			SpawnLocation = Hit.Location;

			if (!IsSpawnLocationValid(SpawnLocation, TargetZombie))
			{
				continue;
			}

			bSpatialQuerySuccess = true;
			break;
		}
	}

	if (!bSpatialQuerySuccess)
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

	TargetZombie->SetActorLocationAndRotation(SpawnLocation, FRotator::ZeroRotator);
	ActivateZombie(TargetZombie, true);
	CurrentSpawnedZombieCount = GetActiveZombieCount();
}

void AZombieManager::DeSpawnZombie(AZombieCharacter* Zombie)
{
	if (!Zombie)
	{
		return;
	}

	Zombie->SetActorHiddenInGame(true);
	Zombie->SetActorEnableCollision(false);
	Zombie->SetActorLocation(FVector(0.0f, 0.0f, -10000.0f));
	CurrentSpawnedZombieCount = GetActiveZombieCount();
}

void AZombieManager::SetZombieWaveIndex(int32 WaveIndex)
{
	CurrentWaveIndex = FMath::Clamp(WaveIndex, 0, 6);
	DesiredActiveZombieCount = ResolveDesiredActiveZombieCount(CurrentWaveIndex);
	MaintainZombiePopulation();
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
		if (Zombie && !Zombie->IsHidden() && !InitialWaitingZombies.Contains(Zombie))
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
		if (Zombie && !Zombie->IsHidden() &&
			!InitialWaitingZombies.Contains(Zombie) && !Zombie->IsZombieAttacking())
		{
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
		if (!Zombie || Zombie->IsHidden() ||
			InitialWaitingZombies.Contains(Zombie) || Zombie->IsZombieAttacking())
		{
			continue;
		}

		AZombieAIController* AICon = Cast<AZombieAIController>(Zombie->GetController());
		if (!AICon || AICon->GetMoveStatus() == EPathFollowingStatus::Moving)
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

void AZombieManager::MaintainZombiePopulation()
{
	const int32 TargetActiveZombieCount = FMath::Min(DesiredActiveZombieCount, Zombies.Num());
	CurrentSpawnedZombieCount = GetActiveZombieCount();

	while (CurrentSpawnedZombieCount < TargetActiveZombieCount)
	{
		const int32 ActiveCountBeforeSpawn = CurrentSpawnedZombieCount;
		SpawnZombie();

		if (CurrentSpawnedZombieCount <= ActiveCountBeforeSpawn)
		{
			break;
		}
	}
}

int32 AZombieManager::GetActiveZombieCount() const
{
	int32 ActiveZombieCount = 0;

	for (const TWeakObjectPtr<AZombieCharacter>& ZombiePtr : Zombies)
	{
		const AZombieCharacter* Zombie = ZombiePtr.Get();
		if (Zombie && !Zombie->IsHidden() && !InitialWaitingZombies.Contains(Zombie))
		{
			++ActiveZombieCount;
		}
	}

	return ActiveZombieCount;
}

int32 AZombieManager::ResolveDesiredActiveZombieCount(int32 WaveIndex) const
{
	switch (WaveIndex)
	{
	case 0:
		return 0;
	case 1:
		return 5;
	default:
		return 15 + ((WaveIndex - 2) * 5);
	}
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

	MaintainZombiePopulation();
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

	if (!InitialSpawnTargetPoints.IsValidIndex(NextInitialSpawnTargetPointIndex))
	{
		UE_LOG(LogTemp, Warning,
			TEXT("Zombie pool warmup stopped: TargetPoints=%d, requested zombies=%d. Add more TargetPoints to the level."),
			InitialSpawnTargetPoints.Num(), MaxPooledZombieCount);
		return false;
	}

	FActorSpawnParameters SpawnParams;
	SpawnParams.Owner = this;
	SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

	ATargetPoint* SpawnPoint = InitialSpawnTargetPoints[NextInitialSpawnTargetPointIndex];
	if (!IsValid(SpawnPoint))
	{
		++NextInitialSpawnTargetPointIndex;
		return false;
	}

	FVector SpawnLocation = SpawnPoint->GetActorLocation();
	const FRotator SpawnRotation = SpawnPoint->GetActorRotation();

	AZombieCharacter* NewZombie = World->SpawnActor<AZombieCharacter>(
		ZombieClassSlot,
		SpawnLocation,
		SpawnRotation,
		SpawnParams);

	if (!NewZombie)
	{
		return false;
	}
	++NextInitialSpawnTargetPointIndex;

	Zombies.Add(NewZombie);
	InitialWaitingZombies.Add(NewZombie);
	NewZombie->OnMoveCompleted.AddDynamic(this, &AZombieManager::OnZombieArrived);
	NewZombie->SetActorHiddenInGame(false);
	NewZombie->SetActorEnableCollision(true);

	if (UCapsuleComponent* Capsule = NewZombie->GetCapsuleComponent())
	{
		Capsule->SetCollisionProfileName(TEXT("Pawn"));
		Capsule->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
		Capsule->SetCollisionObjectType(ECC_GameTraceChannel3);
		Capsule->SetCollisionResponseToChannel(ECC_GameTraceChannel3, ECR_Overlap);
	}

	if (AZombieAIController* AICon = Cast<AZombieAIController>(NewZombie->GetController()))
	{
		AICon->StopMovement();
		AICon->SetPerceptionEnabled(false);
	}

	if (UCharacterMovementComponent* MoveComp = NewZombie->GetCharacterMovement())
	{
		MoveComp->StopMovementImmediately();
		MoveComp->SetMovementMode(MOVE_Walking);
		MoveComp->SetComponentTickEnabled(false);
	}
	return true;
}

bool AZombieManager::ActivateInitialWaitingZombie()
{
	while (InitialWaitingZombies.Num() > 0)
	{
		AZombieCharacter* Zombie = InitialWaitingZombies[0].Get();
		InitialWaitingZombies.RemoveAt(0);
		if (Zombie)
		{
			return ActivateZombie(Zombie, false);
		}
	}

	return false;
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
		else
		{
			Zombie->SimpleMove(bZombieMoveFocusActive
				? GetMoveFocusLocation()
				: Player->GetActorLocation());
		}
	}

	return true;
}
