#include "Left4DeadGameInstance.h"

#include "Engine/Engine.h"
#include "GameFramework/GameUserSettings.h"
#include "TimerManager.h"

void ULeft4DeadGameInstance::Init()
{
	Super::Init();
	ApplyPackagedWindowDefaults();
}

void ULeft4DeadGameInstance::OnStart()
{
	Super::OnStart();
	ApplyPackagedWindowDefaults();

	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().SetTimerForNextTick(
			FTimerDelegate::CreateUObject(this, &ULeft4DeadGameInstance::ApplyPackagedWindowDefaults));
	}
}

void ULeft4DeadGameInstance::ApplyPackagedWindowDefaults()
{
#if WITH_EDITOR
	if (GIsEditor)
	{
		return;
	}
#endif

	if (!GEngine || IsRunningDedicatedServer())
	{
		return;
	}

	UGameUserSettings* Settings = GEngine->GetGameUserSettings();
	if (!Settings)
	{
		return;
	}

	const FIntPoint WindowResolution(1920, 1080);
	UE_LOG(LogTemp, Log, TEXT("Applying packaged window defaults: 1920x1080 Windowed."));
	Settings->SetScreenResolution(WindowResolution);
	Settings->SetFullscreenMode(EWindowMode::Windowed);
	Settings->ConfirmVideoMode();
	Settings->ApplySettings(false);
	Settings->SaveSettings();

	if (UWorld* World = GetWorld())
	{
		GEngine->Exec(World, TEXT("r.SetRes 1920x1080w"));
	}
}
