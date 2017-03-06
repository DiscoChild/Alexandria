// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.
#pragma once
#include "GameFramework/Character.h"
#include "AlexandriaCharacter.generated.h"


//Struct for Movement Scalars for factoring Lucidity mechanic
USTRUCT()
struct FLucidMoveProperty 
{
	GENERATED_USTRUCT_BODY()

	float Base;
	UPROPERTY( EditAnywhere, BlueprintReadWrite, meta = (ClampMin = "0.0", UIMin = "0") )
	float Min;
	UPROPERTY( EditAnywhere, BlueprintReadWrite, meta = (ClampMin = "0.0", UIMin = "0") )
	float Max;
	UPROPERTY( EditAnywhere, BlueprintReadWrite )
	uint32 InverseScale:1;

	FLucidMoveProperty( const float MinimumValue = 0.5f, const float MaximumValue = 3.5f, uint32 inv = false ) :
		Base( 0.f ),
		Min( FMath::Min<float>( MinimumValue, MaximumValue ) ),
		Max( FMath::Max<float>( MinimumValue, MaximumValue ) )
	{}
	float GetProperty( const float LucidValue ) const {
		return (InverseScale == true) ?
			(Base / (Min + LucidValue*(Max - Min))) :
			(Base*(Min + LucidValue*(Max - Min)));
	}
};

UCLASS(config=Game)
class AAlexandriaCharacter : public ACharacter
{
	GENERATED_BODY()
	/** Camera boom positioning the camera behind the character */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = Camera, meta = (AllowPrivateAccess = "true"))
	class USpringArmComponent* CameraBoom;

	/** Follow camera */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = Camera, meta = (AllowPrivateAccess = "true"))
	class UCameraComponent* FollowCamera;

	UPROPERTY( VisibleAnywhere, BlueprintReadWrite, Category = "Lucidity (Radiance)", meta = (AllowPrivateAccess = "true") )
	class UPointLightComponent* RadianceLight;

	UPROPERTY( VisibleAnywhere, BlueprintReadWrite, Category = "Lucidity (Radiance)", meta = (AllowPrivateAccess = "true") )
	class UStaticMeshComponent* RadianceGlobe;

	class UStaticMesh* RadianceGlobeMesh;
	
	class UMaterial* RadianceMaterial;
	
	class UMaterialInstanceDynamic* RadianceMaterialInst;

	class UParticleSystem* RadianceFireEmitter;

	UPROPERTY( VisibleAnywhere, BlueprintReadWrite, Category = "Lucidity (Radiance)", meta = (AllowPrivateAccess = "true") )
	class UParticleSystemComponent* RadianceFire;
	

protected:
	

