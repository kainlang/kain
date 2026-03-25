"""
Python-based validation rules for KAIN Oracle
Allows dynamic rule loading and custom validation logic
"""

from typing import List, Dict, Any, Optional
from dataclasses import dataclass
from enum import Enum
import re


class IssueSeverity(Enum):
    ERROR = "error"
    WARNING = "warning"
    INFO = "info"


class IssueCategory(Enum):
    NAMING = "naming"
    REPLICATION = "replication"
    BLUEPRINT = "blueprint"
    MEMORY = "memory"
    PERFORMANCE = "performance"
    COMPATIBILITY = "compatibility"
    SYNTAX = "syntax"


@dataclass
class ValidationIssue:
    severity: IssueSeverity
    category: IssueCategory
    message: str
    fix_suggestion: Optional[str] = None
    ue5_doc_link: Optional[str] = None
    
    def to_dict(self) -> Dict[str, Any]:
        return {
            "severity": self.severity.value,
            "category": self.category.value,
            "message": self.message,
            "fix_suggestion": self.fix_suggestion,
            "ue5_doc_link": self.ue5_doc_link,
        }


class ValidationRule:
    """Base class for validation rules"""
    
    def __init__(self, name: str, description: str):
        self.name = name
        self.description = description
    
    def validate(self, item_type: str, data: Dict[str, Any]) -> List[ValidationIssue]:
        """Override this method to implement validation logic"""
        raise NotImplementedError


# ============================================================================
# NAMING RULES
# ============================================================================

class ActorNamingRule(ValidationRule):
    """Validates actor naming conventions"""
    
    def __init__(self):
        super().__init__(
            "actor_naming",
            "Ensures actors follow UE5 naming conventions"
        )
    
    def validate(self, item_type: str, data: Dict[str, Any]) -> List[ValidationIssue]:
        if item_type != "actor":
            return []
        
        issues = []
        name = data.get("name", "")
        
        # Check for reserved prefixes
        if name.startswith("U") or name.startswith("A") or name.startswith("F"):
            issues.append(ValidationIssue(
                severity=IssueSeverity.WARNING,
                category=IssueCategory.NAMING,
                message=f"Actor '{name}' starts with UE5 prefix. The compiler will add 'A' prefix automatically.",
                fix_suggestion=f"Rename to '{name[1:]}' and let the compiler add the 'A' prefix."
            ))
        
        # Check for numeric-only names
        if name.isdigit():
            issues.append(ValidationIssue(
                severity=IssueSeverity.ERROR,
                category=IssueCategory.NAMING,
                message=f"Actor name '{name}' is numeric-only, which is invalid in UE5.",
                fix_suggestion="Use a descriptive name like 'GameActor' or 'PlayerCharacter'."
            ))
        
        # Check for overly generic names
        generic_names = ["Actor", "Object", "Thing", "Item", "Entity"]
        if name in generic_names:
            issues.append(ValidationIssue(
                severity=IssueSeverity.WARNING,
                category=IssueCategory.NAMING,
                message=f"Actor name '{name}' is too generic and may cause confusion.",
                fix_suggestion="Use a more specific name that describes the actor's purpose."
            ))
        
        return issues


class ComponentNamingRule(ValidationRule):
    """Validates component naming conventions"""
    
    def __init__(self):
        super().__init__(
            "component_naming",
            "Ensures components follow UE5 naming conventions"
        )
    
    def validate(self, item_type: str, data: Dict[str, Any]) -> List[ValidationIssue]:
        if item_type != "component":
            return []
        
        issues = []
        name = data.get("name", "")
        
        # Components should end with "Component" for clarity
        if not name.endswith("Component"):
            issues.append(ValidationIssue(
                severity=IssueSeverity.INFO,
                category=IssueCategory.NAMING,
                message=f"Component '{name}' doesn't end with 'Component'. Consider renaming for clarity.",
                fix_suggestion=f"Rename to '{name}Component' to follow UE5 conventions."
            ))
        
        return issues


