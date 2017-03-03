// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "Alexandria.h"
#include "Kismet/HeadMountedDisplayFunctionLibrary.h"
#include "AlexandriaCharacter.h"
#include "PrecomputedLightVolume.h"
#include "Components/LightComponent.h"
#include "Runtime/Engine/Classes/Engine/Engine.h"
#include "Runtime/Engine/Classes/Engine/Light.h"
#include "Runtime/Engine/Classes/Engine/DirectionalLight.h"
#include "Runtime/Engine/Classes/Engine/PointLight.h"
#include "Runtime/Engine/Public/SceneManagement.h"
#include "Engine/LocalPlayer.h"
#include "EngineUtils.h"
#include "DrawDebugHelpers.h"
#include "Engine/LevelBounds.h"
#include "CollisionQueryParams.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "ParticleHelper.h"
#include "Particles/ParticleSystem.h"
#include "Particles/ParticleSystemComponent.h"

#define print_color(text, time, color) if (GEngine) GEngine->AddOnScreenDebugMessage(-1, time, color, text)
#define print(text) if (GEngine) GEngine->AddOnScreenDebugMessage(-1, 1.5, FColor::White, text )
//////////////////////////////////////////////////////////////////////////
// AAlexandriaCharacter

const FName AAlexandriaCharacter::EmissiveStrName( TEXT( "EmissiveStrength" ) );
const FName AAlexandriaCharacter::MatOpacityName( TEXT( "Opacity" ) );

AAlexandriaCharacter::AAlexandriaCharacter():
	bInnerRadiance(false),
	InnerRadianceDecayTime(4.f),
	Lucidity(0.f),
	SunIntensity(0.f),
	AbsorbVelocity(1.5f), 
	ConsumeVelocity(3.f),
	SunlightTemperature( 1850.f, 5750.f )

