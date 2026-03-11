// Copyright ALIS. All Rights Reserved.

/**
 * Unit Tests for Dialogue Data Structures
 *
 * Tests FDialogueNode, FDialogueOption, and UDialogueTreeDefinition.
 * Component-level tests are in ProjectDialogueComponentTests.cpp.
 */

#include "Misc/AutomationTest.h"
#include "Data/ProjectDialogueTypes.h"
#include "Data/DialogueTreeDefinition.h"

#if WITH_DEV_AUTOMATION_TESTS

// ============================================================================
// FDialogueOption Tests
// ============================================================================

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FProjectDialogueData_Option_DefaultEmpty,
	"ProjectDialogue.Data.Option.DefaultEmpty",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FProjectDialogueData_Option_DefaultEmpty::RunTest(const FString& Parameters)
{
	FDialogueOption Option;
	TestTrue(TEXT("Text is empty"), Option.Text.IsEmpty());
	TestTrue(TEXT("Next is empty"), Option.Next.IsEmpty());
	TestTrue(TEXT("Condition is empty"), Option.Condition.IsEmpty());
	TestTrue(TEXT("Default is end option (empty next)"), Option.IsEndOption());

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FProjectDialogueData_Option_EndVariants,
	"ProjectDialogue.Data.Option.EndVariants",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FProjectDialogueData_Option_EndVariants::RunTest(const FString& Parameters)
{
	FDialogueOption ExplicitEnd;
	ExplicitEnd.Next = TEXT("$end");
	TestTrue(TEXT("$end is end option"), ExplicitEnd.IsEndOption());

	FDialogueOption EmptyNext;
	TestTrue(TEXT("Empty next is end option"), EmptyNext.IsEndOption());

	FDialogueOption ValidNext;
	ValidNext.Next = TEXT("some_node");
	TestFalse(TEXT("Valid next is not end option"), ValidNext.IsEndOption());

	return true;
}

// ============================================================================
// FDialogueNode Tests
// ============================================================================

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FProjectDialogueData_Node_DefaultEmpty,
	"ProjectDialogue.Data.Node.DefaultEmpty",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FProjectDialogueData_Node_DefaultEmpty::RunTest(const FString& Parameters)
{
	FDialogueNode Node;
	TestTrue(TEXT("Speaker is empty"), Node.Speaker.IsEmpty());
	TestTrue(TEXT("Text is empty"), Node.Text.IsEmpty());
	TestEqual(TEXT("No options"), Node.Options.Num(), 0);
	TestTrue(TEXT("Next is empty"), Node.Next.IsEmpty());
	TestTrue(TEXT("Default node is terminal"), Node.IsTerminal());
	TestFalse(TEXT("Default node has no options"), Node.HasOptions());

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FProjectDialogueData_Node_TerminalVariants,
	"ProjectDialogue.Data.Node.TerminalVariants",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FProjectDialogueData_Node_TerminalVariants::RunTest(const FString& Parameters)
{
	FDialogueNode Terminal;
	Terminal.Text = TEXT("End.");
	TestTrue(TEXT("No next, no options = terminal"), Terminal.IsTerminal());

	FDialogueNode EndNext;
	EndNext.Text = TEXT("End.");
	EndNext.Next = TEXT("$end");
	TestTrue(TEXT("next=$end = terminal"), EndNext.IsTerminal());

	FDialogueNode AutoAdv;
	AutoAdv.Text = TEXT("...");
	AutoAdv.Next = TEXT("next_node");
	TestFalse(TEXT("Has next = not terminal"), AutoAdv.IsTerminal());

	FDialogueNode WithOpts;
	WithOpts.Text = TEXT("Choose:");
	FDialogueOption Opt;
	Opt.Text = TEXT("A");
	WithOpts.Options.Add(Opt);
	TestFalse(TEXT("Has options = not terminal"), WithOpts.IsTerminal());

	return true;
}

// ============================================================================
// UDialogueTreeDefinition Tests
// ============================================================================

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FProjectDialogueData_Tree_EmptyInvalid,
	"ProjectDialogue.Data.Tree.EmptyInvalid",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FProjectDialogueData_Tree_EmptyInvalid::RunTest(const FString& Parameters)
{
	UDialogueTreeDefinition* Tree = NewObject<UDialogueTreeDefinition>();
	TestFalse(TEXT("Empty tree is invalid"), Tree->IsValid());

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FProjectDialogueData_Tree_ValidTree,
	"ProjectDialogue.Data.Tree.ValidTree",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FProjectDialogueData_Tree_ValidTree::RunTest(const FString& Parameters)
{
	UDialogueTreeDefinition* Tree = NewObject<UDialogueTreeDefinition>();
	Tree->StartNode = TEXT("start");

	FDialogueNode Node;
	Node.Text = TEXT("Hello.");
	Tree->Nodes.Add(TEXT("start"), Node);

	TestTrue(TEXT("Tree with matching start node is valid"), Tree->IsValid());

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FProjectDialogueData_Tree_MismatchedStart,
	"ProjectDialogue.Data.Tree.MismatchedStart",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FProjectDialogueData_Tree_MismatchedStart::RunTest(const FString& Parameters)
{
	UDialogueTreeDefinition* Tree = NewObject<UDialogueTreeDefinition>();
	Tree->StartNode = TEXT("nonexistent");

	FDialogueNode Node;
	Node.Text = TEXT("A");
	Tree->Nodes.Add(TEXT("actual_node"), Node);

	TestFalse(TEXT("Start node must exist in nodes map"), Tree->IsValid());

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FProjectDialogueData_Tree_FindNode,
	"ProjectDialogue.Data.Tree.FindNode",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FProjectDialogueData_Tree_FindNode::RunTest(const FString& Parameters)
{
	UDialogueTreeDefinition* Tree = NewObject<UDialogueTreeDefinition>();

	FDialogueNode Node;
	Node.Speaker = TEXT("Test");
	Node.Text = TEXT("Found me.");
	Tree->Nodes.Add(TEXT("target"), Node);

	const FDialogueNode* Found = Tree->FindNode(TEXT("target"));
	TestNotNull(TEXT("Found existing node"), Found);
	if (Found)
	{
		TestEqual(TEXT("Text matches"), Found->Text, TEXT("Found me."));
		TestEqual(TEXT("Speaker matches"), Found->Speaker, TEXT("Test"));
	}

	const FDialogueNode* Missing = Tree->FindNode(TEXT("missing"));
	TestNull(TEXT("Missing node returns null"), Missing);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FProjectDialogueData_Tree_MultipleNodes,
	"ProjectDialogue.Data.Tree.MultipleNodes",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FProjectDialogueData_Tree_MultipleNodes::RunTest(const FString& Parameters)
{
	UDialogueTreeDefinition* Tree = NewObject<UDialogueTreeDefinition>();
	Tree->StartNode = TEXT("a");

	FDialogueNode NodeA;
	NodeA.Text = TEXT("Node A");
	NodeA.Next = TEXT("b");
	Tree->Nodes.Add(TEXT("a"), NodeA);

	FDialogueNode NodeB;
	NodeB.Text = TEXT("Node B");
	Tree->Nodes.Add(TEXT("b"), NodeB);

	TestTrue(TEXT("Tree is valid"), Tree->IsValid());
	TestEqual(TEXT("Two nodes"), Tree->Nodes.Num(), 2);

	const FDialogueNode* FoundA = Tree->FindNode(TEXT("a"));
	TestNotNull(TEXT("Found node A"), FoundA);

	const FDialogueNode* FoundB = Tree->FindNode(TEXT("b"));
	TestNotNull(TEXT("Found node B"), FoundB);

	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
