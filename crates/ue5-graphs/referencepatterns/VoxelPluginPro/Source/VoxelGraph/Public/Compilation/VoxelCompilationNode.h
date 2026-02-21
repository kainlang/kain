// Copyright Voxel Plugin SAS. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "VoxelPinCategory.h"
#include "Compilation/VoxelCompilationEnums.h"
#include "VoxelMinimal.h"

class UVoxelNode;
class FVoxelComputeNode;
class FVoxelCompilationNode;
class FVoxelGraphErrorReporter;
struct FVoxelCompilationPin;

struct VOXELGRAPH_API FVoxelCompilationPin
{
	class FVoxelCompilationPinLinkedToIterator
	{
	public:
		using Type = FVoxelCompilationPin;
		FVoxelCompilationPinLinkedToIterator(const Type& Pin) : Pin(Pin) {}

		class FIterator
		{
		public:
			FIterator(const Type& Pin) : Pin(Pin), InitialNum(Pin.LinkedTo.Num()) {}

			inline void operator++() { Index++; }
			inline bool operator!=(const FIterator&) const
			{
				ensureMsgf(Pin.LinkedTo.Num() == InitialNum, TEXT("Array has changed during ranged-for iteration!"));
				return Pin.LinkedTo.IsValidIndex(Index);
			}
			inline Type& operator*() const { return *Pin.LinkedTo[Index]; }

		private:
			const Type& Pin;
			const int32 InitialNum;
			int32 Index = 0;
		};

		inline FIterator begin() { return FIterator(Pin); }
		inline FIterator end() { return FIterator(Pin); }

	private:
		const Type& Pin;
	};

public:
	FVoxelCompilationNode& Node;
	int32 const Index;
	EVoxelPinDirection const Direction;
	EVoxelPinCategory const PinCategory;
	FName const Name;

public:
	inline void BreakLinkTo(FVoxelCompilationPin& Other)
	{
		ensure(this->LinkedTo.Remove(&Other) == 1);
		ensure(Other.LinkedTo.Remove(this) == 1);
	}
	inline bool IsLinkedTo(FVoxelCompilationPin& Other) const
	{
		const bool bALinkedToB = this->LinkedTo.Contains(&Other);
		const bool bBLinkedToA = Other.LinkedTo.Contains(this);

		ensure((bALinkedToB && bBLinkedToA) || (!bALinkedToB && !bBLinkedToA));

		return bALinkedToB;
	}
	inline void LinkTo(FVoxelCompilationPin& Other)
	{
		ensure(Direction != Other.Direction);
		ensure(PinCategory == Other.PinCategory);

		this->LinkedTo.AddUnique(&Other);
		Other.LinkedTo.AddUnique(this);
	}
	inline void BreakAllLinks()
	{
		auto Copy = LinkedTo;
		for (auto* Pin : Copy)
		{
			BreakLinkTo(*Pin);
		}
		ensure(LinkedTo.Num() == 0);
	}
	inline auto IterateLinkedTo() const { return FVoxelCompilationPinLinkedToIterator(*this); }
	inline TArray<FVoxelCompilationPin*> IterateLinkedToCopy() const { return LinkedTo; }
	inline const TArray<FVoxelCompilationPin*>& GetLinkedToArray() const { return LinkedTo; }
	inline FVoxelCompilationPin& GetLinkedTo(int32 InIndex) const { return *LinkedTo[InIndex]; }
	inline int32 NumLinkedTo() const { return LinkedTo.Num(); }

	void Check(FVoxelGraphErrorReporter& ErrorReporter);

public:
	inline void SetDefaultValue(const FString& String)
	{
		ensure(Direction == EVoxelPinDirection::Input);
		DefaultValue = String;
	}
	inline FString GetDefaultValue() const
	{
		ensure(Direction == EVoxelPinDirection::Input);
		return DefaultValue;
	}

private:
	TArray<FVoxelCompilationPin*> LinkedTo;
	FString DefaultValue;
	
	FVoxelCompilationPin(FVoxelCompilationNode& Node, int32 Index, EVoxelPinDirection Direction, EVoxelPinCategory PinCategory, const FName& Name);
	FVoxelCompilationPin Clone(FVoxelCompilationNode& NewNode) const;

	friend class FVoxelCompilationNode;
	friend class TArray<FVoxelCompilationPin>;
};

class VOXELGRAPH_API FVoxelCompilationNode
{
public:
	template<typename Type, typename PinType, EVoxelPinIter Direction>
	class TVoxelCompilationNodePinsIterator
	{
	public:
		TVoxelCompilationNodePinsIterator(Type& Node) : Node(Node) {}