{
	

	// Capsule Component
	{
		
		// Set size for collision capsule
		SetRootComponent( GetCapsuleComponent() );
		GetCapsuleComponent()->InitCapsuleSize( 42.f, 96.0f );
		
		//SetRootComponent( GetCapsuleComponent() );
		PrimaryActorTick.bCanEverTick = true;
	}

	// Radiance Setup
	// Setup Light
	{
		static const FName SMSocketName( TEXT( "BackSocket" ) );
		RadianceLight = CreateDefaultSubobject<UPointLightComponent>( TEXT( "RadianceLight" ) );
		RadianceLight->SetupAttachment( GetMesh(), SMSocketName );
	}

	// Setup Globe
	{
		RadianceGlobe = CreateDefaultSubobject<UStaticMeshComponent>( TEXT( "RadianceGlobe" ) );
		static ConstructorHelpers::FObjectFinder<UMaterial> RadMatRef( TEXT( "Material'/Game/ThirdPersonCPP/Meshes/RadianceGlobeMaterial.RadianceGlobeMaterial'" ) );
		if (RadMatRef.Succeeded()) {
			RadianceMaterial = RadMatRef.Object;
		}
		static ConstructorHelpers::FObjectFinder<UStaticMesh> StaticRadianceGlobe( TEXT( "StaticMesh'/Game/ThirdPersonCPP/Meshes/RadianceGlobeSphere.RadianceGlobeSphere'" ) );
		if (StaticRadianceGlobe.Succeeded()) {
			RadianceGlobeMesh = StaticRadianceGlobe.Object;

			
			RadianceGlobe->SetStaticMesh( RadianceGlobeMesh );
			RadianceGlobe->bOwnerNoSee = false;
			RadianceGlobe->bCastDynamicShadow = true;
			RadianceGlobe->CastShadow = true;
			RadianceGlobe->SetSimulatePhysics( false );
			RadianceGlobe->BodyInstance.SetObjectType( ECollisionChannel::ECC_WorldDynamic );
			RadianceGlobe->BodyInstance.SetCollisionEnabled( ECollisionEnabled::QueryOnly );
			RadianceGlobe->BodyInstance.SetResponseToAllChannels( ECollisionResponse::ECR_Ignore );
			RadianceGlobe->BodyInstance.SetResponseToChannel( ECollisionChannel::ECC_Pawn, ECollisionResponse::ECR_Block );
			RadianceGlobe->BodyInstance.SetResponseToChannel( ECollisionChannel::ECC_WorldStatic, ECollisionResponse::ECR_Block );
			RadianceGlobe->BodyInstance.SetResponseToChannel( ECollisionChannel::ECC_WorldDynamic, ECollisionResponse::ECR_Block );
			RadianceGlobe->Mobility = EComponentMobility::Movable;
			RadianceGlobe->SetHiddenInGame( false );
			RadianceGlobe->SetupAttachment( RadianceLight );
			RadianceGlobe->bAutoRegister = true;
		}
	}

	// Setup Emitter
	
	{
		RadianceFire = CreateDefaultSubobject<UParticleSystemComponent>( TEXT( "RadianceFire" ) );
		static ConstructorHelpers::FObjectFinder<UParticleSystem> FireEmitter( TEXT( "ParticleSystem'/Game/StarterContent/Particles/P_Fire.P_Fire'" ) );
		if (FireEmitter.Succeeded()) {
			RadianceFireEmitter = FireEmitter.Object;
			RadianceFire->SetTemplate( RadianceFireEmitter );
			RadianceFire->bAutoActivate = true;
			RadianceFire->bAutoRegister = true;
			RadianceFire->bVisible = true;
			RadianceFire->bVisualizeComponent = true;
			static const FName CenterSocket( TEXT( "Center" ) );
			RadianceFire->SetupAttachment( RadianceGlobe, CenterSocket );
			//RadianceFire->bAutoActivate = HasInnerRadiance();
		}
			
	}
	

	// FLucidMovement Base values
	{
		LucidAirControl.Base = 0.2f;
		LucidGravity.Base = GetCharacterMovement()->GravityScale;
		LucidMoveSpeed.Base = GetCharacterMovement()->MaxWalkSpeed;
		LucidAcceleration.Base = GetCharacterMovement()->MaxAcceleration;
		LucidLateralAirFriction.Base = GetCharacterMovement()->FallingLateralFriction;
		LucidJumpZ.Base = 600.f;
		SunlightIntensity.Base = 2500.f;
		SunlightTemperature.Base = 1.f;

		RadianceGlobe->GetMaterial( 0 )->GetScalarParameterValue( MatOpacityName, MaterialOpacity.Base );
		RadianceGlobe->GetMaterial( 0 )->GetScalarParameterValue( EmissiveStrName, EmissiveStrength.Base );
	}

	
	// Player Input/Controls
	{
		// set our turn rates for input
		BaseTurnRate = 45.f;
		BaseLookUpRate = 45.f;

		// Don't rotate when the controller rotates. Let that just affect the camera.
		bUseControllerRotationPitch = false;
		bUseControllerRotationYaw = false;
		bUseControllerRotationRoll = false;

	}

	// Player Movement
	{
		// Configure character movement
		GetCharacterMovement()->bOrientRotationToMovement = true; // Character moves in the direction of input...	
		GetCharacterMovement()->RotationRate = FRotator( 0.0f, 540.0f, 0.0f ); // ...at this rotation rate
		GetCharacterMovement()->JumpZVelocity = LucidJumpZ.Base;
		GetCharacterMovement()->AirControl = LucidAirControl.Base;
		GetCharacterMovement()->BrakingDecelerationFalling = GetCharacterMovement()->BrakingDecelerationWalking / 2.f;
		GetCharacterMovement()->bForceMaxAccel = false;
	}

	// Camera Setup
	{
		// Create a camera boom (pulls in towards the player if there is a collision)
		CameraBoom = CreateDefaultSubobject<USpringArmComponent>( TEXT( "CameraBoom" ) );
		CameraBoom->SetupAttachment( RootComponent );
		CameraBoom->TargetArmLength = 300.0f; // The camera follows at this distance behind the character	
		CameraBoom->bUsePawnControlRotation = true; // Rotate the arm based on the controller

		// Create a follow camera
		FollowCamera = CreateDefaultSubobject<UCameraComponent>( TEXT( "FollowCamera" ) );
		FollowCamera->SetupAttachment( CameraBoom, USpringArmComponent::SocketName ); // Attach the camera to the end of the boom and let the boom adjust to match the controller orientation
		FollowCamera->bUsePawnControlRotation = false; // Camera does not rotate relative to arm
	}

	SunColor = RadianceLight->GetLightColor();



	// Note: The skeletal mesh and anim blueprint references on the Mesh component (inherited from Character) 
	// are set in the derived blueprint asset named MyCharacter (to avoid direct content references in C++)
}

