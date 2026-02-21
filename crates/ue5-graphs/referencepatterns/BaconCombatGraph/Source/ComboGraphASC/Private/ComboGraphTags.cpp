// © 2025 mrbaconvn. All Rights Reserved.

#include "ComboGraphTags.h"

UE_DEFINE_GAMEPLAY_TAG_COMMENT(ComboGraphTags::Block_Replicated, "ComboGraph.Block.Replicated", "Block any combo graph functionality (REPLICATED)");
UE_DEFINE_GAMEPLAY_TAG_COMMENT(ComboGraphTags::Block, "ComboGraph.Block", "Block any combo graph functionality (LOCAL)");
UE_DEFINE_GAMEPLAY_TAG_COMMENT(ComboGraphTags::BlockProceedGraph, "ComboGraph.Block.ProceedGraph", "Block graph to proceed to next node"); 
UE_DEFINE_GAMEPLAY_TAG_COMMENT(ComboGraphTags::InputWindow, "ComboGraph.InputWindow", "Input window for combo graph");

UE_DEFINE_GAMEPLAY_TAG_COMMENT(ComboGraphInputs::Attack, "ComboGraph.Input.Attack", "Attack input for combo graph");
UE_DEFINE_GAMEPLAY_TAG_COMMENT(ComboGraphInputs::SpecialAttack, "ComboGraph.Input.SpecialAttack", "Special attack input for combo graph");
UE_DEFINE_GAMEPLAY_TAG_COMMENT(ComboGraphInputs::ChargeAttack, "ComboGraph.Input.ChargeAttack", "Charge attack input for combo graph");