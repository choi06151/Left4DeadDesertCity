// Fill out your copyright notice in the Description page of Project Settings.



#include "ZombieCharacter.h"
#include "TimerManager.h"
#include "ZombieManager.h" 
#include "ZombieAIController.h"
#include "Components/CapsuleComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "GameFramework/CharacterMovementComponent.h"


class AAIController;
// Sets default values
AZombieCharacter::AZombieCharacter()
{
 	// Set this character to call Tick() every frame.  You can turn this off to improve performance if you don't need it.
	PrimaryActorTick.bCanEverTick = true;

	
}

// Called when the game starts or when spawned
void AZombieCharacter::BeginPlay()
{
	Super::BeginPlay();
	
	CachedZombieAI = Cast<AZombieAIController>(GetController());
}

// Called every frame
void AZombieCharacter::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

}

// Called to bind functionality to input
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
	if (CachedZombieAI)
	{
		CachedZombieAI->SetAIMode(EZombieAIMode::Simple);
		CachedZombieAI->SetPerceptionEnabled(true);
		CachedZombieAI->MoveToLocation(
			TargetLocation,
			30.f,       // Acceptance Radius
			true,       // Stop on overlap
			true,       // Use Pathfinding
			false,      // Project Goal Location
			true        // Can Strafe
		);
	}
	
	
}

void AZombieCharacter::QualityMove(AActor* TargetActor)
{
	if (!TargetActor) return;

	if (!CachedZombieAI)
	{
		CachedZombieAI = Cast<AZombieAIController>(GetController());
	}

	if (CachedZombieAI)
	{
		// AIController의 MoveToActor 호출
		CachedZombieAI->SetAIMode(EZombieAIMode::Quality);
		CachedZombieAI->SetPerceptionEnabled(false);
		CachedZombieAI->MoveToActor(
			TargetActor,
			30.f,      // 슬롯 액터 주변 수용 반경
			true,       // Use Pathfinding
			true,       // Keep Goal Location Unchanged
			true,       // Use Continuous Goal Tracking (실시간 추적 필수!)
			nullptr,    // Filter Class
			true        // Allow Partial Path
		);
	}
	
}

void AZombieCharacter::ZombieDeActivateSet()
{
	// 1. 🧠 AI 및 이동 시스템 즉시 정지
	if (AZombieAIController* AICon = Cast<AZombieAIController>(GetController()))
	{
		AICon->StopMovement();
		// 필요하다면 컨트롤러와 캐릭터의 빙의(UnPossess)를 해제하여 AI의 뇌를 완전히 분리합니다.
		// AICon->UnPossess(); 
	}

	if (UCharacterMovementComponent* MoveComp = GetCharacterMovement())
	{
		MoveComp->StopMovementImmediately();
		MoveComp->DisableMovement(); // 무브먼트 컴포넌트 자체 기능 마비
		MoveComp->SetComponentTickEnabled(false); // 무브먼트 틱 off
	}

	// 2. 🛑 루트 캡슐 컴포넌트 충돌 완전히 끄기 (길막 방지)
	if (UCapsuleComponent* Capsule = GetCapsuleComponent())
	{
		Capsule->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		Capsule->SetCollisionResponseToAllChannels(ECR_Ignore);
	}
	

	// 4. ⏰ 캐릭터 자체 틱 및 컴포넌트 틱 끄기
	SetActorTickEnabled(false);
	
	
	GetWorldTimerManager().SetTimer(
		DespawnTimerHandle, 
		this, 
		&AZombieCharacter::OnDespawnTimerExpired, 
		timeToDisappear, 
		false // 반복 여부 (단발성)
	);
	
	
}

void AZombieCharacter::OnSimpleMoveTargetReached()
{
	if (CachedZombieAI)
	{
		CachedZombieAI->SetPerceptionEnabled(false);
	}
	// 델리게이트에 등록된 리스너(매니저 등)가 있다면 발송!
	if (OnMoveCompleted.IsBound())
	{
		OnMoveCompleted.Broadcast(this);
	}
}


void AZombieCharacter::OnDespawnTimerExpired()
{
	if (AZombieManager* Manager = Cast<AZombieManager>(GetOwner()))
	{
		Manager->DeSpawnZombie(this); // 👈 이미 가지고 계신 despawn 함수 호출!
	}
}