//////////////////////////////////////////////////////////////////////////
// Input

void AAlexandriaCharacter::SetupPlayerInputComponent(class UInputComponent* PlayerInputComponent)
{
	
	// Set up gameplay key bindings
	check(PlayerInputComponent);
	PlayerInputComponent->BindAction("Jump", IE_Pressed, this, &ACharacter::Jump);
	PlayerInputComponent->BindAction("Jump", IE_Released, this, &ACharacter::StopJumping);

	PlayerInputComponent->BindAxis("MoveForward", this, &AAlexandriaCharacter::MoveForward);
	PlayerInputComponent->BindAxis("MoveRight", this, &AAlexandriaCharacter::MoveRight);

	// We have 2 versions of the rotation bindings to handle different kinds of devices differently
	// "turn" handles devices that provide an absolute delta, such as a mouse.
	// "turnrate" is for devices that we choose to treat as a rate of change, such as an analog joystick
	PlayerInputComponent->BindAxis("Turn", this, &APawn::AddControllerYawInput);
	PlayerInputComponent->BindAxis("TurnRate", this, &AAlexandriaCharacter::TurnAtRate);
	PlayerInputComponent->BindAxis("LookUp", this, &APawn::AddControllerPitchInput);
	PlayerInputComponent->BindAxis("LookUpRate", this, &AAlexandriaCharacter::LookUpAtRate);

	// handle touch devices
	PlayerInputComponent->BindTouch(IE_Pressed, this, &AAlexandriaCharacter::TouchStarted);
	PlayerInputComponent->BindTouch(IE_Released, this, &AAlexandriaCharacter::TouchStopped);

	// VR headset functionality
	PlayerInputComponent->BindAction("ResetVR", IE_Pressed, this, &AAlexandriaCharacter::OnResetVR);
}

float AAlexandriaCharacter::CalcLucidity( const float DeltaSeconds )
{
	// Data to be collected
	FSHVectorRGB3 Radiance;
	float Shadowing = 0.f;
	float Weight = 0.f;
	FVector SkyBent = FVector::ZeroVector;
	const FBoxSphereBounds PlayerBounds( GetCapsuleComponent()->Bounds );

	// Get Lucidity from Light Levels affecting player
	const float TickLucidity = (GetSolarIllumination( 4 ) + CalcDynamicLightRadiance( 4 )) / SunIntensity;
	float DeltaLucidity = TickLucidity - GetLucidity();
	const float MaxDeltaLucidity = SunIntensity*DeltaSeconds / SunIntensity;
	
	if (DeltaLucidity > 0.f)
	{
		DeltaLucidity *= AbsorbVelocity;
		DeltaLucidity = FMath::Min<float>( DeltaLucidity, MaxDeltaLucidity );
	}
	else if (DeltaLucidity < 0.f)
	{
		DeltaLucidity *= ConsumeVelocity;
		DeltaLucidity = -1.f*FMath::Min<float>( FMath::Abs<float>(DeltaLucidity), MaxDeltaLucidity );
		if (HasInnerRadiance()) {
			DeltaLucidity /= InnerRadianceDecayTime;
		}
	}
	return FMath::Clamp<float>( GetLucidity()+DeltaLucidity, 0.f, 1.f );
}

