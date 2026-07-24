// Copyright Epic Games, Inc. All Rights Reserved.

#include "Left4Dead_DesertCity.h"

#include "Engine/Engine.h"
#include "Engine/World.h"
#include "GameFramework/GameUserSettings.h"
#include "Misc/CoreDelegates.h"
#include "Modules/ModuleManager.h"
#include "TimerManager.h"

namespace Left4DeadWindowDefaults
{
	static void Apply(UWorld* CurrentWorld)
	{
		if (IsRunningCommandlet() || !GEngine || IsRunningDedicatedServer())
		{
			return;
		}

#if WITH_EDITOR
		if (GIsEditor)
		{
			return;
		}
#endif

		const FIntPoint WindowResolution(1920, 1080);
		UE_LOG(LogTemp, Warning, TEXT("Applying packaged module window defaults: 1920x1080 Windowed."));

		if (UGameUserSettings* Settings = GEngine->GetGameUserSettings())
		{
			Settings->SetScreenResolution(WindowResolution);
			Settings->SetFullscreenMode(EWindowMode::Windowed);
			Settings->ConfirmVideoMode();
			Settings->ApplySettings(false);
			Settings->SaveSettings();
		}

		if (CurrentWorld)
		{
			GEngine->Exec(CurrentWorld, TEXT("r.SetRes 1920x1080w"));
		}
	}

	static void ScheduleRetry(UWorld* CurrentWorld, float Delay)
	{
		if (!CurrentWorld)
		{
			return;
		}

		TWeakObjectPtr<UWorld> WorldPtr(CurrentWorld);
		FTimerDelegate RetryDelegate;
		RetryDelegate.BindLambda([WorldPtr]()
		{
			if (UWorld* RetryWorld = WorldPtr.Get())
			{
				Apply(RetryWorld);
			}
		});

		FTimerHandle RetryHandle;
		CurrentWorld->GetTimerManager().SetTimer(RetryHandle, RetryDelegate, Delay, false);
	}
}

class FLeft4DeadDesertCityModule : public FDefaultGameModuleImpl
{
public:
	virtual void StartupModule() override
	{
		FDefaultGameModuleImpl::StartupModule();

		FCoreDelegates::GetOnPostEngineInit().AddRaw(this, &FLeft4DeadDesertCityModule::HandlePostEngineInit);
		FWorldDelegates::OnPostWorldInitialization.AddRaw(this, &FLeft4DeadDesertCityModule::HandlePostWorldInitialization);
	}

	virtual void ShutdownModule() override
	{
		FWorldDelegates::OnPostWorldInitialization.RemoveAll(this);
		FCoreDelegates::GetOnPostEngineInit().RemoveAll(this);

		FDefaultGameModuleImpl::ShutdownModule();
	}

private:
	void HandlePostEngineInit()
	{
		Left4DeadWindowDefaults::Apply(nullptr);
	}

	void HandlePostWorldInitialization(UWorld* CurrentWorld, const UWorld::InitializationValues)
	{
		if (!CurrentWorld || !CurrentWorld->IsGameWorld())
		{
			return;
		}

		Left4DeadWindowDefaults::Apply(CurrentWorld);
		Left4DeadWindowDefaults::ScheduleRetry(CurrentWorld, 0.2f);
		Left4DeadWindowDefaults::ScheduleRetry(CurrentWorld, 1.0f);
	}
};

IMPLEMENT_PRIMARY_GAME_MODULE(FLeft4DeadDesertCityModule, Left4Dead_DesertCity, "Left4Dead_DesertCity");
