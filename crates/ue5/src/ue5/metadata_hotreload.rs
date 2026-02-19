// Metadata hot-reload system for KAIN compiler
// Watches metadata files and reloads them when changed

use std::path::{Path, PathBuf};
use std::sync::{Arc, Mutex};
use std::time::{Duration, SystemTime};
use std::collections::HashMap;
use std::fs;

use crate::ue5::engine_knowledge::EngineKnowledge;
use crate::ue5::metadata_validation::MetadataValidator;

/// Metadata file watcher that tracks modification times
pub struct MetadataWatcher {
    /// Directory containing metadata files
    metadata_dir: PathBuf,
    
    /// Last modification times for each file
    file_mtimes: HashMap<PathBuf, SystemTime>,
    
    /// Validator for checking metadata before applying
    validator: MetadataValidator,
    
    /// Whether hot-reload is enabled
    enabled: bool,
}

impl MetadataWatcher {
    /// Create a new metadata watcher
    pub fn new(metadata_dir: impl AsRef<Path>) -> Self {
        Self {
            metadata_dir: metadata_dir.as_ref().to_path_buf(),
            file_mtimes: HashMap::new(),
            validator: MetadataValidator::new(),
            enabled: true,
        }
    }
    
    /// Enable or disable hot-reload
    pub fn set_enabled(&mut self, enabled: bool) {
        self.enabled = enabled;
    }
    
    /// Check if hot-reload is enabled
    pub fn is_enabled(&self) -> bool {
        self.enabled
    }
    
    /// Initialize the watcher by recording current modification times
    pub fn initialize(&mut self) -> Result<(), String> {
        if !self.metadata_dir.exists() {
            return Err(format!("Metadata directory not found: {:?}", self.metadata_dir));
        }
        
        // Scan all JSON files in metadata directory
        let entries = fs::read_dir(&self.metadata_dir)
            .map_err(|e| format!("Failed to read metadata directory: {}", e))?;
        
        for entry in entries {
            let entry = entry.map_err(|e| format!("Failed to read directory entry: {}", e))?;
            let path = entry.path();
            
            // Only track JSON files
            if path.extension().and_then(|s| s.to_str()) == Some("json") {
                if let Ok(metadata) = fs::metadata(&path) {
                    if let Ok(mtime) = metadata.modified() {
                        self.file_mtimes.insert(path, mtime);
                    }
                }
            }
        }
        
        Ok(())
    }
    
    /// Check for modified files and return list of changed files
    pub fn check_for_changes(&mut self) -> Result<Vec<PathBuf>, String> {
        if !self.enabled {
            return Ok(Vec::new());
        }
        
        let mut changed_files = Vec::new();
        
        // Check each tracked file
        for (path, old_mtime) in &self.file_mtimes {
            if let Ok(metadata) = fs::metadata(path) {
                if let Ok(new_mtime) = metadata.modified() {
                    if new_mtime > *old_mtime {
                        changed_files.push(path.clone());
                    }
                }
            }
        }
        
        // Check for new files
        let entries = fs::read_dir(&self.metadata_dir)
            .map_err(|e| format!("Failed to read metadata directory: {}", e))?;
        
        for entry in entries {
            let entry = entry.map_err(|e| format!("Failed to read directory entry: {}", e))?;
            let path = entry.path();
            
            if path.extension().and_then(|s| s.to_str()) == Some("json") {
                if !self.file_mtimes.contains_key(&path) {
                    changed_files.push(path.clone());
                }
            }
        }
        
        Ok(changed_files)
    }
    
    /// Update modification time for a file
    fn update_mtime(&mut self, path: &Path) -> Result<(), String> {
        if let Ok(metadata) = fs::metadata(path) {
            if let Ok(mtime) = metadata.modified() {
                self.file_mtimes.insert(path.to_path_buf(), mtime);
                return Ok(());
            }
        }
        Err(format!("Failed to get modification time for {:?}", path))
    }
    
    /// Validate a metadata file before applying changes
    pub fn validate_file(&self, path: &Path) -> Result<(), String> {
        let content = fs::read_to_string(path)
            .map_err(|e| format!("Failed to read file {:?}: {}", path, e))?;
        
        // Validate JSON syntax and schema
        self.validator.validate_file(path, &content)
            .map_err(|e| format!("Validation failed for {:?}: {}", path, e))?;
        
        Ok(())
    }
    
    /// Reload a metadata file into EngineKnowledge
    pub fn reload_file(&mut self, path: &Path, knowledge: &mut EngineKnowledge) -> Result<(), String> {
        // Validate before loading
        self.validate_file(path)?;
        
        // Read file content
        let content = fs::read_to_string(path)
            .map_err(|e| format!("Failed to read file {:?}: {}", path, e))?;
        
        // Determine file type and load appropriately
        let filename = path.file_name()
            .and_then(|s| s.to_str())
            .ok_or_else(|| format!("Invalid filename: {:?}", path))?;
        
        if filename.starts_with("engine_") && filename.ends_with("_scanned.json") {
            // Engine scan file
            knowledge.load_metadata_validated(path, &content)?;
        } else if filename == "engine_knowledge.json" {
            // Curated engine knowledge
            knowledge.load_metadata_validated(path, &content)?;
        } else {
            // Other metadata files - for now just validate
            // TODO: Add support for module_graph, uht_rules, etc.
            return Ok(());
        }
        
        // Update modification time
        self.update_mtime(path)?;
        
        Ok(())
    }
    
