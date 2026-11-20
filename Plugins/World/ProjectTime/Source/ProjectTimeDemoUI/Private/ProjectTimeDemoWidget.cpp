#include "ProjectTimeDemoWidget.h"
#include "ProjectTimeDemoViewModel.h"
#include "ProjectWidgetHelpers.h"
#include "Layout/ProjectWidgetLayoutLoader.h"
#include "Components/TextBlock.h"
#include "Components/Slider.h"
#include "Components/Button.h"
#include "InputCoreTypes.h"

UProjectTimeDemoWidget::UProjectTimeDemoWidget(const FObjectInitializer& Initializer) : Super(Initializer)
{
	ConfigFilePath = UProjectWidgetLayoutLoader::GetPluginUIConfigPath(TEXT("ProjectTime"), TEXT("TimeDemo.json"));
	SetIsFocusable(true);
}

void UProjectTimeDemoWidget::BindCallbacks()
{
	Super::BindCallbacks();
	TimeLabel = UProjectWidgetHelpers::FindWidgetByNameTyped<UTextBlock>(RootWidget, TEXT("TimeLabel"), true, true);
	OffsetSlider = UProjectWidgetHelpers::FindWidgetByNameTyped<USlider>(RootWidget, TEXT("OffsetSlider"), true, true);
	RateSlider = UProjectWidgetHelpers::FindWidgetByNameTyped<USlider>(RootWidget, TEXT("RateSlider"), true, true);
	if (OffsetSlider)
	{
		OffsetSlider->OnValueChanged.AddUniqueDynamic(this, &UProjectTimeDemoWidget::OnOffset);
		OffsetSlider->OnMouseCaptureBegin.AddUniqueDynamic(this, &UProjectTimeDemoWidget::OnScrubBegin);
		OffsetSlider->OnControllerCaptureBegin.AddUniqueDynamic(this, &UProjectTimeDemoWidget::OnScrubBegin);
	}
	if (RateSlider) RateSlider->OnValueChanged.AddUniqueDynamic(this, &UProjectTimeDemoWidget::OnRate);
	if (auto* Reset = UProjectWidgetHelpers::FindWidgetByNameTyped<UButton>(RootWidget, TEXT("ResetButton"), true, true))
		Reset->OnClicked.AddUniqueDynamic(this, &UProjectTimeDemoWidget::OnReset);
	if (auto* Close = UProjectWidgetHelpers::FindWidgetByNameTyped<UButton>(RootWidget, TEXT("CloseButton"), true, true))
		Close->OnClicked.AddUniqueDynamic(this, &UProjectTimeDemoWidget::OnClose);
	RefreshFromViewModel();
}

void UProjectTimeDemoWidget::RefreshFromViewModel_Implementation()
{
	const auto* Model = Cast<UProjectTimeDemoViewModel>(GetViewModel());
	if (!Model) return;
	TGuardValue<bool> Guard(bRefreshing, true);
	if (TimeLabel) TimeLabel->SetText(Model->GetDateText());
	if (OffsetSlider) { OffsetSlider->SetValue(Model->GetOffset()); OffsetSlider->SetIsEnabled(Model->CanControl()); }
	if (RateSlider) { RateSlider->SetValue(Model->GetRate()); RateSlider->SetIsEnabled(Model->CanControl()); }
}

void UProjectTimeDemoWidget::OnOffset(float Value)
{
	if (!bRefreshing) if (auto* Model = Cast<UProjectTimeDemoViewModel>(GetViewModel())) Model->SeekHours(Value);
}
void UProjectTimeDemoWidget::OnRate(float Value)
{
	if (!bRefreshing) if (auto* Model = Cast<UProjectTimeDemoViewModel>(GetViewModel())) Model->ChangeRate(Value);
}
void UProjectTimeDemoWidget::OnScrubBegin()
{
	if (auto* Model = Cast<UProjectTimeDemoViewModel>(GetViewModel())) Model->BeginScrub();
}
void UProjectTimeDemoWidget::OnReset()
{
	if (auto* Model = Cast<UProjectTimeDemoViewModel>(GetViewModel())) Model->Reset();
}
void UProjectTimeDemoWidget::OnClose()
{
	if (auto* Model = Cast<UProjectTimeDemoViewModel>(GetViewModel())) Model->Close();
}
FReply UProjectTimeDemoWidget::NativeOnKeyDown(const FGeometry& Geometry, const FKeyEvent& Event)
{
	if (Event.GetKey() == EKeys::Escape || Event.GetKey() == EKeys::F6) { OnClose(); return FReply::Handled(); }
	return Super::NativeOnKeyDown(Geometry, Event);
}