# ============================================================================
# REPLICATION RULES
# ============================================================================

class ReplicationValidityRule(ValidationRule):
    """Validates replication setup"""
    
    def __init__(self):
        super().__init__(
            "replication_validity",
            "Ensures replicated properties are set up correctly"
        )
    
    def validate(self, item_type: str, data: Dict[str, Any]) -> List[ValidationIssue]:
        issues = []
        
        if item_type == "actor":
            # Check for replicated state without GetLifetimeReplicatedProps
            state_fields = data.get("state", [])
            has_replicated = any(
                "replicated" in field.get("attributes", [])
                for field in state_fields
            )
            
            if has_replicated:
                # Check if GetLifetimeReplicatedProps is declared
                methods = data.get("methods", [])
                has_replication_func = any(
                    m.get("name") == "GetLifetimeReplicatedProps"
                    for m in methods
                )
                
                if not has_replication_func:
                    issues.append(ValidationIssue(
                        severity=IssueSeverity.ERROR,
                        category=IssueCategory.REPLICATION,
                        message=f"Actor '{data.get('name')}' has replicated properties but is missing GetLifetimeReplicatedProps().",
                        fix_suggestion="Add: virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;",
                        ue5_doc_link="https://docs.unrealengine.com/5.4/en-US/replicated-properties-in-unreal-engine/"
                    ))
        
        return issues


class RpcParameterRule(ValidationRule):
    """Validates RPC parameters"""
    
    def __init__(self):
        super().__init__(
            "rpc_parameters",
            "Ensures RPC parameters are valid"
        )
    
    def validate(self, item_type: str, data: Dict[str, Any]) -> List[ValidationIssue]:
        if item_type != "actor":
            return []
        
        issues = []
        handlers = data.get("handlers", [])
        
        for handler in handlers:
            message_type = handler.get("message_type", "")
            is_rpc = any(message_type.startswith(prefix) for prefix in ["Server_", "Client_", "Multicast_"])
            
            if is_rpc:
                params = handler.get("params", [])
                
                # Check for delegate parameters (not allowed in RPCs)
                for param in params:
                    param_type = param.get("type", "")
                    if "delegate" in param_type.lower():
                        issues.append(ValidationIssue(
                            severity=IssueSeverity.ERROR,
                            category=IssueCategory.REPLICATION,
                            message=f"RPC '{message_type}' has delegate parameter '{param.get('name')}'. Delegates cannot be replicated.",
                            fix_suggestion="Remove the delegate parameter or use a different communication method.",
                            ue5_doc_link="https://docs.unrealengine.com/5.4/en-US/rpcs-in-unreal-engine/"
                        ))
                
                # Check for large struct parameters (performance warning)
                for param in params:
                    param_type = param.get("type", "")
                    # TODO: Check struct size from type registry
                    # For now, just warn about arrays
                    if "array" in param_type.lower():
                        issues.append(ValidationIssue(
                            severity=IssueSeverity.WARNING,
                            category=IssueCategory.PERFORMANCE,
                            message=f"RPC '{message_type}' has array parameter '{param.get('name')}'. Large arrays can impact network performance.",
                            fix_suggestion="Consider using a smaller data structure or splitting into multiple RPCs."
                        ))
        
        return issues


# ============================================================================
# BLUEPRINT RULES
# ============================================================================

