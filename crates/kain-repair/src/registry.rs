use crate::{AppliedFix, RepairMode, RepairProfile};

#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub enum RuleSafety {
    Safe,
    Aggressive,
}

#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub enum RuleScope {
    Generic,
    ProfileSensitive,
    Code,
    Class,
}

#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub struct RuleMetadata {
    pub id: &'static str,
    pub kind: crate::FixKind,
    pub safety: RuleSafety,
    pub scope: RuleScope,
}

#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub struct RepairRule {
    pub metadata: RuleMetadata,
    pub enabled_in_apply_safe: bool,
    pub enabled_in_apply_aggressive: bool,
    pub enabled_in_dry_run: bool,
    pub enabled_in_suggest: bool,
    pub profile_gate: fn(&RepairProfile) -> bool,
    pub apply: fn(&str, RepairMode) -> Option<(String, AppliedFix)>,
}

pub fn registry() -> &'static [RepairRule] {
    &RULES
}

pub fn selected_rules(profile: &RepairProfile, mode: RepairMode) -> Vec<&'static RepairRule> {
    registry()
        .iter()
        .filter(|rule| (rule.profile_gate)(profile))
        .filter(|rule| match mode {
            RepairMode::Check => rule.enabled_in_dry_run,
            RepairMode::Suggest => rule.enabled_in_suggest,
            RepairMode::ApplySafe => rule.enabled_in_apply_safe,
            RepairMode::ApplyAggressive => rule.enabled_in_apply_aggressive,
        })
        .collect()
}

fn line_endings(_: &RepairProfile) -> bool {
    true
}
fn trailing_whitespace(_: &RepairProfile) -> bool {
    true
}
fn blank_lines(_: &RepairProfile) -> bool {
    true
}
fn block_comments(profile: &RepairProfile) -> bool {
    profile.fix_unterminated_block_comments
}
fn indentation(profile: &RepairProfile) -> bool {
    profile.normalize_indentation
}
fn declaration_headers(profile: &RepairProfile) -> bool {
    profile.normalize_declaration_headers
}
fn flatten_nested_declarations(profile: &RepairProfile) -> bool {
    profile.flatten_nested_declarations
}
fn reserved_identifiers(profile: &RepairProfile) -> bool {
    profile.rewrite_reserved_identifiers
}
fn self_ctor(profile: &RepairProfile) -> bool {
    profile.normalize_self_constructor_syntax
}
fn inline_initializers(profile: &RepairProfile) -> bool {
    profile.rewrite_inline_initializers
}
fn namespace_paths(profile: &RepairProfile) -> bool {
    profile.normalize_namespace_paths
}
fn parser_safe_blocks(profile: &RepairProfile) -> bool {
    profile.reconstruct_parser_safe_blocks
}

