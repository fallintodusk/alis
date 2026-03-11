// Copyright ALIS. All Rights Reserved.

/**
 * Unit Tests for UProjectDialogueComponent
 *
 * Tests verify tree-based conversation state management,
 * node navigation, option selection, and edge cases.
 */

#include "Misc/AutomationTest.h"
#include "Components/ProjectDialogueComponent.h"
#include "Data/DialogueTreeDefinition.h"
#include "Data/ProjectDialogueTypes.h"

#if WITH_DEV_AUTOMATION_TESTS

// ============================================================================
// Test Helpers
// ============================================================================

namespace DialogueTestHelper
{
	/**
	 * Create a simple dialogue tree for testing:
	 *   greeting -> (options) -> supplies (auto-advance -> greeting)
	 *                         -> farewell ($end)
	 */
	UDialogueTreeDefinition* CreateSimpleTree()
	{
		UDialogueTreeDefinition* Tree = NewObject<UDialogueTreeDefinition>();
		Tree->TreeId = FName(TEXT("TestTree"));
		Tree->StartNode = TEXT("greeting");

		FDialogueNode Greeting;
		Greeting.Speaker = TEXT("NPC");
		Greeting.Text = TEXT("Hello, traveler.");
		{
			FDialogueOption Opt1;
			Opt1.Text = TEXT("Tell me about supplies.");
			Opt1.Next = TEXT("supplies");
			Greeting.Options.Add(Opt1);

			FDialogueOption Opt2;
			Opt2.Text = TEXT("Goodbye.");
			Opt2.Next = TEXT("$end");
			Greeting.Options.Add(Opt2);
		}
		Tree->Nodes.Add(TEXT("greeting"), Greeting);

		FDialogueNode Supplies;
		Supplies.Speaker = TEXT("NPC");
		Supplies.Text = TEXT("Check the market stalls.");
		Supplies.Next = TEXT("greeting");
		Tree->Nodes.Add(TEXT("supplies"), Supplies);

		return Tree;
	}

	// Create a linear tree: start -> middle -> end (terminal)
	UDialogueTreeDefinition* CreateLinearTree()
	{
		UDialogueTreeDefinition* Tree = NewObject<UDialogueTreeDefinition>();
		Tree->TreeId = FName(TEXT("LinearTree"));
		Tree->StartNode = TEXT("start");

		FDialogueNode Start;
		Start.Text = TEXT("Beginning.");
		Start.Next = TEXT("middle");
		Tree->Nodes.Add(TEXT("start"), Start);

		FDialogueNode Middle;
		Middle.Text = TEXT("Middle part.");
		Middle.Next = TEXT("end_node");
		Tree->Nodes.Add(TEXT("middle"), Middle);

		FDialogueNode End;
		End.Text = TEXT("The end.");
		// No next, no options = terminal
		Tree->Nodes.Add(TEXT("end_node"), End);

		return Tree;
	}

	// Create component with tree assigned (bypasses soft ptr load)
	UProjectDialogueComponent* CreateComponentWithTree(UDialogueTreeDefinition* Tree)
	{
		UProjectDialogueComponent* Comp = NewObject<UProjectDialogueComponent>();
		// Set LoadedTree directly via reflection for testing
		FObjectProperty* Prop = CastField<FObjectProperty>(
			UProjectDialogueComponent::StaticClass()->FindPropertyByName(TEXT("LoadedTree")));
		if (Prop)
		{
			Prop->SetObjectPropertyValue_InContainer(Comp, Tree);
		}
		return Comp;
	}
}

