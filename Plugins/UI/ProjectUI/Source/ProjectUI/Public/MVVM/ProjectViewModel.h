// Copyright ALIS. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "ProjectViewModel.generated.h"

/**
 * Delegate fired when a ViewModel property changes
 * Used for data binding between ViewModel and View
 *
 * @param PropertyName - Name of the property that changed
 */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnViewModelPropertyChanged, FName, PropertyName);
DECLARE_MULTICAST_DELEGATE_OneParam(FOnViewModelPropertyChangedNative, FName);

/**
 * Base class for all ViewModels in the MVVM architecture
 *
 * ViewModels are pure C++ logic classes that:
 * - Hold UI state (data)
 * - Expose commands (functions)
 * - Notify views of changes via delegates
 * - Have zero dependencies on UMG or widget classes
 *
 * Design Goals:
 * - Agent-friendly: Pure C++ logic, no Blueprint dependencies
 * - Testable: Can be unit tested without UI framework
 * - Reusable: Same ViewModel can drive multiple view types
 * - Data-driven: Properties can be serialized/deserialized
 *
 * Usage Example:
 * @code
 * UCLASS()
 * class UMyViewModel : public UProjectViewModel
 * {
 *     GENERATED_BODY()
 * public:
 *     VIEWMODEL_PROPERTY(FText, Title)
 *     VIEWMODEL_PROPERTY(int32, ItemCount)
 *
 *     void RefreshData()
 *     {
 *         SetTitle(FText::FromString("Hello"));
 *         SetItemCount(42);
 *     }
 * };
 * @endcode
 */
UCLASS(Blueprintable, BlueprintType)
class PROJECTUI_API UProjectViewModel : public UObject
{
	GENERATED_BODY()

public:
	/**
	 * Delegate broadcast when any property changes
	 * Views bind to this to refresh their presentation
	 */
	UPROPERTY(BlueprintAssignable, Category = "ViewModel")
	FOnViewModelPropertyChanged OnPropertyChanged;

	/** Native delegate for C++ callers (lambda-friendly). */
	FOnViewModelPropertyChangedNative OnPropertyChangedNative;

	/**
	 * Initialize the ViewModel with context data
	 * Override in derived classes to set up initial state
	 *
	 * @param Context - Optional context object (e.g., game instance, subsystems)
	 */
	UFUNCTION(BlueprintCallable, Category = "ViewModel")
	virtual void Initialize(UObject* Context);

	/**
	 * Clean up the ViewModel before destruction
	 * Override in derived classes to unregister delegates, etc.
	 */
	UFUNCTION(BlueprintCallable, Category = "ViewModel")
	virtual void Shutdown();

	// UHT does not reflect UPROPERTYs inside user-defined macros (VIEWMODEL_PROPERTY).
	// Override to expose bool properties for auto_visibility without UE reflection.
	virtual bool GetBoolProperty(FName PropertyName) const;

protected:
	/**
	 * Notify views that a property has changed
	 * Called by VIEWMODEL_PROPERTY setters
	 *
	 * @param PropertyName - Name of the changed property
	 */
	UFUNCTION(BlueprintCallable, Category = "ViewModel")
	void NotifyPropertyChanged(FName PropertyName);

	/**
	 * Check if ViewModel is initialized
	 */
	UFUNCTION(BlueprintPure, Category = "ViewModel")
	bool IsInitialized() const { return bInitialized; }

private:
	/** Initialization state */
	bool bInitialized = false;
};

/**
 * Property Binding Macros
 *
 * These macros generate getter/setter pairs with automatic change notification.
 * Use them to declare ViewModel properties that views can bind to.
 *
 * Example:
 * @code
 * VIEWMODEL_PROPERTY(FText, UserName)
 * // Generates:
 * // - UPROPERTY FText UserName
 * // - FText GetUserName() const
 * // - void SetUserName(const FText& Value)
 * @endcode
 */

/**
 * Declare a ViewModel property with auto-notification helper
 * Generates getter, backing field, and Update##Name() utility that fires NotifyPropertyChanged.
 *
 * @param Type - Property type (e.g., int32, FText, FString)
 * @param Name - Property name (e.g., ItemCount, Title)
 */
#define VIEWMODEL_PROPERTY(Type, Name) \
protected: \
	UPROPERTY(BlueprintReadOnly, Category = "ViewModel") \
	Type Name; \
	void Update##Name(const Type& InValue) \
	{ \
		Name = InValue; \
		NotifyPropertyChanged(FName(TEXT(#Name))); \
	} \
public: \
	FORCEINLINE Type Get##Name() const { return Name; }

/**
 * Declare a read-only ViewModel property
 * Only generates getter, no setter (internal modification only)
 *
 * @param Type - Property type
 * @param Name - Property name
 */
#define VIEWMODEL_PROPERTY_READONLY(Type, Name) \
protected: \
	UPROPERTY(BlueprintReadOnly, Category = "ViewModel") \
	Type Name; \
public: \
	FORCEINLINE Type Get##Name() const { return Name; }

/**
 * Declare a ViewModel property for arrays
 * Generates getter returning const reference (no setter for direct array access)
 * Use AddTo##Name, RemoveFrom##Name, Clear##Name for modifications
 *
 * @param Type - Array element type
 * @param Name - Property name
 */
#define VIEWMODEL_PROPERTY_ARRAY(Type, Name) \
protected: \
	UPROPERTY(BlueprintReadOnly, Category = "ViewModel") \
	TArray<Type> Name; \
	void Update##Name(const TArray<Type>& InValue) \
	{ \
		Name = InValue; \
		NotifyPropertyChanged(FName(TEXT(#Name))); \
	} \
public: \
	FORCEINLINE const TArray<Type>& Get##Name() const { return Name; } \
	void Set##Name(const TArray<Type>& InValue) \
	{ \
		Update##Name(InValue); \
	} \
	void AddTo##Name(const Type& Item) \
	{ \
		Name.Add(Item); \
		NotifyPropertyChanged(FName(TEXT(#Name))); \
	} \
	void RemoveFrom##Name(const Type& Item) \
	{ \
		Name.Remove(Item); \
		NotifyPropertyChanged(FName(TEXT(#Name))); \
	} \
	void Clear##Name() \
	{ \
		Name.Empty(); \
		NotifyPropertyChanged(FName(TEXT(#Name))); \
	}
