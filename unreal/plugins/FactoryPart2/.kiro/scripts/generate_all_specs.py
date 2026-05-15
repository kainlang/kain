#!/usr/bin/env python3
"""
Batch Plugin Specification Generator

Generates specifications for all 50 plugins in the catalog.
Processes plugins sequentially and generates a summary report.
"""

import os
import sys
import time
from pathlib import Path
from typing import Dict, List
from generate_plugin_spec import PluginSpecGenerator


class BatchSpecGenerator:
    """Generates specifications for all plugins in catalog."""
    
    def __init__(self, base_dir: str, output_base: str = None):
        self.base_dir = Path(base_dir)
        self.output_base = Path(output_base) if output_base else self.base_dir / 'plugins'
        self.generator = PluginSpecGenerator(str(base_dir))
        self.results = []
        
    def generate_all_specifications(self) -> Dict:
        """Generate specifications for all plugins in catalog."""
        print("=" * 80)
        print("BATCH PLUGIN SPECIFICATION GENERATOR")
        print("=" * 80)
        print()
        
        # Parse catalog to get all plugins
        plugins = self.generator.parse_catalog()
        total_plugins = len(plugins)
        
        print(f"Found {total_plugins} plugins in catalog")
        print(f"Output directory: {self.output_base}")
        print()
        
        # Create output base directory
        self.output_base.mkdir(parents=True, exist_ok=True)
        
        # Process each plugin
        start_time = time.time()
        successful = 0
        failed = 0
        
        for i, plugin in enumerate(plugins, 1):
            print(f"\n[{i}/{total_plugins}] Processing {plugin['name']} ({plugin['id']})...")
            print("-" * 80)
            
            plugin_start = time.time()
            
            try:
                success = self.generator.generate_specification(
                    plugin['id'],
                    str(self.output_base)
                )
                
                plugin_time = time.time() - plugin_start
                
                if success:
                    successful += 1
                    status = "SUCCESS"
                else:
                    failed += 1
                    status = "FAILED"
                
                self.results.append({
                    'plugin_id': plugin['id'],
                    'plugin_name': plugin['name'],
                    'domain': plugin['domain'],
                    'status': status,
                    'time': plugin_time
                })
                
                print(f"\n{status} in {plugin_time:.2f}s")
                
            except Exception as e:
                failed += 1
                plugin_time = time.time() - plugin_start
                
                self.results.append({
                    'plugin_id': plugin['id'],
                    'plugin_name': plugin['name'],
                    'domain': plugin['domain'],
                    'status': 'ERROR',
                    'time': plugin_time,
                    'error': str(e)
                })
                
                print(f"\nERROR: {e}")
        
        total_time = time.time() - start_time
        
        # Generate summary
        summary = {
            'total_plugins': total_plugins,
            'successful': successful,
            'failed': failed,
            'total_time': total_time,
            'avg_time': total_time / total_plugins if total_plugins > 0 else 0,
            'results': self.results
        }
        
        return summary
    
    def generate_summary_report(self, summary: Dict) -> str:
        """Generate summary report text."""
        report = []
        report.append("=" * 80)
        report.append("BATCH SPECIFICATION GENERATION SUMMARY")
        report.append("=" * 80)
        report.append("")
        
        # Overall statistics
        report.append("## Overall Statistics")
        report.append("")
        report.append(f"Total Plugins: {summary['total_plugins']}")
        report.append(f"Successful: {summary['successful']}")
        report.append(f"Failed: {summary['failed']}")
        report.append(f"Success Rate: {summary['successful'] / summary['total_plugins'] * 100:.1f}%")
        report.append(f"Total Time: {summary['total_time']:.2f}s ({summary['total_time'] / 60:.1f}m)")
        report.append(f"Average Time per Plugin: {summary['avg_time']:.2f}s")
        report.append("")
        
        # Results by domain
        report.append("## Results by Domain")
        report.append("")
        
        domains = {}
        for result in summary['results']:
            domain = result['domain']
            if domain not in domains:
                domains[domain] = {'total': 0, 'successful': 0, 'failed': 0}
            
            domains[domain]['total'] += 1
            if result['status'] == 'SUCCESS':
                domains[domain]['successful'] += 1
            else:
                domains[domain]['failed'] += 1
        
        for domain, stats in sorted(domains.items()):
            success_rate = stats['successful'] / stats['total'] * 100 if stats['total'] > 0 else 0
            report.append(f"### {domain}")
            report.append(f"- Total: {stats['total']}")
            report.append(f"- Successful: {stats['successful']}")
            report.append(f"- Failed: {stats['failed']}")
            report.append(f"- Success Rate: {success_rate:.1f}%")
            report.append("")
        
        # Detailed results
        report.append("## Detailed Results")
        report.append("")
        report.append("| Plugin ID | Plugin Name | Domain | Status | Time (s) |")
        report.append("|-----------|-------------|--------|--------|----------|")
        
        for result in summary['results']:
            status_icon = "✓" if result['status'] == 'SUCCESS' else "✗"
            report.append(
                f"| {result['plugin_id']} | {result['plugin_name']} | "
                f"{result['domain']} | {status_icon} {result['status']} | "
                f"{result['time']:.2f} |"
            )
        
        report.append("")
        
        # Failed plugins
        failed_results = [r for r in summary['results'] if r['status'] != 'SUCCESS']
        if failed_results:
            report.append("## Failed Plugins")
            report.append("")
            
            for result in failed_results:
                report.append(f"### {result['plugin_name']} ({result['plugin_id']})")
                report.append(f"- Status: {result['status']}")
                if 'error' in result:
                    report.append(f"- Error: {result['error']}")
                report.append("")
        
        return '\n'.join(report)
    
    def save_summary_report(self, summary: Dict):
        """Save summary report to file."""
        report_text = self.generate_summary_report(summary)
        
        report_path = self.base_dir / '.kiro/specs/factory-part-2-plugin-assembly-line/batch_generation_summary.md'
        report_path.parent.mkdir(parents=True, exist_ok=True)
        
        with open(report_path, 'w', encoding='utf-8') as f:
            f.write(report_text)
        
        print(f"\nSummary report saved to: {report_path}")
        
        # Also print to console
        print()
        print(report_text)


def main():
    """Main entry point."""
    # Determine base directory (FactoryPart2)
    script_dir = Path(__file__).parent
    base_dir = script_dir.parent
    
    # Parse command line arguments
    output_base = sys.argv[1] if len(sys.argv) > 1 else None
    
    if output_base:
        print(f"Using custom output directory: {output_base}")
    else:
        print(f"Using default output directory: {base_dir}/plugins")
    
    # Create batch generator
    batch_gen = BatchSpecGenerator(str(base_dir), output_base)
    
    # Generate all specifications
    summary = batch_gen.generate_all_specifications()
    
    # Save summary report
    batch_gen.save_summary_report(summary)
    
    # Exit with appropriate code
    sys.exit(0 if summary['failed'] == 0 else 1)


if __name__ == '__main__':
    main()
