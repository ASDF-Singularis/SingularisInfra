#pragma once

#include <CoreMinimal.h>
#include <Components/ActorComponent.h>

#include "SingularisVehicleViewComponent.generated.h"

class USpringArmComponent;

#pragma region 委托签名

/** 视角模式切换时广播，携带是否已进入载具相对模式 */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnViewModeChangedSignature, bool, bVehicleRelative);

#pragma endregion

/**
 * 引力奇点载具视角组件。
 *
 * 将乘员相机臂从"世界控制旋转"切换到"载具参考系自由视角"：
 * 相机臂继承乘员附加链（即载具）的完整旋转，鼠标输入仅作为载具本地系内的局部偏移，
 * 从而在车上保持自由视角的同时随载具俯仰、翻滚。离座时原样还原相机臂设置，不影响地面逻辑。
 */
UCLASS(
	Blueprintable,
	BlueprintType,
	ClassGroup = ("Singularis"),
	meta = (BlueprintSpawnableComponent, DisplayName = "引力奇点载具视角组件")
)
class SINGULARISINFRA_API USingularisVehicleViewComponent : public UActorComponent
{
	GENERATED_BODY()

public:
#pragma region Parameter

	/**
	 * 相机臂组件引用。
	 *
	 * 指向 Owner（乘员）上某个 SpringArmComponent；
	 * 载具相对模式下其局部旋转被本组件接管，因此该相机臂的局部旋转不应作为固定基准使用。
	 */
	UPROPERTY(
		EditAnywhere,
		BlueprintReadWrite,
		Category = "引力奇点载具视角组件",
		meta = (DisplayName = "相机臂", UseComponentPicker, AllowedClasses = "/Script/Engine.SpringArmComponent")
	)
	FComponentReference CameraBoom{};

	/** 鼠标输入的偏航灵敏度倍率 */
	UPROPERTY(
		EditAnywhere,
		BlueprintReadWrite,
		Category = "引力奇点载具视角组件",
		meta = (DisplayName = "偏航灵敏度")
	)
	float YawSensitivity = 1.0f;

	/** 鼠标输入的俯仰灵敏度倍率 */
	UPROPERTY(
		EditAnywhere,
		BlueprintReadWrite,
		Category = "引力奇点载具视角组件",
		meta = (DisplayName = "俯仰灵敏度")
	)
	float PitchSensitivity = 1.0f;

	/** 局部俯仰下限（度） */
	UPROPERTY(
		EditAnywhere,
		BlueprintReadWrite,
		Category = "引力奇点载具视角组件",
		meta = (DisplayName = "俯仰下限")
	)
	float PitchMin = -80.0f;

	/** 局部俯仰上限（度） */
	UPROPERTY(
		EditAnywhere,
		BlueprintReadWrite,
		Category = "引力奇点载具视角组件",
		meta = (DisplayName = "俯仰上限")
	)
	float PitchMax = 80.0f;

#pragma endregion

#pragma region Event Dispatcher

	/** 视角模式切换时广播 */
	UPROPERTY(
		BlueprintAssignable,
		Category = "引力奇点载具视角组件|事件分发器",
		meta = (DisplayName = "视角模式切换时")
	)
	FOnViewModeChangedSignature OnViewModeChanged{};

#pragma endregion

private:
#pragma region State

	/** 缓存的相机臂组件，由 CameraBoom 解析填充 */
	TWeakObjectPtr<USpringArmComponent> CachedBoom = nullptr;

	/** 载具本地系内的偏航偏移 */
	float LookYaw = 0.0f;

	/** 载具本地系内的俯仰偏移 */
	float LookPitch = 0.0f;

	/** 是否处于载具相对模式 */
	bool bVehicleRelative = false;

	/** 进入载具相对模式前的相机臂设置，用于离座还原 */
	bool bSavedUsePawnControlRotation = true;
	bool bSavedInheritPitch = true;
	bool bSavedInheritYaw = true;
	bool bSavedInheritRoll = true;

#pragma endregion

public:
#pragma region Constructors

	/** 默认构造函数，开启 Tick 但默认不启用（仅在载具相对模式下运行） */
	USingularisVehicleViewComponent();

#pragma endregion

#pragma region ActorComponent Interface

	/** 解析 CameraBoom 组件引用并缓存相机臂 */
	virtual void BeginPlay() override;

	/** 载具相对模式下把局部视角偏移写入相机臂 */
	virtual void TickComponent(
		float DeltaTime,
		ELevelTick TickType,
		FActorComponentTickFunction* ThisTickFunction
	) override;

#pragma endregion

#pragma region API

	/**
	 * 切换载具相对视角模式。
	 *
	 * 幂等。开启时解除相机臂的世界控制旋转并开启全轴继承，用当前局部旋转初始化偏移避免跳变；
	 * 关闭时复位偏移并还原相机臂原有设置。
	 *
	 * @param bEnable 为 true 进入载具相对模式，为 false 还原为世界控制旋转。
	 */
	UFUNCTION(
		BlueprintCallable,
		Category = "引力奇点载具视角组件|API",
		meta = (DisplayName = "SetVehicleRelative")
	)
	void SetVehicleRelative(bool bEnable);

	/**
	 * 注入偏航视角输入。
	 *
	 * 载具相对模式下累加为本地偏移；否则转发给 PlayerController，保持地面行为不变。
	 *
	 * @param Value 输入轴值。
	 */
	UFUNCTION(
		BlueprintCallable,
		Category = "引力奇点载具视角组件|API",
		meta = (DisplayName = "AddLookYaw")
	)
	void AddLookYaw(float Value);

	/**
	 * 注入俯仰视角输入。
	 *
	 * 载具相对模式下累加并夹紧为本地偏移；否则转发给 PlayerController，保持地面行为不变。
	 *
	 * @param Value 输入轴值。
	 */
	UFUNCTION(
		BlueprintCallable,
		Category = "引力奇点载具视角组件|API",
		meta = (DisplayName = "AddLookPitch")
	)
	void AddLookPitch(float Value);

#pragma endregion
};
