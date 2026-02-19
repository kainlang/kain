use cli::packager::dependencies::{DependencyResolver, Dependencies};
use std::path::PathBuf;

#[test]
fn test_dependency_resolver_creation() {
    let resolver = DependencyResolver::new();
    let module_map = DependencyResolver::default_module_map();
    assert!(module_map.len() > 0);
}

#[test]
fn test_include_parsing() {
    let resolver = DependencyResolver::new();
    
    // Test extracting include from #include statement
    let include1 = resolver.extract_include("#include \"CoreMinimal.h\"");
    assert_eq!(include1, Some("CoreMinimal.h".to_string()));
    
    let include2 = resolver.extract_include("#include <Engine/Engine.h>");
    assert_eq!(include2, Some("Engine/Engine.h".to_string()));
}

#[test]
fn test_analyze_simple_file() {
    let resolver = DependencyResolver::new();
    
    let test_content = r#"
#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "RenderResource.h"

class AMyActor : public AActor {
    // ...
};
"#;
    
    let files = vec![(PathBuf::from("MyActor.h"), test_content.to_string())];
    let deps = resolver.analyze(&files).expect("Should analyze successfully");
    
    // Should detect Core, Engine, and RenderCore modules
    assert!(deps.public_modules.contains("Core") || deps.private_modules.contains("Core"));
    assert!(deps.public_modules.contains("Engine") || deps.private_modules.contains("Engine"));
}

#[test]
fn test_automatic_module_addition() {
    let resolver = DependencyResolver::new();
    let mut deps = Dependencies::new();
    
    // Test shader module addition
    resolver.add_automatic_modules(&mut deps, true, false, false, false, false, false, false);
    assert!(deps.public_modules.contains("RenderCore"));
    assert!(deps.public_modules.contains("RHI"));
    
    // Test slate module addition
    let mut deps2 = Dependencies::new();
    resolver.add_automatic_modules(&mut deps2, false, true, false, false, false, false, false);
    assert!(deps2.private_modules.contains("Slate"));
    assert!(deps2.private_modules.contains("SlateCore"));
    
    // Test networking module addition
    let mut deps3 = Dependencies::new();
    resolver.add_automatic_modules(&mut deps3, false, false, false, false, false, false, true);
    assert!(deps3.public_modules.contains("Engine"));
    assert!(deps3.public_modules.contains("NetCore"));
}

#[test]
fn test_circular_dependency_detection() {
    // Create a resolver with a circular dependency
    let mut module_map = std::collections::HashMap::new();
    module_map.insert("ModuleA".to_string(), vec!["ModuleB".to_string()]);
    module_map.insert("ModuleB".to_string(), vec!["ModuleC".to_string()]);
    module_map.insert("ModuleC".to_string(), vec!["ModuleA".to_string()]); // Creates cycle
    
    let resolver = DependencyResolver {
        module_map,
        include_to_modules: std::collections::HashMap::new(),
    };
    
    let mut deps = Dependencies::new();
    deps.public_modules.insert("ModuleA".to_string());
    
    // Should detect the circular dependency
    let result = resolver.validate_dependencies(&mut deps);
    assert!(result.is_err());
    assert!(result.unwrap_err().to_string().contains("Circular dependency"));
}