void AAlexandriaCharacter::UpdateMovementParams( const float DeltaSeconds )
{
	UCharacterMovementComponent *Mvmt = GetCharacterMovement();
	// Apply Scalar
	Mvmt->AirControl = LucidAirControl.GetProperty(Lucidity);
	Mvmt->MaxAcceleration = LucidAcceleration.GetProperty( Lucidity );
	Mvmt->MaxWalkSpeed = LucidMoveSpeed.GetProperty(Lucidity);
	Mvmt->GravityScale = LucidGravity.GetProperty( Lucidity );
	Mvmt->FallingLateralFriction = LucidLateralAirFriction.GetProperty( Lucidity );
	Mvmt->JumpZVelocity = LucidJumpZ.GetProperty( Lucidity );

	// Apply Inv Scalar
	

	// Debug Prints
	/*
	print_color( FString::Printf( TEXT( "MaxWalkSpeed:    %f" ), Mvmt->MaxWalkSpeed ), DeltaSeconds, FColor::White );
	print_color( FString::Printf( TEXT( "MaxAccelaration: %f" ), Mvmt->MaxAcceleration ), DeltaSeconds, FColor::White );
	print_color( FString::Printf( TEXT( "JumpZVelocity:   %f" ), Mvmt->JumpZVelocity ), DeltaSeconds, FColor::White );
	print_color( FString::Printf( TEXT( "GravityScale:    %f" ), Mvmt->GravityScale ), DeltaSeconds, FColor::White );
	print_color( FString::Printf( TEXT( "AirControl:      %f" ), Mvmt->AirControl ), DeltaSeconds, FColor::White );
	*/

}

void AAlexandriaCharacter::UpdateVisualFeedback( const float DeltaSeconds )
{
	//GetRadianceLight()->SetLightColor( SunColor*Lucidity );
	GetRadianceLight()->SetTemperature( SunlightTemperature.GetProperty( Lucidity ) );
	RadianceGlobe->SetScalarParameterValueOnMaterials( MatOpacityName, MaterialOpacity.GetProperty( Lucidity ) );
	RadianceGlobe->SetScalarParameterValueOnMaterials( EmissiveStrName, EmissiveStrength.GetProperty( Lucidity ) );
	RadianceGlobe->GetMaterial( 0 )->SetEmissiveBoost( Lucidity );
	RadianceGlobe->GetMaterial( 0 )->SetDiffuseBoost( Lucidity );

	if (HasInnerRadiance()) {
		if (!RadianceFire->IsActive() && (Lucidity > SMALL_NUMBER)) {
			RadianceFire->ActivateSystem( false );
		}
		else if (RadianceFire->IsActive() && (Lucidity <= SMALL_NUMBER)) {
			RadianceFire->DeactivateSystem();
		}
		RadianceFire->SetRelativeScale3D( FVector( Lucidity ));
	}
}


void AAlexandriaCharacter::OnResetVR()
{
	UHeadMountedDisplayFunctionLibrary::ResetOrientationAndPosition();
}

void AAlexandriaCharacter::TouchStarted(ETouchIndex::Type FingerIndex, FVector Location)
{
		Jump();
}

void AAlexandriaCharacter::TouchStopped(ETouchIndex::Type FingerIndex, FVector Location)
{
		StopJumping();
}

void AAlexandriaCharacter::TurnAtRate(float Rate)
{
	// calculate delta for this frame from the rate information
	AddControllerYawInput(Rate * BaseTurnRate * GetWorld()->GetDeltaSeconds());
}

void AAlexandriaCharacter::LookUpAtRate(float Rate)
{
	// calculate delta for this frame from the rate information
	AddControllerPitchInput(Rate * BaseLookUpRate * GetWorld()->GetDeltaSeconds());
}

void AAlexandriaCharacter::MoveForward(float Value)
{
	if ((Controller != nullptr) && (Value != 0.0f))
	{
		// find out which way is forward
		const FRotator Rotation = Controller->GetControlRotation();
		const FRotator YawRotation(0, Rotation.Yaw, 0);

		// get forward vector
		const FVector Direction = FRotationMatrix(YawRotation).GetUnitAxis(EAxis::X);
		AddMovementInput(Direction, Value);
	}
}

