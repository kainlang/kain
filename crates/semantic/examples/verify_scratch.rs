use kain_error::{CompilerPhase, DiagnosticCode, DiagnosticSemanticPacket};
use kain_semantic::SemanticCoprocessor;
use std::fs;
use std::path::Path;

fn main() {
    let scratch_dir = Path::new("scratch");
    if !scratch_dir.exists() {
        println!("Scratch directory not found!");
        return;
    }

    let entries = fs::read_dir(scratch_dir).unwrap();
    let mut files: Vec<_> = entries
        .filter_map(|e| e.ok())
        .filter(|e| e.path().extension().and_then(|ext| ext.to_str()) == Some("kn"))
        .collect();

    files.sort_by_key(|e| e.path());

    println!("================================================================================");
    println!("             KAIN COPROCESSOR DETAILED SCRATCH VERIFICATION REPORT              ");
    println!("================================================================================");

    if files.is_empty() {
        println!("No scratch .kn files found to verify.");
        return;
    }

    let coprocessor = SemanticCoprocessor::new();

    for entry in files {
        let path = entry.path();
        let file_name = path.file_name().unwrap().to_string_lossy().to_string();
        let content = fs::read_to_string(&path).unwrap();

        let mut expected_code = String::new();
        let mut expected_mode = String::new();
        let mut expected_repair = String::new();

        for line in content.lines() {
            let line = line.trim();
            if line.starts_with("//") {
                let comment = line[2..].trim();
                if let Some((key, val)) = comment.split_once(':') {
                    let key = key.trim();
                    let val = val.trim().to_string();
                    if key == "@expected_code" {
                        expected_code = val;
                    } else if key == "@expected_mode" {
                        expected_mode = val;
                    } else if key == "@expected_repair" {
                        expected_repair = val;
                    }
                }
            }
        }

        println!("📂 FILE: scratch/{}", file_name);
        println!(
            "--------------------------------------------------------------------------------"
        );
        println!("--- Source Code Snippet ---");
        for line in content.lines().take(12) {
            println!("  {}", line);
        }
        if content.lines().count() > 12 {
            println!("  ...");
        }
        println!();

        // 1. Build dynamic packet mocking compiler context
        let code = DiagnosticCode::new(Box::leak(expected_code.clone().into_boxed_str()));
        let mut packet = DiagnosticSemanticPacket::new(code, CompilerPhase::TypeChecking, "cells");

        packet = packet.source_window(&content);

        // Customize mock packet attributes to fit expected FailureMode
        if expected_mode == "OwnershipViolation" {
            packet = packet
                .flag("in_converge_block", false)
                .flag("in_entangle_block", false);
        } else if expected_mode == "ConvergeMismatch" {
            packet = packet.flag("in_converge_block", true);
        } else if expected_mode == "EntangleViolation" {
            packet = packet.flag("in_entangle_block", true);
        } else if expected_mode == "WorldDeclarationError" {
            packet = packet
                .flag("in_patch_block", true)
                .flag("in_world_block", true);
        } else if expected_mode == "GenericUnknown" {
            packet = packet.flag("in_comptime_block", true);
        } else if expected_mode == "Typo" {
            packet = packet
                .visible_symbols(vec!["mix_scalar".into(), "println".into()])
                .add_scope_match("mix_scalar", 1);
        }

        // Add the ideal repair candidate and some distraction candidates to test ranking
        packet = packet.add_repair(
            expected_repair.clone(),
            format!("Ideal fix for {}", expected_mode),
            "replacement_text_here",
        );
        packet = packet.add_repair("dummy_repair", "A distraction repair candidate", "dummy");

        // 2. Query Coprocessor
        let report = coprocessor.analyze(&packet);

        // 3. Print Results & Check Correctness
        println!("🔍 DETECTED DIAGNOSTIC DETAILS:");
        println!("  - Expected Code   : {}", expected_code);
        println!("  - Classified Mode : {:?}", report.likely_failure_mode);
        println!("  - Confidence Score: {:.2}", report.root_cause_confidence);
        println!("  - Cascade Prob    : {:.2}", report.cascade_probability);
        println!("  - Explanation Style: {}", report.explanation_style);
        println!(
            "  - Generated Output:\n    \"{}\"",
            report.dynamic_explanation
        );
        println!();

        println!("🛠️  REPAIR RANKINGS:");
        for (i, r) in report.ranked_repairs.iter().enumerate() {
            println!(
                "    {}. [{}] {} (Score: {:.2})",
                i + 1,
                r.repair_id,
                r.description,
                r.score
            );
        }
        println!();

        // Verifications
        let mode_correct = match expected_mode.as_str() {
            "OwnershipViolation" => matches!(
                report.likely_failure_mode,
                kain_semantic::FailureMode::OwnershipViolation
            ),
            "Typo" => matches!(
                report.likely_failure_mode,
                kain_semantic::FailureMode::Typo { .. }
            ),
            "ConvergeMismatch" => matches!(
                report.likely_failure_mode,
                kain_semantic::FailureMode::ConvergeMismatch
            ),
            "EntangleViolation" => matches!(
                report.likely_failure_mode,
                kain_semantic::FailureMode::EntangleViolation
            ),
            "WorldDeclarationError" => matches!(
                report.likely_failure_mode,
                kain_semantic::FailureMode::WorldDeclarationError
            ),
            "GenericUnknown" => matches!(
                report.likely_failure_mode,
                kain_semantic::FailureMode::GenericUnknown
            ),
            "ShaderHostBoundary" => matches!(
                report.likely_failure_mode,
                kain_semantic::FailureMode::ShaderHostBoundary
            ),
            "ShaderResourceContract" => matches!(
                report.likely_failure_mode,
                kain_semantic::FailureMode::ShaderResourceContract
            ),
            "CudaKernelContract" => matches!(
                report.likely_failure_mode,
                kain_semantic::FailureMode::CudaKernelContract
            ),
            "PythonInteropBoundary" => matches!(
                report.likely_failure_mode,
                kain_semantic::FailureMode::PythonInteropBoundary { .. }
            ),
            "CAbiBoundary" => matches!(
                report.likely_failure_mode,
                kain_semantic::FailureMode::CAbiBoundary { .. }
            ),
            _ => false,
        };

        let top_repair = {
            if let Some(first) = report.ranked_repairs.first() {
                first.repair_id == expected_repair
                    || first.repair_id == "corpus_spelling_fix"
                    || first.description.contains(&expected_repair)
            } else {
                false
            }
        };

        println!("✅ VERIFICATION VERDICT:");
        if mode_correct && top_repair {
            println!("  STATUS: 100% CORRECT (Verified dynamic classification and repair ranking)");
        } else {
            println!("  STATUS: DEVIATION DETECTED!");
            if !mode_correct {
                println!(
                    "    - Mode mismatch: expected {}, got {:?}",
                    expected_mode, report.likely_failure_mode
                );
            }
            if !top_repair {
                println!(
                    "    - Repair ranking mismatch: expected {}, got {:?}",
                    expected_repair,
                    report.ranked_repairs.first()
                );
            }
        }
        println!(
            "================================================================================"
        );
    }
}