    /// Check for changes and reload modified files
    pub fn check_and_reload(&mut self, knowledge: &mut EngineKnowledge) -> Result<Vec<PathBuf>, String> {
        let changed_files = self.check_for_changes()?;
        
        if changed_files.is_empty() {
            return Ok(Vec::new());
        }
        
        let mut reloaded = Vec::new();
        
        for path in &changed_files {
            match self.reload_file(path, knowledge) {
                Ok(()) => {
                    reloaded.push(path.clone());
                }
                Err(e) => {
                    eprintln!("Warning: Failed to reload {:?}: {}", path, e);
                    // Don't fail the entire reload if one file fails
                }
            }
        }
        
        Ok(reloaded)
    }
}

/// Hot-reload manager that can be shared across threads
pub struct HotReloadManager {
    watcher: Arc<Mutex<MetadataWatcher>>,
    knowledge: Arc<Mutex<EngineKnowledge>>,
}

impl HotReloadManager {
    /// Create a new hot-reload manager
    pub fn new(metadata_dir: impl AsRef<Path>, knowledge: Arc<Mutex<EngineKnowledge>>) -> Result<Self, String> {
        let mut watcher = MetadataWatcher::new(metadata_dir);
        watcher.initialize()?;
        
        Ok(Self {
            watcher: Arc::new(Mutex::new(watcher)),
            knowledge,
        })
    }
    
    /// Enable or disable hot-reload
    pub fn set_enabled(&self, enabled: bool) {
        if let Ok(mut watcher) = self.watcher.lock() {
            watcher.set_enabled(enabled);
        }
    }
    
    /// Check for changes and reload if needed
    pub fn check_and_reload(&self) -> Result<Vec<PathBuf>, String> {
        let mut watcher = self.watcher.lock()
            .map_err(|e| format!("Failed to lock watcher: {}", e))?;
        
        let mut knowledge = self.knowledge.lock()
            .map_err(|e| format!("Failed to lock knowledge: {}", e))?;
        
        watcher.check_and_reload(&mut knowledge)
    }
    
    /// Start a background thread that periodically checks for changes
    pub fn start_background_watcher(self, interval: Duration) -> std::thread::JoinHandle<()> {
        std::thread::spawn(move || {
            loop {
                std::thread::sleep(interval);
                
                match self.check_and_reload() {
                    Ok(reloaded) => {
                        if !reloaded.is_empty() {
                            println!("Hot-reloaded {} metadata file(s):", reloaded.len());
                            for path in reloaded {
                                println!("  - {:?}", path.file_name().unwrap_or_default());
                            }
                        }
                    }
                    Err(e) => {
                        eprintln!("Hot-reload check failed: {}", e);
                    }
                }
            }
        })
    }
}

#[cfg(test)]
mod tests {
    use super::*;
    use std::fs;
    use std::io::Write;
    use tempfile::TempDir;
    
    #[test]
    fn test_watcher_creation() {
        let temp_dir = TempDir::new().unwrap();
        let watcher = MetadataWatcher::new(temp_dir.path());
        assert!(watcher.is_enabled());
    }
    
    #[test]
    fn test_watcher_initialization() {
        let temp_dir = TempDir::new().unwrap();
        
        // Create a test JSON file
        let test_file = temp_dir.path().join("test.json");
        fs::write(&test_file, "{}").unwrap();
        
        let mut watcher = MetadataWatcher::new(temp_dir.path());
        assert!(watcher.initialize().is_ok());
        assert_eq!(watcher.file_mtimes.len(), 1);
    }
    
    #[test]
    fn test_change_detection() {
        let temp_dir = TempDir::new().unwrap();
        
        // Create initial file
        let test_file = temp_dir.path().join("test.json");
        fs::write(&test_file, "{}").unwrap();
        
        let mut watcher = MetadataWatcher::new(temp_dir.path());
        watcher.initialize().unwrap();
        
        // No changes initially
        let changes = watcher.check_for_changes().unwrap();
        assert_eq!(changes.len(), 0);
        
        // Wait a bit to ensure different mtime
        std::thread::sleep(Duration::from_millis(100));
        
        // Modify file
        fs::write(&test_file, "{\"modified\": true}").unwrap();
        
        // Should detect change
        let changes = watcher.check_for_changes().unwrap();
        assert_eq!(changes.len(), 1);
        assert_eq!(changes[0], test_file);
    }
    
    #[test]
    fn test_new_file_detection() {
        let temp_dir = TempDir::new().unwrap();
        
        let mut watcher = MetadataWatcher::new(temp_dir.path());
        watcher.initialize().unwrap();
        
        // Create new file
        let new_file = temp_dir.path().join("new.json");
        fs::write(&new_file, "{}").unwrap();
        
        // Should detect new file
        let changes = watcher.check_for_changes().unwrap();
        assert_eq!(changes.len(), 1);
        assert_eq!(changes[0], new_file);
    }
    
    #[test]
    fn test_disable_hotreload() {
        let temp_dir = TempDir::new().unwrap();
        
        let test_file = temp_dir.path().join("test.json");
        fs::write(&test_file, "{}").unwrap();
        
        let mut watcher = MetadataWatcher::new(temp_dir.path());
        watcher.initialize().unwrap();
        watcher.set_enabled(false);
        
        // Modify file
        std::thread::sleep(Duration::from_millis(100));
        fs::write(&test_file, "{\"modified\": true}").unwrap();
        
        // Should not detect changes when disabled
        let changes = watcher.check_for_changes().unwrap();
        assert_eq!(changes.len(), 0);
    }
}