// ============================================================================
// Data Structure Tests
// ============================================================================

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDialogueOption_IsEndOption,
	"ProjectDialogue.Data.Option.IsEndOption",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FDialogueOption_IsEndOption::RunTest(const FString& Parameters)
{
	FDialogueOption EndOpt;
	EndOpt.Text = TEXT("Goodbye");
	EndOpt.Next = TEXT("$end");
	TestTrue(TEXT("$end is end option"), EndOpt.IsEndOption());

	FDialogueOption EmptyOpt;
	EmptyOpt.Text = TEXT("Leave");
	TestTrue(TEXT("Empty next is end option"), EmptyOpt.IsEndOption());

	FDialogueOption ContinueOpt;
	ContinueOpt.Text = TEXT("Continue");
	ContinueOpt.Next = TEXT("next_node");
	TestFalse(TEXT("Node reference is not end option"), ContinueOpt.IsEndOption());

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDialogueNode_IsTerminal,
	"ProjectDialogue.Data.Node.IsTerminal",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FDialogueNode_IsTerminal::RunTest(const FString& Parameters)
{
	FDialogueNode Terminal;
	Terminal.Text = TEXT("The end.");
	TestTrue(TEXT("No options, no next = terminal"), Terminal.IsTerminal());

	FDialogueNode AutoAdvance;
	AutoAdvance.Text = TEXT("Continue...");
	AutoAdvance.Next = TEXT("next");
	TestFalse(TEXT("Has next = not terminal"), AutoAdvance.IsTerminal());

	FDialogueNode WithOptions;
	WithOptions.Text = TEXT("Choose:");
	FDialogueOption Opt;
	Opt.Text = TEXT("Option A");
	Opt.Next = TEXT("a");
	WithOptions.Options.Add(Opt);
	TestFalse(TEXT("Has options = not terminal"), WithOptions.IsTerminal());

	FDialogueNode EndNext;
	EndNext.Text = TEXT("Done.");
	EndNext.Next = TEXT("$end");
	TestTrue(TEXT("Next=$end = terminal"), EndNext.IsTerminal());

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDialogueNode_HasOptions,
	"ProjectDialogue.Data.Node.HasOptions",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FDialogueNode_HasOptions::RunTest(const FString& Parameters)
{
	FDialogueNode NoOpts;
	NoOpts.Text = TEXT("Line.");
	TestFalse(TEXT("Empty options = false"), NoOpts.HasOptions());

	FDialogueNode WithOpts;
	WithOpts.Text = TEXT("Choose:");
	FDialogueOption Opt;
	Opt.Text = TEXT("A");
	WithOpts.Options.Add(Opt);
	TestTrue(TEXT("Has options = true"), WithOpts.HasOptions());

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDialogueTreeDef_IsValid,
	"ProjectDialogue.Data.TreeDefinition.IsValid",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FDialogueTreeDef_IsValid::RunTest(const FString& Parameters)
{
	UDialogueTreeDefinition* ValidTree = DialogueTestHelper::CreateSimpleTree();
	TestTrue(TEXT("Simple tree is valid"), ValidTree->IsValid());

	UDialogueTreeDefinition* EmptyTree = NewObject<UDialogueTreeDefinition>();
	TestFalse(TEXT("Empty tree is invalid"), EmptyTree->IsValid());

	UDialogueTreeDefinition* BadStart = NewObject<UDialogueTreeDefinition>();
	BadStart->StartNode = TEXT("nonexistent");
	FDialogueNode Node;
	Node.Text = TEXT("A");
	BadStart->Nodes.Add(TEXT("real"), Node);
	TestFalse(TEXT("Tree with bad start node is invalid"), BadStart->IsValid());

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDialogueTreeDef_FindNode,
	"ProjectDialogue.Data.TreeDefinition.FindNode",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FDialogueTreeDef_FindNode::RunTest(const FString& Parameters)
{
	UDialogueTreeDefinition* Tree = DialogueTestHelper::CreateSimpleTree();

	const FDialogueNode* Found = Tree->FindNode(TEXT("greeting"));
	TestNotNull(TEXT("Found greeting node"), Found);
	TestEqual(TEXT("Greeting text matches"), Found->Text, TEXT("Hello, traveler."));

	const FDialogueNode* NotFound = Tree->FindNode(TEXT("nonexistent"));
	TestNull(TEXT("Nonexistent node returns null"), NotFound);

	return true;
}

// ============================================================================
// Component Default State Tests
// ============================================================================

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FProjectDialogueComponent_DefaultState,
	"ProjectDialogue.Component.DefaultState",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FProjectDialogueComponent_DefaultState::RunTest(const FString& Parameters)
{
	UProjectDialogueComponent* Comp = NewObject<UProjectDialogueComponent>();

	TestFalse(TEXT("Not in conversation initially"), Comp->IsInConversation());
	TestTrue(TEXT("Current text is empty"), Comp->GetCurrentText().IsEmpty());
	TestTrue(TEXT("Current speaker is empty"), Comp->GetCurrentSpeaker().IsEmpty());
	TestTrue(TEXT("Terminal when no tree"), Comp->IsCurrentNodeTerminal());
	TestFalse(TEXT("No options when no tree"), Comp->CurrentNodeHasOptions());

	return true;
}

// ============================================================================
// Conversation Lifecycle Tests
// ============================================================================

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FProjectDialogueComponent_StartConversation_NoTree,
	"ProjectDialogue.Component.Start.NoTree",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FProjectDialogueComponent_StartConversation_NoTree::RunTest(const FString& Parameters)
{
	UProjectDialogueComponent* Comp = NewObject<UProjectDialogueComponent>();
	Comp->StartConversation();
	TestFalse(TEXT("Not in conversation without tree"), Comp->IsInConversation());

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FProjectDialogueComponent_StartConversation_WithTree,
	"ProjectDialogue.Component.Start.WithTree",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FProjectDialogueComponent_StartConversation_WithTree::RunTest(const FString& Parameters)
{
	UDialogueTreeDefinition* Tree = DialogueTestHelper::CreateSimpleTree();
	UProjectDialogueComponent* Comp = DialogueTestHelper::CreateComponentWithTree(Tree);

	Comp->StartConversation();
	TestTrue(TEXT("In conversation after start"), Comp->IsInConversation());
	TestEqual(TEXT("Current node is start node"), Comp->GetCurrentNodeId(), TEXT("greeting"));
	TestEqual(TEXT("Speaker is NPC"), Comp->GetCurrentSpeaker(), TEXT("NPC"));
	TestEqual(TEXT("Text matches greeting"), Comp->GetCurrentText(), TEXT("Hello, traveler."));
	TestTrue(TEXT("Greeting has options"), Comp->CurrentNodeHasOptions());
	TestFalse(TEXT("Greeting is not terminal"), Comp->IsCurrentNodeTerminal());

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FProjectDialogueComponent_EndConversation,
	"ProjectDialogue.Component.EndConversation",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FProjectDialogueComponent_EndConversation::RunTest(const FString& Parameters)
{
	UDialogueTreeDefinition* Tree = DialogueTestHelper::CreateSimpleTree();
	UProjectDialogueComponent* Comp = DialogueTestHelper::CreateComponentWithTree(Tree);

	Comp->StartConversation();
	TestTrue(TEXT("In conversation"), Comp->IsInConversation());

	Comp->EndConversation();
	TestFalse(TEXT("Not in conversation after end"), Comp->IsInConversation());
	TestTrue(TEXT("Node ID cleared"), Comp->GetCurrentNodeId().IsEmpty());

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FProjectDialogueComponent_ConversationCycle,
	"ProjectDialogue.Component.ConversationCycle",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FProjectDialogueComponent_ConversationCycle::RunTest(const FString& Parameters)
{
	UDialogueTreeDefinition* Tree = DialogueTestHelper::CreateSimpleTree();
	UProjectDialogueComponent* Comp = DialogueTestHelper::CreateComponentWithTree(Tree);

	Comp->StartConversation();
	TestTrue(TEXT("First start"), Comp->IsInConversation());

	Comp->EndConversation();
	TestFalse(TEXT("First end"), Comp->IsInConversation());

	Comp->StartConversation();
	TestTrue(TEXT("Second start"), Comp->IsInConversation());

	Comp->EndConversation();
	TestFalse(TEXT("Second end"), Comp->IsInConversation());

	return true;
}

// ============================================================================
// Edge Case Tests
// ============================================================================

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FProjectDialogueComponent_EndWithoutStart,
	"ProjectDialogue.Component.EdgeCases.EndWithoutStart",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FProjectDialogueComponent_EndWithoutStart::RunTest(const FString& Parameters)
{
	UProjectDialogueComponent* Comp = NewObject<UProjectDialogueComponent>();
	Comp->EndConversation();
	TestFalse(TEXT("Not in conversation"), Comp->IsInConversation());

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FProjectDialogueComponent_MultipleStarts,
	"ProjectDialogue.Component.EdgeCases.MultipleStarts",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FProjectDialogueComponent_MultipleStarts::RunTest(const FString& Parameters)
{
	UDialogueTreeDefinition* Tree = DialogueTestHelper::CreateSimpleTree();
	UProjectDialogueComponent* Comp = DialogueTestHelper::CreateComponentWithTree(Tree);

	Comp->StartConversation();
	Comp->StartConversation();
	Comp->StartConversation();

	TestTrue(TEXT("Still in conversation"), Comp->IsInConversation());

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FProjectDialogueComponent_MultipleEnds,
	"ProjectDialogue.Component.EdgeCases.MultipleEnds",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FProjectDialogueComponent_MultipleEnds::RunTest(const FString& Parameters)
{
	UDialogueTreeDefinition* Tree = DialogueTestHelper::CreateSimpleTree();
	UProjectDialogueComponent* Comp = DialogueTestHelper::CreateComponentWithTree(Tree);

	Comp->StartConversation();
	Comp->EndConversation();
	Comp->EndConversation();
	Comp->EndConversation();

	TestFalse(TEXT("Not in conversation after multiple ends"), Comp->IsInConversation());

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FProjectDialogueComponent_SelectOptionWithoutStart,
	"ProjectDialogue.Component.EdgeCases.SelectOptionWithoutStart",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FProjectDialogueComponent_SelectOptionWithoutStart::RunTest(const FString& Parameters)
{
	UProjectDialogueComponent* Comp = NewObject<UProjectDialogueComponent>();
	Comp->SelectOption(0);
	TestFalse(TEXT("Not in conversation"), Comp->IsInConversation());

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FProjectDialogueComponent_AdvanceWithoutStart,
	"ProjectDialogue.Component.EdgeCases.AdvanceWithoutStart",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FProjectDialogueComponent_AdvanceWithoutStart::RunTest(const FString& Parameters)
{
	UProjectDialogueComponent* Comp = NewObject<UProjectDialogueComponent>();
	Comp->AdvanceOrEnd();
	TestFalse(TEXT("Not in conversation"), Comp->IsInConversation());

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FProjectDialogueComponent_InvalidOptionIndex,
	"ProjectDialogue.Component.EdgeCases.InvalidOptionIndex",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FProjectDialogueComponent_InvalidOptionIndex::RunTest(const FString& Parameters)
{
	UDialogueTreeDefinition* Tree = DialogueTestHelper::CreateSimpleTree();
	UProjectDialogueComponent* Comp = DialogueTestHelper::CreateComponentWithTree(Tree);

	Comp->StartConversation();
	Comp->SelectOption(99);

	TestTrue(TEXT("Still in conversation after invalid index"), Comp->IsInConversation());
	TestEqual(TEXT("Still at greeting"), Comp->GetCurrentNodeId(), TEXT("greeting"));

	return true;
}

// ============================================================================
// Option Selection Tests
// ============================================================================

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FProjectDialogueComponent_SelectOption_Navigate,
	"ProjectDialogue.Component.SelectOption.Navigate",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FProjectDialogueComponent_SelectOption_Navigate::RunTest(const FString& Parameters)
{
	UDialogueTreeDefinition* Tree = DialogueTestHelper::CreateSimpleTree();
	UProjectDialogueComponent* Comp = DialogueTestHelper::CreateComponentWithTree(Tree);

	Comp->StartConversation();
	// Select "Tell me about supplies." (index 0)
	Comp->SelectOption(0);

	TestTrue(TEXT("Still in conversation"), Comp->IsInConversation());
	TestEqual(TEXT("Navigated to supplies"), Comp->GetCurrentNodeId(), TEXT("supplies"));
	TestEqual(TEXT("Supplies text"), Comp->GetCurrentText(), TEXT("Check the market stalls."));

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FProjectDialogueComponent_SelectOption_End,
	"ProjectDialogue.Component.SelectOption.End",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FProjectDialogueComponent_SelectOption_End::RunTest(const FString& Parameters)
{
	UDialogueTreeDefinition* Tree = DialogueTestHelper::CreateSimpleTree();
	UProjectDialogueComponent* Comp = DialogueTestHelper::CreateComponentWithTree(Tree);

	Comp->StartConversation();
	// Select "Goodbye." (index 1, next=$end)
	Comp->SelectOption(1);

	TestFalse(TEXT("Conversation ended"), Comp->IsInConversation());

	return true;
}

// ============================================================================
// Auto-Advance Tests
// ============================================================================

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FProjectDialogueComponent_AdvanceOrEnd_AutoAdvance,
	"ProjectDialogue.Component.AdvanceOrEnd.AutoAdvance",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FProjectDialogueComponent_AdvanceOrEnd_AutoAdvance::RunTest(const FString& Parameters)
{
	UDialogueTreeDefinition* Tree = DialogueTestHelper::CreateSimpleTree();
	UProjectDialogueComponent* Comp = DialogueTestHelper::CreateComponentWithTree(Tree);

	Comp->StartConversation();
	Comp->SelectOption(0);
	TestEqual(TEXT("At supplies"), Comp->GetCurrentNodeId(), TEXT("supplies"));
	TestFalse(TEXT("Supplies has no options"), Comp->CurrentNodeHasOptions());
	TestFalse(TEXT("Supplies is not terminal (has next)"), Comp->IsCurrentNodeTerminal());

	Comp->AdvanceOrEnd();
	TestTrue(TEXT("Still in conversation"), Comp->IsInConversation());
	TestEqual(TEXT("Back at greeting"), Comp->GetCurrentNodeId(), TEXT("greeting"));

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FProjectDialogueComponent_AdvanceOrEnd_Terminal,
	"ProjectDialogue.Component.AdvanceOrEnd.Terminal",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FProjectDialogueComponent_AdvanceOrEnd_Terminal::RunTest(const FString& Parameters)
{
	UDialogueTreeDefinition* Tree = DialogueTestHelper::CreateLinearTree();
	UProjectDialogueComponent* Comp = DialogueTestHelper::CreateComponentWithTree(Tree);

	Comp->StartConversation();
	TestEqual(TEXT("At start"), Comp->GetCurrentNodeId(), TEXT("start"));

	Comp->AdvanceOrEnd();
	TestEqual(TEXT("At middle"), Comp->GetCurrentNodeId(), TEXT("middle"));

	Comp->AdvanceOrEnd();
	TestEqual(TEXT("At end_node"), Comp->GetCurrentNodeId(), TEXT("end_node"));
	TestTrue(TEXT("End node is terminal"), Comp->IsCurrentNodeTerminal());

	Comp->AdvanceOrEnd();
	TestFalse(TEXT("Conversation ended at terminal"), Comp->IsInConversation());

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FProjectDialogueComponent_AdvanceOrEnd_IgnoresOptions,
	"ProjectDialogue.Component.AdvanceOrEnd.IgnoresOptions",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FProjectDialogueComponent_AdvanceOrEnd_IgnoresOptions::RunTest(const FString& Parameters)
{
	UDialogueTreeDefinition* Tree = DialogueTestHelper::CreateSimpleTree();
	UProjectDialogueComponent* Comp = DialogueTestHelper::CreateComponentWithTree(Tree);

	Comp->StartConversation();
	TestTrue(TEXT("Greeting has options"), Comp->CurrentNodeHasOptions());

	Comp->AdvanceOrEnd();
	TestTrue(TEXT("Still in conversation"), Comp->IsInConversation());
	TestEqual(TEXT("Still at greeting"), Comp->GetCurrentNodeId(), TEXT("greeting"));

	return true;
}

// ============================================================================
// Visible Options Tests
// ============================================================================

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FProjectDialogueComponent_GetVisibleOptions,
	"ProjectDialogue.Component.GetVisibleOptions",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FProjectDialogueComponent_GetVisibleOptions::RunTest(const FString& Parameters)
{
	UDialogueTreeDefinition* Tree = DialogueTestHelper::CreateSimpleTree();
	UProjectDialogueComponent* Comp = DialogueTestHelper::CreateComponentWithTree(Tree);

	Comp->StartConversation();

	TArray<FDialogueOption> Options;
	Comp->GetVisibleOptions(Options);

	TestEqual(TEXT("Two visible options"), Options.Num(), 2);
	TestEqual(TEXT("First option text"), Options[0].Text, TEXT("Tell me about supplies."));
	TestEqual(TEXT("Second option text"), Options[1].Text, TEXT("Goodbye."));

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FProjectDialogueComponent_GetVisibleOptions_NoConversation,
	"ProjectDialogue.Component.GetVisibleOptions.NoConversation",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FProjectDialogueComponent_GetVisibleOptions_NoConversation::RunTest(const FString& Parameters)
{
	UProjectDialogueComponent* Comp = NewObject<UProjectDialogueComponent>();

	TArray<FDialogueOption> Options;
	Comp->GetVisibleOptions(Options);

	TestEqual(TEXT("No options when not in conversation"), Options.Num(), 0);

	return true;
}

// ============================================================================
// Full Flow Integration Tests
// ============================================================================

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FProjectDialogueComponent_FullFlow_BranchAndReturn,
	"ProjectDialogue.Component.Integration.BranchAndReturn",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FProjectDialogueComponent_FullFlow_BranchAndReturn::RunTest(const FString& Parameters)
{
	UDialogueTreeDefinition* Tree = DialogueTestHelper::CreateSimpleTree();
	UProjectDialogueComponent* Comp = DialogueTestHelper::CreateComponentWithTree(Tree);

	Comp->StartConversation();
	TestEqual(TEXT("At greeting"), Comp->GetCurrentNodeId(), TEXT("greeting"));

	Comp->SelectOption(0);
	TestEqual(TEXT("At supplies"), Comp->GetCurrentNodeId(), TEXT("supplies"));

	Comp->AdvanceOrEnd();
	TestEqual(TEXT("Back at greeting"), Comp->GetCurrentNodeId(), TEXT("greeting"));

	Comp->SelectOption(1);
	TestFalse(TEXT("Conversation ended"), Comp->IsInConversation());

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FProjectDialogueComponent_FullFlow_LinearDialogue,
	"ProjectDialogue.Component.Integration.LinearDialogue",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FProjectDialogueComponent_FullFlow_LinearDialogue::RunTest(const FString& Parameters)
{
	UDialogueTreeDefinition* Tree = DialogueTestHelper::CreateLinearTree();
	UProjectDialogueComponent* Comp = DialogueTestHelper::CreateComponentWithTree(Tree);

	Comp->StartConversation();
	TestTrue(TEXT("Started"), Comp->IsInConversation());
	TestEqual(TEXT("Text: Beginning"), Comp->GetCurrentText(), TEXT("Beginning."));

	Comp->AdvanceOrEnd();
	TestEqual(TEXT("Text: Middle"), Comp->GetCurrentText(), TEXT("Middle part."));

	Comp->AdvanceOrEnd();
	TestEqual(TEXT("Text: End"), Comp->GetCurrentText(), TEXT("The end."));

	Comp->AdvanceOrEnd();
	TestFalse(TEXT("Conversation ended at terminal"), Comp->IsInConversation());

	return true;
}

// ============================================================================
// Speaker Tests (universal: NPCs vs objects)
// ============================================================================

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FProjectDialogueComponent_NoSpeaker_Object,
	"ProjectDialogue.Component.Speaker.ObjectNoSpeaker",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FProjectDialogueComponent_NoSpeaker_Object::RunTest(const FString& Parameters)
{
	UDialogueTreeDefinition* Tree = NewObject<UDialogueTreeDefinition>();
	Tree->TreeId = FName(TEXT("ObjectTree"));
	Tree->StartNode = TEXT("menu");

	FDialogueNode Menu;
	Menu.Text = TEXT("What would you like to do?");
	FDialogueOption Opt;
	Opt.Text = TEXT("Play");
	Opt.Next = TEXT("$end");
	Menu.Options.Add(Opt);
	Tree->Nodes.Add(TEXT("menu"), Menu);

	UProjectDialogueComponent* Comp = DialogueTestHelper::CreateComponentWithTree(Tree);
	Comp->StartConversation();

	TestTrue(TEXT("Speaker is empty for objects"), Comp->GetCurrentSpeaker().IsEmpty());
	TestEqual(TEXT("Text shows menu"), Comp->GetCurrentText(), TEXT("What would you like to do?"));

	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