void AAlexandriaCharacter::MoveRight(float Value)
{
	if ( (Controller != nullptr) && (Value != 0.0f) )
	{
		// find out which way is right
		const FRotator Rotation = Controller->GetControlRotation();
		const FRotator YawRotation(0, Rotation.Yaw, 0);
	
		// get right vector 
		const FVector Direction = FRotationMatrix(YawRotation).GetUnitAxis(EAxis::Y);
		// add movement in that direction
		AddMovementInput(Direction, Value);
	}
}
void AAlexandriaCharacter::DebugPrintRadiance( const float DrawTime, const FVector PollPoint, const FSHVectorRGB3 &Radiance, const float Shadowing, const float Weight, const FVector &SkyBent, const float Luminance ) const
{
	if (GEngine) {
		FLinearColor IntegralRadiance = Radiance.CalcIntegral();
		const FColor TextColor = IntegralRadiance.ToFColor( true );
		GEngine->AddOnScreenDebugMessage( INDEX_NONE,
			DrawTime,
			FColor::White,
			FString::Printf( TEXT( "Poll Point: %s" ), *PollPoint.ToString() ) );

		const FString LuminanceMsg = FString::Printf( TEXT( "Luminance: %f" ), Luminance );
		GEngine->AddOnScreenDebugMessage( INDEX_NONE,
			DrawTime,
			TextColor,
			LuminanceMsg );

		GEngine->AddOnScreenDebugMessage( INDEX_NONE,
			DrawTime,
			TextColor,
			FString::Printf( TEXT( "IntegralRadiance: %s" ), *IntegralRadiance.ToString() ) );

		GEngine->AddOnScreenDebugMessage( INDEX_NONE,
			DrawTime,
			FColor::White,
			FString::Printf( TEXT( "Shadowing: %f" ), Shadowing ) );

		GEngine->AddOnScreenDebugMessage( INDEX_NONE,
			DrawTime,
			FColor::White,
			FString::Printf( TEXT( "Weight: %f" ), Weight ) );

		GEngine->AddOnScreenDebugMessage( INDEX_NONE,
			DrawTime,
			FColor::White,
			FString::Printf( TEXT( "SkyBent: %s" ), *SkyBent.ToString() ) );
	}
}

void AAlexandriaCharacter::Tick( float DeltaSeconds )
{
	Super::Tick( DeltaSeconds );

	Lucidity = CalcLucidity( DeltaSeconds );


	GetRadianceLight()->SetIntensity( SunlightIntensity.GetProperty( Lucidity ) );
	UpdateVisualFeedback( DeltaSeconds );
	UpdateMovementParams( DeltaSeconds );

	// Debug Prints
	/*
	float print_val = -1.f;
	RadianceGlobe->GetMaterial( 0 )->GetScalarParameterValue( MatOpacityName, print_val );
	print_color( FString::Printf( TEXT( "MatOpacity: %f" ), print_val ), DeltaSeconds, FColor::White );

	print_val = -1.f;
	RadianceGlobe->GetMaterial( 0 )->GetScalarParameterValue( EmissiveStrName, print_val );
	print_color( FString::Printf( TEXT( "EmissiveStr: %f" ), print_val ), DeltaSeconds, FColor::White );
	DrawDebugString( GetWorld(), 
		(FVector(GetCapsuleComponent()->Bounds.Origin.X, GetCapsuleComponent()->Bounds.Origin.Y, GetCapsuleComponent()->Bounds.Origin.Z+50.f)- GetCapsuleComponent()->Bounds.Origin), 
		FString::SanitizeFloat( GetLucidity() ), 
		this, 
		GetSun()->GetLightComponent()->GetLightColor().ToFColor(false), 
		DeltaSeconds, 
		false );
		*/
	//DebugPrintRadiance( DeltaSeconds, PlayerBounds.GetBox().GetCenter(), Radiance, Shadowing, Weight, SkyBent, GetLucidity() );

}

void AAlexandriaCharacter::PostInitializeComponents()
{
	Super::PostInitializeComponents();
	if (RadianceMaterial == nullptr)
	{
		return;
	}
	RadianceMaterialInst = UMaterialInstanceDynamic::Create(RadianceMaterial, this, FName(TEXT("DynamicRadianceInst") ));
	RadianceGlobe->SetMaterial( 0, RadianceMaterialInst );

	if (HasInnerRadiance()) {
		RadianceFire->ActivateSystem();
	}
	else {
		RadianceFire->DeactivateSystem();
	}
}