		class FIterator
		{
		public:
			FIterator(Type& Node) : Node(Node) {}

			inline void operator++() { Index++; }
			inline bool operator!=(const FIterator&) { return Node.template GetCachedPins<Direction>().IsValidIndex(Index); }
			inline PinType& operator*() { return GetPin(Node.template GetCachedPins<Direction>()[Index]); }

		private:
			Type& Node;
			int32 Index = 0;

			static inline PinType& GetPin(PinType* Pin) { return *Pin; }
			static inline PinType& GetPin(PinType& Pin) { return Pin; }
		};

		inline FIterator begin() { return FIterator(Node); }
		inline FIterator end() { return FIterator(Node); }

	private:
		Type& Node;
	};

public:
	uint8 Dependencies = 0;
	// First are deeper in the callstack
	TArray<const UVoxelNode*> SourceNodes;
	const UVoxelNode& Node;
	TArray<FString> DebugMessages;

	virtual ~FVoxelCompilationNode() {}

private:
	const EVoxelCompilationNodeType PrivateType;
	TArray<FVoxelCompilationPin> Pins;
	TArray<int32> InputIds;
	TArray<int32> OutputIds;	

	FVoxelCompilationNode(EVoxelCompilationNodeType Type, const UVoxelNode& Node);
	FVoxelCompilationNode(EVoxelCompilationNodeType Type, const UVoxelNode& Node,
		const TArray<EVoxelPinCategory>& InputCategories,
		const TArray<EVoxelPinCategory>& OutputCategories);

	template<EVoxelCompilationNodeType InType, typename InClass, typename InParent>
	friend class TVoxelCompilationNode;
	friend class FVoxelPassthroughCompilationNode;
	friend class FVoxelSetterCompilationNode;

public:
	inline       FVoxelCompilationPin& GetInputPin (int32 I)       { return *CachedInputPins [I]; }
	inline const FVoxelCompilationPin& GetInputPin (int32 I) const { return *CachedInputPins [I]; }
	inline       FVoxelCompilationPin& GetOutputPin(int32 I)       { return *CachedOutputPins[I]; }
	inline const FVoxelCompilationPin& GetOutputPin(int32 I) const { return *CachedOutputPins[I]; }
	inline       FVoxelCompilationPin& GetPin(int32 I)       { return Pins[I]; }
	inline const FVoxelCompilationPin& GetPin(int32 I) const { return Pins[I]; }

	inline int32 GetInputId (int32 I) const { return InputIds [I]; }
	inline int32 GetOutputId(int32 I) const { return OutputIds[I]; }
	inline void SetInputId (int32 I, int32 Id) { InputIds [I] = Id; }
	inline void SetOutputId(int32 I, int32 Id) { OutputIds[I] = Id; }
	inline const TArray<int32>& GetInputIds()  const { return InputIds ; }
	inline const TArray<int32>& GetOutputIds() const { return OutputIds; }

	inline int32 GetInputCount () const { return CachedInputPins .Num(); }
	inline int32 GetOutputCount() const { return CachedOutputPins.Num(); }
	inline int32 GetNumPins() const { return Pins.Num(); }

	inline int32 GetInputCountWithoutExecs () const { return InputIds .Num(); }
	inline int32 GetOutputCountWithoutExecs() const { return OutputIds.Num(); }

	inline int32 GetExecInputCount () const { return GetInputCount () - GetInputCountWithoutExecs (); }
	inline int32 GetExecOutputCount() const { return GetOutputCount() - GetOutputCountWithoutExecs(); }

	TArray<EVoxelPinCategory> GetInputPinCategories() const;
	TArray<EVoxelPinCategory> GetOutputPinCategories() const;

	FString GetPrettyName() const;

	bool IsExecNode() const { return GetExecInputCount() > 0 || GetExecOutputCount() > 0; }
	bool IsSeedNode() const;

