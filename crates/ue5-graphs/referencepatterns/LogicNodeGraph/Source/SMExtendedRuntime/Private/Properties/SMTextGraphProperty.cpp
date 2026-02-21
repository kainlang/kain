// Copyright Recursoft LLC 2019-2023. All Rights Reserved.

#include "SMTextGraphProperty.h"

void FSMTextGraphProperty_Runtime::SetResult(uint8* Value)
{
	Result = *(FText*)Value;
}


FSMTextGraphProperty::FSMTextGraphProperty() : Super()
{
#if WITH_EDITORONLY_DATA
	GraphModuleClassName = "SMExtendedEditor";
	GraphClassName = "SMTextPropertyGraph";
	GraphSchemaClassName = "SMTextPropertyGraphSchema";
#endif
}

void FSMTextGraphProperty::SetResult(uint8* Value)
{
	Result = *(FText*)Value;
}