void AAlexandriaCharacter::BeginPlay()
{
	Super::BeginPlay();
	if (Sun == nullptr)
	{
		TActorIterator<ADirectionalLight> DLightItr( GetWorld() );
		if (DLightItr) {
			Sun = *DLightItr;
		}
	}
}

ULocalPlayer* AAlexandriaCharacter::GetLocalPlayer() const
{
	ULocalPlayer* LocPlayer = nullptr;
	if (GetController()->IsLocalPlayerController())
	{
		LocPlayer = Cast<APlayerController, AController>(GetController())->GetLocalPlayer();
	}
	return LocPlayer;
}

FSceneView* AAlexandriaCharacter::GetPlayerSceneView( ULocalPlayer* LocPlayer )
{
	if (LocPlayer != nullptr)
	{
		FSceneViewFamilyContext ViewFamily( FSceneViewFamily::ConstructionValues(
			LocPlayer->ViewportClient->Viewport,
			LocPlayer->GetWorld()->Scene,
			LocPlayer->ViewportClient->EngineShowFlags ).SetRealtimeUpdate( true ) );
		FVector ViewLocation;
		FRotator ViewRotation;

		return LocPlayer->CalcSceneView( &ViewFamily, ViewLocation, ViewRotation, LocPlayer->ViewportClient->Viewport );
	}
	return nullptr;
}

float AAlexandriaCharacter::CalcPlayerIncidentRadiance( const FBoxSphereBounds &Bounds, FSHVectorRGB3 &Radiance, float &Shadowing, float &Weight, FVector &SkyBent ) const
{
	const FLevelCollection* const ActiveLevels = GetWorld()->GetActiveLevelCollection();
	if (ActiveLevels == nullptr)
	{
		return 0.f;
	}
	const FVector PollPoint = Bounds.GetBox().GetCenter();

	TArray<FSHVectorRGB3> RadianceArray;
	TArray<float> WeightArray;
	TArray<float> ShadowingArray;
	TArray<float> LuminanceArray;
	TArray<FVector> SkyBentArray;
	const FSHVector3 Ambient = FSHVector3::AmbientFunction();

	FSHVectorRGB3 LocRadiance;
	float LocWeight = 0.f;
	float LocShadowing = 0.f;
	float LocLuminance = 0.f;
	FVector LocSkyBent = FVector::ZeroVector;	
	
	for (TSet<ULevel*>::TConstIterator LevelIter( ActiveLevels->GetLevels().CreateConstIterator() );
		LevelIter; ++LevelIter)
	{
		ULevel* Level = *LevelIter;
		FPrecomputedLightVolume* PLV = Level->PrecomputedLightVolume;
		PLV->InterpolateIncidentRadiancePoint( PollPoint, LocWeight, LocShadowing, LocRadiance, LocSkyBent );
	}
	if (LocWeight != 0.f)
	{
		const float InvWeight = 1.f / LocWeight;
		LocRadiance *= InvWeight;
		LocShadowing *= InvWeight;
	}
	LocLuminance = Dot( LocRadiance, Ambient ).GetLuminance();
	//LocLuminance = LocRadiance.GetLuminance().CalcIntegral();

	Radiance += LocRadiance;
	SkyBent += LocSkyBent;
	Weight += LocWeight;
	Shadowing += LocShadowing;
	return LocLuminance;
}



float AAlexandriaCharacter::CalcDynamicLightRadiance( const int32 AvailableTraces ) const
{
	float Intensity = 0.f;
	int32 TraceCount = 0;
	for (TActorIterator<APointLight> LightIter( GetWorld(), APointLight::StaticClass() ); LightIter; ++LightIter)
	{
		if (TraceCount >= AvailableTraces)
		{
			break;
		}
		APointLight *Light = *LightIter;
		if ((Light == nullptr) || (Light->GetLightComponent()->AffectsPrimitive( GetMesh() ) == false))
		{
			continue;
		}

		//Get Light info for calculating effect on player
		ULightComponent *LightComp = Light->GetLightComponent();
		const float LRadius = FMath::Abs<float>( LightComp->SceneProxy->GetRadius() );
		if (FMath::Abs( LRadius ) < SMALL_NUMBER)
		{
			// Something not right here...
			continue;
		}
		const FVector LightPos = FVector( LightComp->GetLightPosition() );
		const float DistanceToPlayer = FMath::Abs<float>( FVector::Dist( LightPos, GetActorLocation() ) );
		const float LIntensity = LightComp->ComputeLightBrightness();

		const float effect = (LIntensity *(DistanceToPlayer / FMath::Max<float>( LRadius, DistanceToPlayer )));
		Intensity += effect;
		++TraceCount;
	}
	if (TraceCount > 0)
	{
		return Intensity / (float)TraceCount;
	}
	return Intensity;

}