	void BreakAllLinks();
	bool IsLinkedTo(const FVoxelCompilationNode* OtherNode) const;
	template<typename T>
	bool IsLinkedToOne(const T& OtherNodes) const
	{
		for (auto* OtherNode : OtherNodes)
		{
			if (IsLinkedTo(OtherNode))
			{
				return true;
			}
		}
		return false;
	}
	void CheckIsNotLinked(FVoxelGraphErrorReporter& ErrorReporter) const;

public:
	template<typename T>
	inline bool IsA() const
	{
		return T::StaticType() == PrivateType;
	}
	inline EVoxelCompilationNodeType GetPrivateType() const
	{
		return PrivateType;
	}

public:
	//~ Begin FVoxelCompilationNode Interface
	// Clone this node
	virtual TSharedPtr<FVoxelCompilationNode> Clone(bool bFixLinks = false) const = 0;
	// Get the compute node corresponding to this compilation node
	virtual TVoxelSharedPtr<FVoxelComputeNode> GetComputeNode() const;
	// The axis used by this nodes
	virtual uint8 GetDefaultAxisDependencies() const { return 0; }
	// Can this node be computed at compile time?
	virtual bool IsPureNode() const { return false; }
	// Check this node validity
	virtual void Check(FVoxelGraphErrorReporter& ErrorReporter) const;
	// This class name
	virtual FString GetClassName() const { return "NONE"; }
	//~ End FVoxelCompilationNode Interface

public:
	template<EVoxelPinIter Direction>
	inline auto IteratePins()
	{
		return TVoxelCompilationNodePinsIterator<FVoxelCompilationNode, FVoxelCompilationPin, Direction>(*this);
	}
	template<EVoxelPinIter Direction>
	inline auto IteratePins() const
	{
		return TVoxelCompilationNodePinsIterator<const FVoxelCompilationNode, const FVoxelCompilationPin, Direction>(*this);
	}

protected:
	template<typename T>
	TSharedPtr<T> CloneInternal(bool bFixLinks) const
	{
		auto NewNode = MakeShared<T>(Node, TArray<EVoxelPinCategory>(), TArray<EVoxelPinCategory>());
		CopyPropertiesToNewNode(NewNode, bFixLinks);
		return NewNode;
	}

private:
	void CopyPropertiesToNewNode(const TSharedRef<FVoxelCompilationNode>& NewNode, bool bFixLinks) const;

private:
	// Cache for fast access
	TArray<FVoxelCompilationPin*> CachedInputPins;
	TArray<FVoxelCompilationPin*> CachedOutputPins;

	void RebuildPinsArray();

	template<EVoxelPinIter Direction>
	const auto& GetCachedPins() const;
	template<EVoxelPinIter Direction>
	auto& GetCachedPins();
};

template<> inline const auto& FVoxelCompilationNode::GetCachedPins<EVoxelPinIter::Input >() const { return CachedInputPins; }
template<> inline       auto& FVoxelCompilationNode::GetCachedPins<EVoxelPinIter::Input >()       { return CachedInputPins; }
template<> inline const auto& FVoxelCompilationNode::GetCachedPins<EVoxelPinIter::Output>() const { return CachedOutputPins; }
template<> inline       auto& FVoxelCompilationNode::GetCachedPins<EVoxelPinIter::Output>()       { return CachedOutputPins; }
template<> inline const auto& FVoxelCompilationNode::GetCachedPins<EVoxelPinIter::All   >() const { return Pins; }
template<> inline       auto& FVoxelCompilationNode::GetCachedPins<EVoxelPinIter::All   >()       { return Pins; }

template<EVoxelCompilationNodeType InType, typename InClass, typename InParent = FVoxelCompilationNode>
class TVoxelCompilationNode : public InParent
{
public:
	TVoxelCompilationNode(const UVoxelNode& Node)
		: InParent(InType, Node)
	{
	}
	TVoxelCompilationNode(const UVoxelNode& Node, const TArray<EVoxelPinCategory>& InputCategories, const TArray<EVoxelPinCategory>& OutputCategories)
		: InParent(InType, Node, InputCategories, OutputCategories)
	{
	}

	virtual TSharedPtr<FVoxelCompilationNode> Clone(bool bFixLinks = false) const override final
	{
		TSharedPtr<InClass> Result = this->template CloneInternal<InClass>(bFixLinks);
		InClass::CopyProperties(*static_cast<const InClass*>(this), *Result.Get());
		return Result;
	}

	static void CopyProperties(const InClass& From, InClass& To)
	{
		// Do copy fixup here
	}
	
	static EVoxelCompilationNodeType StaticType()
	{
		return InType;
	}
};

template<typename To, typename From>
inline To* CastVoxel(From* Node)
{
	if (Node && Node->GetPrivateType() == To::StaticType())
	{
		return static_cast<To*>(Node);
	}
	else
	{
		return nullptr;
	}
}

template<typename To, typename From>
inline To& CastCheckedVoxel(From* Node)
{
	check(Node);
	auto* Result = CastVoxel<To>(Node);
	check(Result);
	return *Result;
}
template<typename To, typename From>
inline To& CastCheckedVoxel(From& Node)
{
	return CastCheckedVoxel<To>(&Node);
}
