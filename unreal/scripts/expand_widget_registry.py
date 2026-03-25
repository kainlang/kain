#!/usr/bin/env python3
"""
Expand widget_registry.json with missing Slate widgets, property mappings, and composition rules.

This script:
1. Loads the existing widget_registry.json
2. Adds missing commonly-used Slate widgets
3. Adds composition rules for widget validation
4. Adds property constraints and validation rules
5. Validates the expanded registry
"""

import json
import sys
from pathlib import Path
from typing import Dict, List, Any

# Common Slate widgets that should be present
REQUIRED_WIDGETS = [
    # Core layout
    "SBox", "SBorder", "SOverlay", "SCanvas", "SSpacer", "SScaleBox",
    "SHorizontalBox", "SVerticalBox", "SScrollBox", "SGridPanel",
    
    # Input widgets
    "SButton", "SCheckBox", "SSlider", "SEditableText", "SEditableTextBox",
    "SMultiLineEditableText", "SComboBox", "SComboButton", "SSpinBox",
    "SNumericEntryBox", "SSearchBox",
    
    # Display widgets
    "STextBlock", "SImage", "SProgressBar", "SCircularThrobber",
    "SRichTextBlock", "SHyperlink",
    
    # List/Tree widgets
    "SListView", "STreeView", "STileView", "STableRow", "SHeaderRow",
    
    # Advanced widgets
    "SColorPicker", "SColorBlock", "SColorWheel", "SNotificationList",
    "SWidgetSwitcher", "SSplitter",
    
    # Editor-specific
    "SEditorViewport", "SViewport", "SDockTab", "SWindow",
]

# Composition rules for common widgets
COMPOSITION_RULES = {
    "slot_requirements": {
        "SBorder": {
            "required_slots": ["Content"],
            "max_children": 1,
            "slot_type": "single",
            "description": "SBorder can only contain a single child widget"
        },
        "SBox": {
            "required_slots": ["Content"],
            "max_children": 1,
            "slot_type": "single",
            "description": "SBox can only contain a single child widget"
        },
        "SOverlay": {
            "required_slots": [],
            "max_children": -1,
            "slot_type": "multi",
            "description": "SOverlay can contain multiple overlapping children"
        },
        "SVerticalBox": {
            "required_slots": [],
            "max_children": -1,
            "slot_type": "multi",
            "description": "SVerticalBox arranges children vertically"
        },
        "SHorizontalBox": {
            "required_slots": [],
            "max_children": -1,
            "slot_type": "multi",
            "description": "SHorizontalBox arranges children horizontally"
        },
        "SScrollBox": {
            "required_slots": [],
            "max_children": -1,
            "slot_type": "multi",
            "description": "SScrollBox provides scrolling for its children"
        },
        "SGridPanel": {
            "required_slots": [],
            "max_children": -1,
            "slot_type": "grid",
            "description": "SGridPanel arranges children in a grid with row/column indices"
        },
        "SCanvas": {
            "required_slots": [],
            "max_children": -1,
            "slot_type": "canvas",
            "description": "SCanvas allows absolute positioning of children"
        },
        "SSplitter": {
            "required_slots": [],
            "max_children": -1,
            "slot_type": "multi",
            "description": "SSplitter divides space between children with resizable dividers"
        },
        "SWidgetSwitcher": {
            "required_slots": [],
            "max_children": -1,
            "slot_type": "switcher",
            "description": "SWidgetSwitcher shows one child at a time based on active index"
        },
        "SButton": {
            "required_slots": ["Content"],
            "max_children": 1,
            "slot_type": "single",
            "description": "SButton can contain a single child widget as its content"
        },
        "SCheckBox": {
            "required_slots": ["Content"],
            "max_children": 1,
            "slot_type": "single",
            "description": "SCheckBox can contain a single child widget as its label"
        },
        "SComboButton": {
            "required_slots": ["ButtonContent", "MenuContent"],
            "max_children": 2,
            "slot_type": "named",
            "description": "SComboButton requires ButtonContent and MenuContent slots"
        },
        "SDockTab": {
            "required_slots": ["Content"],
            "max_children": 1,
            "slot_type": "single",
            "description": "SDockTab can contain a single child widget as its content"
        },
        "SWindow": {
            "required_slots": ["Content"],
            "max_children": 1,
            "slot_type": "single",
            "description": "SWindow can contain a single child widget as its content"
        },
        "SViewport": {
            "required_slots": ["Content"],
            "max_children": 1,
            "slot_type": "single",
            "description": "SViewport can optionally contain overlay content"
        }
    },
    "property_dependencies": {
        "SSlider": {
            "Value": {
                "requires": ["MinValue", "MaxValue"],
                "validation": "Value >= MinValue && Value <= MaxValue",
                "description": "Slider value must be between min and max"
            }
        },
        "SSpinBox": {
            "Value": {
                "requires": ["MinValue", "MaxValue"],
                "validation": "Value >= MinValue && Value <= MaxValue",
                "description": "SpinBox value must be between min and max"
            }
        },
        "SProgressBar": {
            "Percent": {
                "requires": [],
                "validation": "Percent >= 0.0 && Percent <= 1.0",
                "description": "Progress percent must be between 0.0 and 1.0"
            }
        },
        "SGridPanel": {
            "Slot": {
                "requires": ["Row", "Column"],
                "validation": "Row >= 0 && Column >= 0",
                "description": "Grid slots require non-negative row and column indices"
            }
        },
        "SWidgetSwitcher": {
            "WidgetIndex": {
                "requires": [],
                "validation": "WidgetIndex >= 0 && WidgetIndex < NumChildren",
                "description": "Active widget index must be valid"
            }
        }
    },
    "property_exclusions": {
        "SButton": {
            "mutually_exclusive": [
                ["Text", "Content"]
            ],
            "description": "Button can have either Text or Content, not both"
        },
        "STextBlock": {
            "mutually_exclusive": [
                ["Text", "Content"]
            ],
            "description": "TextBlock can have either Text or Content, not both"
        }
    },
    "parent_child_compatibility": {
        "SPanel": {
            "can_contain": ["*"],
            "description": "Panels can contain any widget"
        },
        "SLeafWidget": {
            "can_contain": [],
            "description": "Leaf widgets cannot contain children"
        },
        "SCompoundWidget": {
            "can_contain": ["*"],
            "description": "Compound widgets can contain children based on their slots"
        }
    }
}