const RULES: [RepairRule; 12] = [
    RepairRule {
        metadata: RuleMetadata {
            id: "normalize_line_endings",
            kind: crate::FixKind::NormalizeLineEndings,
            safety: RuleSafety::Safe,
            scope: RuleScope::Generic,
        },
        enabled_in_apply_safe: true,
        enabled_in_apply_aggressive: true,
        enabled_in_dry_run: true,
        enabled_in_suggest: true,
        profile_gate: line_endings,
        apply: crate::engine::apply_normalize_line_endings,
    },
    RepairRule {
        metadata: RuleMetadata {
            id: "trim_trailing_whitespace",
            kind: crate::FixKind::TrimTrailingWhitespace,
            safety: RuleSafety::Safe,
            scope: RuleScope::Generic,
        },
        enabled_in_apply_safe: true,
        enabled_in_apply_aggressive: true,
        enabled_in_dry_run: true,
        enabled_in_suggest: true,
        profile_gate: trailing_whitespace,
        apply: crate::engine::apply_trim_trailing_whitespace,
    },
    RepairRule {
        metadata: RuleMetadata {
            id: "collapse_extra_blank_lines",
            kind: crate::FixKind::CollapseExtraBlankLines,
            safety: RuleSafety::Safe,
            scope: RuleScope::Generic,
        },
        enabled_in_apply_safe: true,
        enabled_in_apply_aggressive: true,
        enabled_in_dry_run: true,
        enabled_in_suggest: true,
        profile_gate: blank_lines,
        apply: crate::engine::apply_collapse_extra_blank_lines,
    },
    RepairRule {
        metadata: RuleMetadata {
            id: "close_unterminated_block_comment",
            kind: crate::FixKind::CloseUnterminatedBlockComment,
            safety: RuleSafety::Safe,
            scope: RuleScope::Class,
        },
        enabled_in_apply_safe: true,
        enabled_in_apply_aggressive: true,
        enabled_in_dry_run: false,
        enabled_in_suggest: true,
        profile_gate: block_comments,
        apply: crate::engine::apply_close_unterminated_block_comment,
    },
    RepairRule {
        metadata: RuleMetadata {
            id: "normalize_indentation",
            kind: crate::FixKind::NormalizeIndentation,
            safety: RuleSafety::Safe,
            scope: RuleScope::Code,
        },
        enabled_in_apply_safe: true,
        enabled_in_apply_aggressive: true,
        enabled_in_dry_run: true,
        enabled_in_suggest: true,
        profile_gate: indentation,
        apply: crate::engine::apply_normalize_indentation,
    },
    RepairRule {
        metadata: RuleMetadata {
            id: "normalize_declaration_headers",
            kind: crate::FixKind::NormalizeDeclarationHeader,
            safety: RuleSafety::Safe,
            scope: RuleScope::Class,
        },
        enabled_in_apply_safe: true,
        enabled_in_apply_aggressive: true,
        enabled_in_dry_run: true,
        enabled_in_suggest: true,
        profile_gate: declaration_headers,
        apply: crate::engine::apply_normalize_declaration_headers,
    },
    RepairRule {
        metadata: RuleMetadata {
            id: "flatten_nested_declaration_placement",
            kind: crate::FixKind::FlattenNestedDeclarationPlacement,
            safety: RuleSafety::Safe,
            scope: RuleScope::Class,
        },
        enabled_in_apply_safe: true,
        enabled_in_apply_aggressive: true,
        enabled_in_dry_run: true,
        enabled_in_suggest: true,
        profile_gate: flatten_nested_declarations,
        apply: crate::engine::apply_flatten_nested_declaration_placement,
    },
    RepairRule {
        metadata: RuleMetadata {
            id: "rewrite_reserved_identifiers",
            kind: crate::FixKind::RewriteReservedIdentifier,
            safety: RuleSafety::Aggressive,
            scope: RuleScope::Class,
        },
        enabled_in_apply_safe: false,
        enabled_in_apply_aggressive: true,
        enabled_in_dry_run: true,
        enabled_in_suggest: true,
        profile_gate: reserved_identifiers,
        apply: crate::engine::apply_rewrite_reserved_identifiers,
    },
    RepairRule {
        metadata: RuleMetadata {
            id: "normalize_self_constructor_syntax",
            kind: crate::FixKind::NormalizeSelfConstructorSyntax,
            safety: RuleSafety::Aggressive,
            scope: RuleScope::Code,
        },
        enabled_in_apply_safe: false,
        enabled_in_apply_aggressive: true,
        enabled_in_dry_run: true,
        enabled_in_suggest: true,
        profile_gate: self_ctor,
        apply: crate::engine::apply_normalize_self_constructor_syntax,
    },
    RepairRule {
        metadata: RuleMetadata {
            id: "rewrite_inline_initializers",
            kind: crate::FixKind::RewriteInlineInitialization,
            safety: RuleSafety::Aggressive,
            scope: RuleScope::ProfileSensitive,
        },
        enabled_in_apply_safe: false,
        enabled_in_apply_aggressive: true,
        enabled_in_dry_run: true,
        enabled_in_suggest: true,
        profile_gate: inline_initializers,
        apply: crate::engine::apply_rewrite_inline_initializers,
    },
    RepairRule {
        metadata: RuleMetadata {
            id: "normalize_namespace_paths",
            kind: crate::FixKind::NormalizeNamespacePath,
            safety: RuleSafety::Aggressive,
            scope: RuleScope::ProfileSensitive,
        },
        enabled_in_apply_safe: false,
        enabled_in_apply_aggressive: true,
        enabled_in_dry_run: true,
        enabled_in_suggest: true,
        profile_gate: namespace_paths,
        apply: crate::engine::apply_normalize_namespace_paths,
    },
    RepairRule {
        metadata: RuleMetadata {
            id: "reconstruct_parser_safe_blocks",
            kind: crate::FixKind::ReconstructParserSafeBlock,
            safety: RuleSafety::Aggressive,
            scope: RuleScope::Code,
        },
        enabled_in_apply_safe: false,
        enabled_in_apply_aggressive: true,
        enabled_in_dry_run: true,
        enabled_in_suggest: true,
        profile_gate: parser_safe_blocks,
        apply: crate::engine::apply_reconstruct_parser_safe_blocks,
    },
];
