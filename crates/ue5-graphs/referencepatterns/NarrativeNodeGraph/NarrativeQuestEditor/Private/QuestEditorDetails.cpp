// Copyright Narrative Tools 2022. 

#include "QuestEditorDetails.h"
#include "DetailLayoutBuilder.h"
#include "DetailWidgetRow.h"
#include "QuestSM.h"
#include "Quest.h"
#include "QuestBlueprint.h"
#include "DetailCategoryBuilder.h"
#include "NarrativeDataTask.h"

#define LOCTEXT_NAMESPACE "QuestEditorDetails"

TSharedRef<IDetailCustomization> FQuestEditorDetails::MakeInstance()
{
	return MakeShareable(new FQuestEditorDetails);
}

void FQuestEditorDetails::CustomizeDetails(IDetailLayoutBuilder& DetailLayout)
{
	LayoutBuilder = &DetailLayout;

	TArray<TWeakObjectPtr<UObject>> EditedObjects;
	DetailLayout.GetObjectsBeingCustomized(EditedObjects);

	if (EditedObjects.Num() > 0 && EditedObjects.IsValidIndex(0))
	{
		//Currently quest nodes don't actually support narrative conditions, so don't display these in the editor 
		if (UQuestNode* EditedNode = Cast<UQuestNode>(EditedObjects[0].Get()))
		{
			TSharedPtr<IPropertyHandle> ConditionsHandle = DetailLayout.GetProperty(GET_MEMBER_NAME_CHECKED(UNarrativeNodeBase, Conditions));

			DetailLayout.HideProperty(ConditionsHandle);
		}

		if (UQuestState* EditedState = Cast<UQuestState>(EditedObjects[0].Get()))
		{
			//IDetailCategoryBuilder& Category = DetailLayout.EditCategory("Details");
			//TSharedPtr<IPropertyHandle> NamePropertyHandle = DetailLayout.GetProperty(GET_MEMBER_NAME_CHECKED(UQuestState, ID));
			//NamePropertyHandle->SetOnPropertyValueChanged(FSimpleDelegate::CreateRaw(this, &FQuestEditorDetails::OnStateNameChanged));
		}
		else if (UQuestBranch* EditedBranch = Cast<UQuestBranch>(EditedObjects[0].Get()))
		{
			IDetailCategoryBuilder& Category = DetailLayout.EditCategory("Task");

			////TODO why isn't this hiding 
			//if (EditedBranch->bAddMultipleTasks)
			//{
			//	DetailLayout.HideProperty(TaskPropHandle);

			//	TSharedPtr<IPropertyHandleArray> TasksPropHandleArr = TasksPropHandle->AsArray();

			//	uint32 NumElems;
			//	TasksPropHandleArr->GetNumElements(NumElems);
			//	for (uint32 i = 0; i < NumElems; ++i)
			//	{
			//		if (TSharedPtr<IPropertyHandle> TaskArrPropHandle = TasksPropHandleArr->GetElement(i))
			//		{
			//			if (EditedBranch->Tasks.IsValidIndex(i))
			//			{
			//				FQuestTask& Task = EditedBranch->Tasks[i];

			//				if (Task.Task)
			//				{
			//					TSharedPtr<IPropertyHandle> TaskTaskHandle = TaskArrPropHandle->GetChildHandle(GET_MEMBER_NAME_CHECKED(FQuestTask, Task));
			//					TSharedPtr<IPropertyHandle> ArgHandle = TaskArrPropHandle->GetChildHandle(GET_MEMBER_NAME_CHECKED(FQuestTask, Argument));

			//					//Need to refresh when a task is changed 
			//					TaskTaskHandle->SetOnPropertyValueChanged(FSimpleDelegate::CreateRaw(this, &FQuestEditorDetails::OnTaskChanged));

			//					ArgHandle->SetPropertyDisplayName(FText::FromString(Task.Task->ArgumentName));
			//				}
			//			}
			//		}
			//	}
			//}
			//else
			//{
			//	DetailLayout.HideProperty(TasksPropHandle);

			//	TSharedPtr<IPropertyHandle> TaskTaskHandle = TaskPropHandle->GetChildHandle(GET_MEMBER_NAME_CHECKED(FQuestTask, Task));
			//	TSharedPtr<IPropertyHandle> ArgHandle = TaskPropHandle->GetChildHandle(GET_MEMBER_NAME_CHECKED(FQuestTask, Argument));


			//	TaskTaskHandle->SetOnPropertyValueChanged(FSimpleDelegate::CreateRaw(this, &FQuestEditorDetails::OnTaskChanged));

			//	if (EditedBranch->Task.Task)
			//	{
			//		ArgHandle->SetPropertyDisplayName(FText::FromString(EditedBranch->Task.Task->ArgumentName));
			//	}

			//}

			//FText GroupLabel(LOCTEXT("DetailsGroup", "Details"));

			////Add a button to make the quest designer more simplified 
			//Category.AddCustomRow(GroupLabel)
			//	.ValueContent()
			//	[
			//		SNew(SButton)
			//		.ButtonStyle(FAppStyle::Get(), "RoundButton")
			//		.OnClicked(this, &FQuestEditorDetails::AddMultipleTasksClicked)
			//		[
			//			SNew(STextBlock)
			//			.Font(IDetailLayoutBuilder::GetDetailFontBold())
			//			.Text(EditedBranch->bAddMultipleTasks ? LOCTEXT("HaveOneTaskText", "Use Single Task") : LOCTEXT("HaveMultipleTasksText", "Add Multiple Tasks"))
			//		]
			//	];


			//TSharedPtr<IPropertyHandle> ActionPropHandle = DetailLayout.GetProperty(GET_MEMBER_NAME_CHECKED(UQuestBranch, Event));
			//TSharedPtr<IPropertyHandle> ArgumentPropHandle = DetailLayout.GetProperty(GET_MEMBER_NAME_CHECKED(UQuestBranch, Argument));
			//TSharedPtr<IPropertyHandle> QuantityPropHandle = DetailLayout.GetProperty(GET_MEMBER_NAME_CHECKED(UQuestBranch, Quantity));
			//TSharedPtr<IPropertyHandle> HiddenPropHandle = DetailLayout.GetProperty(GET_MEMBER_NAME_CHECKED(UQuestBranch, bHidden));
			//TSharedPtr<IPropertyHandle> DescriptionPropHandle = DetailLayout.GetProperty(GET_MEMBER_NAME_CHECKED(UQuestNode, Description));

			//Category.AddProperty(DescriptionPropHandle);

			//ArgumentPropHandle->SetOnPropertyValueChanged(FSimpleDelegate::CreateRaw(this, &FQuestEditorDetails::OnArgumentChanged));
			//ActionPropHandle->SetOnPropertyValueChanged(FSimpleDelegate::CreateRaw(this, &FQuestEditorDetails::OnActionChanged));

			//bool bShouldHideReference = false;

			//Category.AddProperty(ActionPropHandle);
			//if (ArgumentPropHandle->IsValidHandle())
			//{
			//	if (EditedBranch->Event)
			//	{
			//		ArgumentPropHandle->SetPropertyDisplayName(EditedBranch->Event->GetReferenceDisplayText());
			//		Category.AddProperty(ArgumentPropHandle);
			//	}
			//	else
			//	{
			//		DetailLayout.HideProperty(ArgumentPropHandle);
			//	}
			//}

			//Category.AddProperty(QuantityPropHandle);
			//Category.AddProperty(HiddenPropHandle);
		}

	}
}

void FQuestEditorDetails::OnArgumentChanged()
{
	//When argument changes auto-update the description
	if (LayoutBuilder)
	{
		TArray<TWeakObjectPtr<UObject>> EditedObjects;
		LayoutBuilder->GetObjectsBeingCustomized(EditedObjects);

		if (EditedObjects.IsValidIndex(0))
		{
			if (UQuestBranch* EditedBranch = Cast<UQuestBranch>(EditedObjects[0].Get()))
			{
				//LOCTEXT only takes literals so we use the function directly
				//EditedBranch->Description = FText::FromString*EditedBranch->Event->DefaultDescription, TEXT(LOCTEXT_NAMESPACE), TEXT("ActionDescription")), FText::FromString(EditedBranch->Argument));
			}
		}
	}
}

void FQuestEditorDetails::OnTaskChanged()
{
	if (LayoutBuilder)
	{
		LayoutBuilder->ForceRefreshDetails();
	}
}

#undef LOCTEXT_NAMESPACE