# Property type constraints
PROPERTY_CONSTRAINTS = {
    "float": {
        "validation": "is_finite",
        "default_range": {"min": 0.0, "max": 1.0}
    },
    "int32": {
        "validation": "is_integer",
        "default_range": {"min": 0, "max": 100}
    },
    "FLinearColor": {
        "validation": "is_color",
        "components": ["R", "G", "B", "A"],
        "component_range": {"min": 0.0, "max": 1.0}
    },
    "FVector2D": {
        "validation": "is_vector2d",
        "components": ["X", "Y"]
    },
    "FMargin": {
        "validation": "is_margin",
        "components": ["Left", "Top", "Right", "Bottom"]
    },
    "EHorizontalAlignment": {
        "validation": "is_enum",
        "values": ["HAlign_Fill", "HAlign_Left", "HAlign_Center", "HAlign_Right"]
    },
    "EVerticalAlignment": {
        "validation": "is_enum",
        "values": ["VAlign_Fill", "VAlign_Top", "VAlign_Center", "VAlign_Bottom"]
    },
    "EOrientation": {
        "validation": "is_enum",
        "values": ["Orient_Horizontal", "Orient_Vertical"]
    },
    "EVisibility": {
        "validation": "is_enum",
        "values": ["Visible", "Collapsed", "Hidden", "HitTestInvisible", "SelfHitTestInvisible"]
    }
}

def load_widget_registry(path: Path) -> Dict[str, Any]:
    """Load the existing widget registry."""
    print(f"Loading widget registry from {path}...")
    with open(path, 'r', encoding='utf-8') as f:
        return json.load(f)

def verify_required_widgets(registry: Dict[str, Any]) -> List[str]:
    """Check which required widgets are missing."""
    widgets = registry.get("widgets", {})
    missing = []
    
    for widget_name in REQUIRED_WIDGETS:
        if widget_name not in widgets:
            missing.append(widget_name)
    
    return missing

def add_composition_rules(registry: Dict[str, Any]) -> Dict[str, Any]:
    """Add composition rules to the registry."""
    print("Adding composition rules...")
    
    if "composition_rules" not in registry:
        registry["composition_rules"] = {}
    
    # Merge composition rules
    for rule_type, rules in COMPOSITION_RULES.items():
        if rule_type not in registry["composition_rules"]:
            registry["composition_rules"][rule_type] = {}
        
        registry["composition_rules"][rule_type].update(rules)
    
    return registry

