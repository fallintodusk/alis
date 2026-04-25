// Copyright ALIS. All Rights Reserved.
//
// Consumed by drag-E2E automation tests in
//   Plugins/Test/ProjectIntegrationTests/.../Integration/*
// (InventoryDragE2ESyntheticInputTests, InventoryCellDropTargetE2ETests,
//  InventoryDragEventBusTests).
//
// Problem: several tests rebuild an identical painted-grid fixture -
// UDragBusTestHostWidget + FInventoryPanelGridBuilder + AddToViewport +
// ForceLayoutPrepass + 30-frame latent paint wait. That boilerplate was
// duplicated across three files and made the tests longer than the
// interesting assertions they contain.
//
// Fix: one builder struct. Tests that want a painted viewport-visible
// grid call FInventoryDragE2EFixture::BuildPainted. Tests that only
// want the tag identity (source surface not meant to paint) call
// BuildTagOnly. Teardown is RAII via the destructor; the helper calls
// RemoveFromParent() for viewport-visible hosts so no test has to
// remember to clean up.

#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "Math/IntPoint.h"
#include "Templates/SharedPointer.h"
#include "UObject/ObjectPtr.h"

class UBorder;
class UGameInstance;
class UProjectGridCell;
class UTextBlock;
class UUniformGridPanel;
class UW_InventoryCellDropTarget;
class UUserWidget;

#if WITH_DEV_AUTOMATION_TESTS

struct FInventoryDragE2EFixture
{
    // Host widget (UDragBusTestHostWidget) that owns the grid.
    // Strong ref kept in TObjectPtr so GC doesn't reap mid-test.
    TObjectPtr<UUserWidget> Host = nullptr;

    // Root of the host widget tree - the grid the builder produced.
    UUniformGridPanel* Grid = nullptr;

    // Per-cell UW_InventoryCellDropTarget wrappers. Index order matches
    // the row-major order FInventoryPanelGridBuilder::BuildGrid uses.
    TArray<TObjectPtr<UW_InventoryCellDropTarget>> CellHosts;

    // Surface identity the caller wanted.
    FGameplayTag SurfaceTag;
    FIntPoint Dims = FIntPoint(0, 0);

    // Whether the host is currently attached to the viewport. Set by
    // BuildPainted() / BuildTagOnly(); consulted by the destructor.
    bool bInViewport = false;

    FInventoryDragE2EFixture() = default;

    // Move-only so tests can safely carry the fixture into latent commands.
    FInventoryDragE2EFixture(const FInventoryDragE2EFixture&) = delete;
    FInventoryDragE2EFixture& operator=(const FInventoryDragE2EFixture&) = delete;

    FInventoryDragE2EFixture(FInventoryDragE2EFixture&& Other) noexcept
        : Host(Other.Host)
        , Grid(Other.Grid)
        , CellHosts(MoveTemp(Other.CellHosts))
        , SurfaceTag(Other.SurfaceTag)
        , Dims(Other.Dims)
        , bInViewport(Other.bInViewport)
    {
        Other.Host = nullptr;
        Other.Grid = nullptr;
        Other.bInViewport = false;
    }

    FInventoryDragE2EFixture& operator=(FInventoryDragE2EFixture&& Other) noexcept
    {
        if (this != &Other)
        {
            ReleaseHostFromViewport();
            Host = Other.Host;
            Grid = Other.Grid;
            CellHosts = MoveTemp(Other.CellHosts);
            SurfaceTag = Other.SurfaceTag;
            Dims = Other.Dims;
            bInViewport = Other.bInViewport;
            Other.Host = nullptr;
            Other.Grid = nullptr;
            Other.bInViewport = false;
        }
        return *this;
    }

    ~FInventoryDragE2EFixture()
    {
        ReleaseHostFromViewport();
    }

    // Build a painted-and-attached grid fixture. The host is added to
    // the viewport and force-prepassed so Slate starts laying it out on
    // the next frame; callers still need to pump ~30 frames via a latent
    // command before GetCachedGeometry() is non-zero.
    //
    // Returns false if construction failed (e.g. GameInstance unavailable
    // or grid builder rejected the dims).
    static bool BuildPainted(
        UGameInstance* GI,
        const FGameplayTag& SurfaceTag,
        int32 GridW,
        int32 GridH,
        FInventoryDragE2EFixture& Out);

    // Build a grid that carries the tag identity ONLY - host is NOT added
    // to the viewport and its grid geometry stays zero. Use for the SOURCE
    // surface in cross-grid drag tests where only the target needs to
    // resolve paint.
    static bool BuildTagOnly(
        UGameInstance* GI,
        const FGameplayTag& SurfaceTag,
        int32 GridW,
        int32 GridH,
        FInventoryDragE2EFixture& Out);

    // Explicit teardown hook. Safe to call multiple times; idempotent.
    void ReleaseHostFromViewport();

private:
    static bool BuildCommon(
        UGameInstance* GI,
        const FGameplayTag& SurfaceTag,
        int32 GridW,
        int32 GridH,
        bool bAddToViewport,
        FInventoryDragE2EFixture& Out);
};

#endif // WITH_DEV_AUTOMATION_TESTS