public:
	AAlexandriaCharacter();

	/** Base turn rate, in deg/sec. Other scaling may affect final turn rate. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category=Camera)
	float BaseTurnRate;

	/** Base look up/down rate, in deg/sec. Other scaling may affect final rate. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category=Camera)
	float BaseLookUpRate;

protected:
	UPROPERTY( Category = "Lucidity (Radiance)", EditAnywhere, BlueprintReadOnly )
	class ADirectionalLight* Sun;


	UPROPERTY( Category = "Lucidity (Radiance)", EditAnywhere, BlueprintReadWrite )
	FLinearColor RadianceColor;

	UPROPERTY( Category = "Lucidity (Movement)", EditAnywhere, BlueprintReadWrite)
	struct FLucidMoveProperty LucidMoveSpeed;

	UPROPERTY( Category = "Lucidity (Movement)", EditAnywhere, BlueprintReadWrite)
	struct FLucidMoveProperty LucidAcceleration;

	UPROPERTY( Category = "Lucidity (Movement)", EditAnywhere, BlueprintReadWrite)
	struct FLucidMoveProperty LucidGravity;

	UPROPERTY( Category = "Lucidity (Movement)", EditAnywhere, BlueprintReadWrite)
	struct FLucidMoveProperty LucidAirControl;

	UPROPERTY( Category = "Lucidity (Movement)", EditAnywhere, BlueprintReadWrite )
	struct FLucidMoveProperty LucidJumpZ;

	UPROPERTY( Category = "Lucidity (Movement)", EditAnywhere, BlueprintReadWrite)
	struct FLucidMoveProperty LucidLateralAirFriction;

	UPROPERTY( Category = "Lucidity (Radiance)", EditAnywhere, BlueprintReadWrite )
	struct FLucidMoveProperty MaterialOpacity;

	UPROPERTY( Category = "Lucidity (Radiance)", EditAnywhere, BlueprintReadWrite )
	struct FLucidMoveProperty EmissiveStrength;

	UPROPERTY( Category = "Lucidity (Radiance)", EditAnywhere, BlueprintReadWrite )
	struct FLucidMoveProperty SunlightIntensity;

	struct FLucidMoveProperty SunlightTemperature;

	UPROPERTY( Category = "Lucidity (Radiance)", EditAnywhere, BlueprintReadWrite )
	uint32 bInnerRadiance : 1;

	UPROPERTY( Category = "Lucidity (Radiance)", EditAnywhere, BlueprintReadWrite )
	float InnerRadianceDecayTime;





	// Rate at which Lucidity decays when exposed to shadow
	UPROPERTY( Category = "Lucidity (Radiance)", EditAnywhere, BlueprintReadWrite, meta=( ClampMin = "0", UIMin = "0" ) )
	float AbsorbVelocity;

	UPROPERTY( Category = "Lucidity (Radiance)", EditAnywhere, BlueprintReadWrite, meta = (ClampMin = "0", UIMin = "0") )
	float ConsumeVelocity;



public:

	virtual void Tick( float DeltaSeconds );

	virtual void PostInitializeComponents();

	virtual void BeginPlay();

	void DebugPrintRadiance( const float DrawTime, const FVector PollPoint, const FSHVectorRGB3 &Radiance, const float Shadowing, const float Weight, const FVector &SkyBent, const float Luminance ) const;

	class ULocalPlayer* GetLocalPlayer() const;

	static FSceneView* GetPlayerSceneView( ULocalPlayer* LocPlayer );

	FORCEINLINE static float GetMoveScalar( FLucidMoveProperty LucidMove, const float LucidValue) { return LucidMove.Min + LucidValue*(LucidMove.Max - LucidMove.Min); }

protected:

	UFUNCTION( BlueprintCallable )
	void GiveInnerRadiance() { bInnerRadiance = true; }

	UFUNCTION( BlueprintCallable )
	bool HasInnerRadiance() const { return bInnerRadiance; }

	/** Resets HMD orientation in VR. */
	void OnResetVR();

	/** Called for forwards/backward input */
	void MoveForward(float Value);

	/** Called for side to side input */
	void MoveRight(float Value);

	/** 
	 * Called via input to turn at a given rate. 
	 * @param Rate	This is a normalized rate, i.e. 1.0 means 100% of desired turn rate
	 */
	void TurnAtRate(float Rate);

	/**
	 * Called via input to turn look up/down at a given rate. 
	 * @param Rate	This is a normalized rate, i.e. 1.0 means 100% of desired turn rate
	 */
	void LookUpAtRate(float Rate);

	float CalcPlayerIncidentRadiance( const FBoxSphereBounds &Bounds, FSHVectorRGB3 &Radiance, float &Shadowing, float &Weight, FVector &SkyBent ) const;

	float CalcDynamicLightRadiance( const int32 AvailableTraces ) const;

	// Updates the Sun properties and gets its current effect on the player
	float GetSolarIllumination( const int32 AvailableTraces );

	FVector GetPollPoint() const;

	//float GetLuminanceOfBlock( const FBoxSphereBounds &Bounds );

	/** Handler for when a touch input begins. */
	void TouchStarted(ETouchIndex::Type FingerIndex, FVector Location);

	/** Handler for when a touch input stops. */
	void TouchStopped(ETouchIndex::Type FingerIndex, FVector Location);

protected:
	// APawn interface
	virtual void SetupPlayerInputComponent(class UInputComponent* PlayerInputComponent) override;
	// End of APawn interface

private:

	// current amount of illumination (1.f - 0.f)
	float Lucidity;

	float SunIntensity;
	float BaseSunIntensity;

	FLinearColor SunColor;
	// Determines whether or not Lucidity is maintained when exposed to darkness.

	static const FName MatOpacityName;
	static const FName EmissiveStrName;

	float CalcLucidity( const float DeltaSeconds );
	void UpdateMovementParams( const float DeltaSeconds );
	void UpdateVisualFeedback( const float DeltaSeconds );

	

public:
	/** Returns CameraBoom subobject **/
	FORCEINLINE class USpringArmComponent* GetCameraBoom() const { return CameraBoom; }
	/** Returns FollowCamera subobject **/
	FORCEINLINE class UCameraComponent* GetFollowCamera() const { return FollowCamera; }
	
	FORCEINLINE class UPointLightComponent* GetRadianceLight() const { return RadianceLight; }
	FORCEINLINE class UStaticMeshComponent* GetRadianceGlobe() const { return RadianceGlobe; }

	FORCEINLINE class ADirectionalLight* GetSun() const { return Sun; }
	FORCEINLINE float GetLucidity() const { return Lucidity; }
	FORCEINLINE float GetAbsorbtionRate() const { return AbsorbVelocity; }
	
	
	
};