def add_property_constraints(registry: Dict[str, Any]) -> Dict[str, Any]:
    """Add property type constraints to the registry."""
    print("Adding property constraints...")
    
    if "property_constraints" not in registry:
        registry["property_constraints"] = {}
    
    registry["property_constraints"].update(PROPERTY_CONSTRAINTS)
    
    return registry

def add_missing_widgets(registry: Dict[str, Any], missing: List[str]) -> Dict[str, Any]:
    """Add placeholder entries for missing widgets."""
    if not missing:
        return registry
    
    print(f"Adding {len(missing)} missing widgets...")
    
    widgets = registry.get("widgets", {})
    
    for widget_name in missing:
        # Add basic placeholder - these should be filled in from actual UE5 headers
        widgets[widget_name] = {
            "name": widget_name,
            "parent": "SCompoundWidget",  # Default parent
            "header": f"{widget_name}.h",
            "properties": {},
            "events": {},
            "slots": [],
            "note": "Placeholder - needs verification against UE5 headers"
        }
    
    registry["widgets"] = widgets
    return registry

def validate_registry(registry: Dict[str, Any]) -> bool:
    """Validate the registry structure."""
    print("Validating registry structure...")
    
    required_sections = ["widgets", "delegates"]
    for section in required_sections:
        if section not in registry:
            print(f"ERROR: Missing required section: {section}")
            return False
    
    # Check widgets structure
    widgets = registry["widgets"]
    for widget_name, widget_data in list(widgets.items())[:10]:  # Sample first 10
        required_fields = ["name", "parent", "header", "properties", "events", "slots"]
        for field in required_fields:
            if field not in widget_data:
                print(f"WARNING: Widget {widget_name} missing field: {field}")
    
    print(f"Registry contains {len(widgets)} widgets")
    print(f"Registry contains {len(registry.get('delegates', {}))} delegates")
    
    if "composition_rules" in registry:
        print(f"Composition rules: {len(registry['composition_rules'])} categories")
    
    if "property_constraints" in registry:
        print(f"Property constraints: {len(registry['property_constraints'])} types")
    
    return True

def save_widget_registry(registry: Dict[str, Any], path: Path) -> None:
    """Save the expanded registry."""
    print(f"Saving expanded registry to {path}...")
    
    # Create backup
    backup_path = path.with_suffix('.json.backup')
    if path.exists():
        import shutil
        shutil.copy2(path, backup_path)
        print(f"Backup created: {backup_path}")
    
    with open(path, 'w', encoding='utf-8') as f:
        json.dump(registry, f, indent=2, ensure_ascii=False)
    
    print(f"Registry saved successfully")

def main():
    """Main expansion workflow."""
    # Determine paths
    script_dir = Path(__file__).parent
    metadata_dir = script_dir.parent / "metadata"
    registry_path = metadata_dir / "widget_registry.json"
    
    if not registry_path.exists():
        print(f"ERROR: Widget registry not found at {registry_path}")
        return 1
    
    # Load existing registry
    registry = load_widget_registry(registry_path)
    
    # Verify required widgets
    missing = verify_required_widgets(registry)
    if missing:
        print(f"\nMissing {len(missing)} required widgets:")
        for widget in missing[:10]:  # Show first 10
            print(f"  - {widget}")
        if len(missing) > 10:
            print(f"  ... and {len(missing) - 10} more")
    else:
        print("\nAll required widgets are present!")
    
    # Add missing widgets (as placeholders)
    registry = add_missing_widgets(registry, missing)
    
    # Add composition rules
    registry = add_composition_rules(registry)
    
    # Add property constraints
    registry = add_property_constraints(registry)
    
    # Validate
    if not validate_registry(registry):
        print("\nERROR: Registry validation failed")
        return 1
    
    # Save
    save_widget_registry(registry, registry_path)
    
    print("\n✓ Widget registry expansion complete!")
    print(f"  - Total widgets: {len(registry['widgets'])}")
    print(f"  - Total delegates: {len(registry.get('delegates', {}))}")
    print(f"  - Composition rules: {len(registry.get('composition_rules', {}))}")
    print(f"  - Property constraints: {len(registry.get('property_constraints', {}))}")
    
    return 0

if __name__ == "__main__":
    sys.exit(main())
