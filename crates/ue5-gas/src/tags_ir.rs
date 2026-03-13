use anyhow::{bail, Result};
use kain_core::ast::{GameplayTagNode, GameplayTagsNamespace};
use std::collections::HashSet;

/// Intermediate representation for GameplayTags
/// Flattens the hierarchical AST into a flat list with full paths
#[derive(Debug, Clone)]
pub struct GameplayTagsIR {
    pub namespaces: Vec<TagNamespaceIR>,
}

#[derive(Debug, Clone)]
pub struct TagNamespaceIR {
    pub name: String,
    pub tags: Vec<GameplayTagIR>,
}

#[derive(Debug, Clone)]
pub struct GameplayTagIR {
    pub tag: String, // Full path: "Ability.Attack.Melee.Sword"
    pub comment: Option<String>,
    pub parent: Option<String>, // "Ability.Attack.Melee"
    pub cpp_name: String,       // "Ability_Attack_Melee_Sword" (for C++ identifier)
}

impl GameplayTagsIR {
    /// Convert AST namespaces to IR
    /// Flattens hierarchy, generates parent tags, validates uniqueness
    pub fn from_ast(namespaces: Vec<GameplayTagsNamespace>) -> Result<Self> {
        let mut ir_namespaces = Vec::new();

        for namespace in namespaces {
            let tags = Self::flatten_hierarchy(&namespace.name, &namespace.children)?;

            ir_namespaces.push(TagNamespaceIR {
                name: namespace.name.clone(),
                tags,
            });
        }

        // Validate no duplicate tags across all namespaces
        Self::validate_no_duplicates(&ir_namespaces)?;

        Ok(GameplayTagsIR {
            namespaces: ir_namespaces,
        })
    }

    /// Flatten tag hierarchy into flat list with full paths
    /// Automatically generates parent tags
    fn flatten_hierarchy(
        _namespace: &str,
        nodes: &[GameplayTagNode],
    ) -> Result<Vec<GameplayTagIR>> {
        let mut tags = Vec::new();
        let mut seen = HashSet::new();

        for node in nodes {
            Self::flatten_node(node, &mut tags, &mut seen)?;
        }

        Ok(tags)
    }

    /// Recursively flatten a single node and its children
    fn flatten_node(
        node: &GameplayTagNode,
        tags: &mut Vec<GameplayTagIR>,
        seen: &mut HashSet<String>,
    ) -> Result<()> {
        // Check for duplicates
        if seen.contains(&node.full_path) {
            bail!("Duplicate tag: {}", node.full_path);
        }
        seen.insert(node.full_path.clone());

        // Extract parent path
        let parent = if node.full_path.contains('.') {
            let parts: Vec<&str> = node.full_path.rsplitn(2, '.').collect();
            if parts.len() == 2 {
                Some(parts[1].to_string())
            } else {
                None
            }
        } else {
            None
        };

        // Generate C++ identifier (replace dots with underscores)
        let cpp_name = node.full_path.replace('.', "_");

        // Add this tag
        tags.push(GameplayTagIR {
            tag: node.full_path.clone(),
            comment: node.comment.clone(),
            parent,
            cpp_name,
        });

        // Recursively flatten children
        for child in &node.children {
            Self::flatten_node(child, tags, seen)?;
        }

        Ok(())
    }

    /// Validate no duplicate tags across all namespaces
    fn validate_no_duplicates(namespaces: &[TagNamespaceIR]) -> Result<()> {
        let mut all_tags = HashSet::new();

        for namespace in namespaces {
            for tag in &namespace.tags {
                if all_tags.contains(&tag.tag) {
                    bail!("Duplicate tag across namespaces: {}", tag.tag);
                }
                all_tags.insert(tag.tag.clone());
            }
        }

        Ok(())
    }

    /// Get all tags as a flat list (across all namespaces)
    pub fn all_tags(&self) -> Vec<&GameplayTagIR> {
        self.namespaces
            .iter()
            .flat_map(|ns| ns.tags.iter())
            .collect()
    }

    /// Get tags for a specific namespace
    pub fn get_namespace(&self, name: &str) -> Option<&TagNamespaceIR> {
        self.namespaces.iter().find(|ns| ns.name == name)
    }
}

impl GameplayTagIR {
    /// Get the namespace path components for C++ namespace generation
    /// "Ability.Attack.Melee" -> ["Ability", "Attack", "Melee"]
    pub fn namespace_parts(&self) -> Vec<String> {
        self.tag.split('.').map(|s| s.to_string()).collect()
    }

    /// Get the leaf name (last component)
    /// "Ability.Attack.Melee" -> "Melee"
    pub fn leaf_name(&self) -> String {
        self.tag.split('.').last().unwrap_or(&self.tag).to_string()
    }
}