float AAlexandriaCharacter::GetSolarIllumination( const int32 AvailableTraces )
{
	if (GetSun() == nullptr) {
		return 0.f;
	}
	
	// return value
	float Solarity = 0.f;
	int32 TraceCount = 0;
	//Get Light info for calculating effect on player
	ULightComponent *LightComp = GetSun()->GetLightComponent();
	const FVector LightPos( LightComp->GetLightPosition() );
	const float Intensity = LightComp->ComputeLightBrightness();

	

	// Update SunlightIntensity
	SunIntensity = Intensity;
	RadianceColor = LightComp->GetLightColor();

	// Get ray traces projected onto plane
	FVector Plane( LightComp->GetDirection() );
	if (!Plane.IsNormalized())
	{
		Plane.Normalize();
	}
	const FVector InvPlane( Plane*-1.f );
	for (int32 i = 0; i < AvailableTraces; i++)
	{
		// Seed start position
		FVector Start( LightPos );
		FVector End( GetPollPoint() );
		FVector EndVector( End - GetActorLocation() );

		// Project End onto inverse plane
		float cs = FVector::DotProduct( EndVector.GetSafeNormal(), InvPlane );
		if (cs > SMALL_NUMBER)
		{
			End = End + InvPlane*(FVector::DotProduct( EndVector, InvPlane ) / cs);
		}

		float t = 0.f;
		// Get the projected starting point on the plane from the End point
		FVector EndStartVec( Start - End );
		cs = FVector::DotProduct( EndStartVec.GetSafeNormal(), Plane );
		if (cs > SMALL_NUMBER)
		{
			t = FVector::DotProduct( EndStartVec, Plane ) / cs;
			Start = End + (InvPlane*t);
		}
		
		/*
		t = 0.f;
		FVector StartEndVec( End - Start );
		cs = FVector::DotProduct( StartEndVec.GetSafeNormal(), InvPlane );
		if (cs > SMALL_NUMBER)
		{
			t = (FVector::DotProduct( StartEndVec, InvPlane ) / cs)* 0.75f;
			End = Start + (Plane*t);
		}
		*/
		

		FVector HitLocation = FVector::ZeroVector;
		FVector HitNormal = FVector::ForwardVector;
		FHitResult Result = FHitResult( ForceInit );
		FCollisionQueryParams CollQParms = FCollisionQueryParams::DefaultQueryParam;
		CollQParms.bTraceComplex = true;
		bool bHitPlayer = GetWorld()->LineTraceSingleByProfile( Result, Start, End, UCollisionProfile::BlockAll_ProfileName, CollQParms );
		AActor *HitActor = Result.GetActor();
		if ((HitActor == nullptr) || (HitActor == this))
		{
			Solarity += Intensity;
			//DrawDebugLine( GetWorld(), Start, End, LightComp->GetLightColor().ToFColor( false ), false, GetWorld()->GetDeltaSeconds()*FMath::FRandRange(1.f, 5.f) );

		}

	}

	if (AvailableTraces > 0)
	{
		return Solarity / (float)AvailableTraces;
	}
	return Solarity;
}

FVector AAlexandriaCharacter::GetPollPoint() const
{
	FVector PollPoint( GetActorLocation() );
	PollPoint.X += FMath::FRandRange( -50.f, 50.f );
	PollPoint.Y += FMath::FRandRange( -50.f, 50.f );
	PollPoint.Z += FMath::FRandRange( -100.f, 100.f );

	return PollPoint;
}