class BlueprintEventConflictRule(ValidationRule):
    """Validates Blueprint event conflicts"""
    
    def __init__(self):
        super().__init__(
            "blueprint_event_conflict",
            "Ensures Blueprint events don't conflict with replication"
        )
    
    def validate(self, item_type: str, data: Dict[str, Any]) -> List[ValidationIssue]:
        if item_type not in ["actor", "function"]:
            return []
        
        issues = []
        
        # Check for BlueprintImplementableEvent + RPC conflict
        attributes = data.get("attributes", [])
        name = data.get("name", "")
        
        is_blueprint_implementable = "blueprint_implementable_event" in attributes
        is_blueprint_native = "blueprint_native_event" in attributes
        is_rpc = any(name.startswith(prefix) for prefix in ["Server_", "Client_", "Multicast_"])
        
        if is_blueprint_implementable and is_rpc:
            issues.append(ValidationIssue(
                severity=IssueSeverity.ERROR,
                category=IssueCategory.BLUEPRINT,
                message=f"Function '{name}' is both BlueprintImplementableEvent and an RPC. This is not allowed.",
                fix_suggestion="Remove either the BlueprintImplementableEvent attribute or the RPC prefix.",
                ue5_doc_link="https://docs.unrealengine.com/5.4/en-US/blueprint-function-specifiers/"
            ))
        
        if is_blueprint_native and is_rpc:
            issues.append(ValidationIssue(
                severity=IssueSeverity.ERROR,
                category=IssueCategory.BLUEPRINT,
                message=f"Function '{name}' is both BlueprintNativeEvent and an RPC. This is not allowed.",
                fix_suggestion="Remove either the BlueprintNativeEvent attribute or the RPC prefix.",
                ue5_doc_link="https://docs.unrealengine.com/5.4/en-US/blueprint-function-specifiers/"
            ))
        
        if is_blueprint_implementable and is_blueprint_native:
            issues.append(ValidationIssue(
                severity=IssueSeverity.ERROR,
                category=IssueCategory.BLUEPRINT,
                message=f"Function '{name}' is both BlueprintImplementableEvent and BlueprintNativeEvent. Choose one.",
                fix_suggestion="Use BlueprintNativeEvent if you want a C++ implementation, or BlueprintImplementableEvent for Blueprint-only."
            ))
        
        return issues


# ============================================================================
# PERFORMANCE RULES
# ============================================================================

class TickFunctionComplexityRule(ValidationRule):
    """Warns about complex Tick functions"""
    
    def __init__(self):
        super().__init__(
            "tick_complexity",
            "Warns about potentially expensive Tick functions"
        )
    
    def validate(self, item_type: str, data: Dict[str, Any]) -> List[ValidationIssue]:
        if item_type != "actor":
            return []
        
        issues = []
        handlers = data.get("handlers", [])
        
        for handler in handlers:
            if handler.get("message_type") == "Tick":
                # Check for expensive operations in Tick
                body = handler.get("body", "")
                
                # Check for string operations
                if "string" in body.lower() or "concat" in body.lower():
                    issues.append(ValidationIssue(
                        severity=IssueSeverity.WARNING,
                        category=IssueCategory.PERFORMANCE,
                        message="Tick function contains string operations. This can be expensive when called every frame.",
                        fix_suggestion="Cache string results or move string operations outside of Tick."
                    ))
                
                # Check for array operations
                if "push" in body.lower() or "append" in body.lower():
                    issues.append(ValidationIssue(
                        severity=IssueSeverity.WARNING,
                        category=IssueCategory.PERFORMANCE,
                        message="Tick function modifies arrays. This can cause memory allocations every frame.",
                        fix_suggestion="Pre-allocate arrays or use object pooling."
                    ))
        
        return issues


class LargeReplicatedStructRule(ValidationRule):
    """Warns about large replicated structs"""
    
    def __init__(self):
        super().__init__(
            "large_replicated_struct",
            "Warns about structs with many replicated fields"
        )
    
    def validate(self, item_type: str, data: Dict[str, Any]) -> List[ValidationIssue]:
        if item_type != "struct":
            return []
        
        issues = []
        fields = data.get("fields", [])
        
        replicated_fields = [
            f for f in fields
            if "replicated" in f.get("attributes", [])
        ]
        
        if len(replicated_fields) > 10:
            issues.append(ValidationIssue(
                severity=IssueSeverity.WARNING,
                category=IssueCategory.PERFORMANCE,
                message=f"Struct '{data.get('name')}' has {len(replicated_fields)} replicated fields. This can impact network performance.",
                fix_suggestion="Consider splitting into multiple structs or using conditional replication."
            ))
        
        return issues


# ============================================================================
# MARKETPLACE RULES
# ============================================================================

class MarketplaceNamingRule(ValidationRule):
    """Validates naming for marketplace submission"""
    
    def __init__(self):
        super().__init__(
            "marketplace_naming",
            "Ensures names are marketplace-friendly"
        )
    
    def validate(self, item_type: str, data: Dict[str, Any]) -> List[ValidationIssue]:
        issues = []
        name = data.get("name", "")
        
        # Check for profanity or inappropriate names
        inappropriate_words = ["test", "temp", "debug", "foo", "bar", "baz"]
        if any(word in name.lower() for word in inappropriate_words):
            issues.append(ValidationIssue(
                severity=IssueSeverity.WARNING,
                category=IssueCategory.NAMING,
                message=f"Name '{name}' contains development placeholder words. Consider using production-ready names for marketplace.",
                fix_suggestion="Use descriptive, professional names for marketplace submission."
            ))
        
        return issues


# ============================================================================
# RULE REGISTRY
# ============================================================================

class RuleRegistry:
    """Central registry for all validation rules"""
    
    def __init__(self):
        self.rules: List[ValidationRule] = []
        self._load_default_rules()
    
    def _load_default_rules(self):
        """Load all default validation rules"""
        self.rules = [
            # Naming rules
            ActorNamingRule(),
            ComponentNamingRule(),
            
            # Replication rules
            ReplicationValidityRule(),
            RpcParameterRule(),
            
            # Blueprint rules
            BlueprintEventConflictRule(),
            
            # Performance rules
            TickFunctionComplexityRule(),
            LargeReplicatedStructRule(),
            
            # Marketplace rules
            MarketplaceNamingRule(),
        ]
    
    def add_rule(self, rule: ValidationRule):
        """Add a custom validation rule"""
        self.rules.append(rule)
    
    def validate(self, item_type: str, data: Dict[str, Any]) -> List[ValidationIssue]:
        """Run all rules against an item"""
        issues = []
        for rule in self.rules:
            try:
                rule_issues = rule.validate(item_type, data)
                issues.extend(rule_issues)
            except Exception as e:
                # Don't let one rule failure break everything
                print(f"Warning: Rule '{rule.name}' failed: {e}")
        return issues


# ============================================================================
# PYTHON API FOR RUST
# ============================================================================

# Global registry instance
_registry = RuleRegistry()


def validate_item(item_type: str, data: Dict[str, Any]) -> List[Dict[str, Any]]:
    """
    Main entry point called from Rust
    
    Args:
        item_type: Type of item ("actor", "struct", "component", etc.)
        data: Item data as dictionary
    
    Returns:
        List of validation issues as dictionaries
    """
    issues = _registry.validate(item_type, data)
    return [issue.to_dict() for issue in issues]


def add_custom_rule(rule: ValidationRule):
    """Add a custom validation rule"""
    _registry.add_rule(rule)


def get_rule_count() -> int:
    """Get number of loaded rules"""
    return len(_registry.rules)


# Example: Custom rule for project-specific validation
class CustomProjectRule(ValidationRule):
    """Example custom rule for project-specific validation"""
    
    def __init__(self):
        super().__init__(
            "custom_project",
            "Project-specific validation rules"
        )
    
    def validate(self, item_type: str, data: Dict[str, Any]) -> List[ValidationIssue]:
        issues = []
        
        # Example: Enforce naming prefix for all actors
        if item_type == "actor":
            name = data.get("name", "")
            if not name.startswith("Game"):
                issues.append(ValidationIssue(
                    severity=IssueSeverity.INFO,
                    category=IssueCategory.NAMING,
                    message=f"Actor '{name}' doesn't start with 'Game' prefix (project convention).",
                    fix_suggestion=f"Rename to 'Game{name}' to follow project conventions."
                ))
        
        return issues


# Uncomment to add custom rules
# add_custom_rule(CustomProjectRule())
