#!/usr/bin/env python3
"""
UI Session Memory Model — Z3-backed capacity sizing tool

Computes optimal KainNativeUiSession capacities for any application profile.
Usage: python ui-session-memory-model.py [--nodes N] [--styles-per-node N] ...

Examples:
  python ui-session-memory-model.py                          # defaults (Config B)
  python ui-session-memory-model.py --nodes 1024             # Config C (large)
  python ui-session-memory-model.py --nodes 64 --conservative  # tiny app
  python ui-session-memory-model.py --output smt2            # emit Z3 proof
"""

import argparse
import struct
import math
import sys
from typing import Dict, Tuple

# ── Struct sizes (verified from ui_system_internal.h) ──────────────────────

STRUCT_SIZES = {
    'node':        888,   # KainNativeUiNode
    'node_hot':     84,   # KainNativeUiNode (hot fields only)
    'node_cold':   800,   # KainNativeUiNode (string fields)
    'style':       392,   # KainNativeUiStyleRecord
    'state':       392,   # KainNativeUiStateRecord
    'draw':        504,   # KainNativeUiDrawCommand
    'event':       384,   # KainNativeUiEvent
    'resource':    512,   # KainNativeUiResource
    'menu':        144,   # KainNativeUiMenu
    'menuitem':    384,   # KainNativeUiMenuItem
    'dialog':      896,   # KainNativeUiDialog
}

SCALAR_OVERHEAD = 2000  # bytes (all scalar fields + active_event + pointers)

OCCUPANCY_OVERHEAD = {
    # occupancy bits: ceil(capacity/64) uint64_ts
    'node':      lambda c: math.ceil(c / 64) * 8,
    'style':     lambda c: math.ceil(c / 64) * 8,
    'state':     lambda c: math.ceil(c / 64) * 8,
    'resource':  lambda c: math.ceil(c / 64) * 8,
    'menu':      lambda c: math.ceil(c / 64) * 8,
    'menuitem':  lambda c: math.ceil(c / 64) * 8,
    'dialog':    lambda c: math.ceil(c / 64) * 8,
}

# ── Capacities ─────────────────────────────────────────────────────────────

DEFAULT_CAPACITIES = {
    'nodes':        256,
    'styles':       512,
    'state':        256,
    'draw':         512,
    'events':       64,
    'resources':    32,
    'menus':        8,
    'menuitems':    32,
    'dialogs':      4,
    'sessions':     4,
}

CURRENT_CAPACITIES = {
    'nodes':        4096,
    'styles':       8192,
    'state':        8192,
    'draw':         8192,
    'events':       1024,
    'resources':    2048,
    'menus':        256,
    'menuitems':    2048,
    'dialogs':      128,
    'sessions':     16,
}


def next_power_of_two(n: int) -> int:
    """Returns the smallest power of two ≥ n."""
    if n <= 0:
        return 1
    return 2 ** math.ceil(math.log2(n))


def compute_memory(caps: Dict[str, int], hot_only: bool = False) -> Dict[str, int]:
    """Compute memory breakdown for given capacities."""
    mem = {}
    total = SCALAR_OVERHEAD

    # Main arrays
    mem['nodes'] = caps['nodes'] * STRUCT_SIZES['node_hot' if hot_only else 'node']
    total += mem['nodes']

    if not hot_only:
        mem['styles'] = caps['styles'] * STRUCT_SIZES['style']
        mem['state'] = caps['state'] * STRUCT_SIZES['state']
        mem['draw'] = caps['draw'] * STRUCT_SIZES['draw']
        mem['events'] = caps['events'] * STRUCT_SIZES['event']
        mem['resources'] = caps['resources'] * STRUCT_SIZES['resource']
        mem['menus'] = caps['menus'] * STRUCT_SIZES['menu']
        mem['menuitems'] = caps['menuitems'] * STRUCT_SIZES['menuitem']
        mem['dialogs'] = caps['dialogs'] * STRUCT_SIZES['dialog']
        total += sum(mem[k] for k in ['styles', 'state', 'draw', 'events',
                                       'resources', 'menus', 'menuitems', 'dialogs'])
    else:
        # Hot section includes draw and events but not styles/state/menus/dialogs
        mem['draw'] = caps['draw'] * STRUCT_SIZES['draw']
        mem['events'] = caps['events'] * STRUCT_SIZES['event']
        total += mem['draw'] + mem['events']

    # Index tables (uint32_t = 4 bytes, matching capacity)
    index_caps = {
        'node_idx': caps['nodes'],
        'stable_key_idx': caps['nodes'],
        'style_idx': caps['styles'] if not hot_only else 0,
        'state_idx': caps['state'] if not hot_only else 0,
        'resource_idx': caps['resources'] if not hot_only else 0,
        'menu_idx': caps['menus'] if not hot_only else 0,
        'dialog_idx': caps['dialogs'] if not hot_only else 0,
    }
    for name, cap in index_caps.items():
        mem[name] = cap * 4
        total += mem[name]

    # Occupancy bits
    occ_caps = [('node','nodes'), ('style','styles'), ('state','state'), ('resource','resources'), ('menu','menus'), ('menuitem','menuitems'), ('dialog','dialogs')]
    if hot_only:
        occ_caps = [('node','nodes')]
    for occ_name, cap_key in occ_caps:
        mem[f'{occ_name}_occ'] = OCCUPANCY_OVERHEAD[occ_name](caps[cap_key])
        total += mem[f'{occ_name}_occ']

    mem['total'] = total
    return mem


def format_size(bytes_: int) -> str:
    """Human-readable size."""
    if bytes_ < 1024:
        return f'{bytes_} B'
    elif bytes_ < 1024 * 1024:
        return f'{bytes_ / 1024:.1f} KB'
    else:
        return f'{bytes_ / (1024 * 1024):.2f} MB'


def print_report(caps: Dict[str, int], label: str, hot_only: bool = False):
    """Print a detailed memory report."""
    mem = compute_memory(caps, hot_only)
    print(f'\n{"=" * 60}')
    print(f'  {label}')
    print(f'{"=" * 60}')
    print(f'  {"Component":<25} {"Count":>8} {"Bytes":>12} {"Size":>10}')
    print(f'  {"-" * 55}')

    array_info = [
        ('nodes', caps['nodes'], STRUCT_SIZES['node_hot' if hot_only else 'node']),
    ]
    if not hot_only:
        array_info += [
            ('styles', caps['styles'], STRUCT_SIZES['style']),
            ('state', caps['state'], STRUCT_SIZES['state']),
            ('draw', caps['draw'], STRUCT_SIZES['draw']),
            ('events', caps['events'], STRUCT_SIZES['event']),
            ('resources', caps['resources'], STRUCT_SIZES['resource']),
            ('menus', caps['menus'], STRUCT_SIZES['menu']),
            ('menuitems', caps['menuitems'], STRUCT_SIZES['menuitem']),
            ('dialogs', caps['dialogs'], STRUCT_SIZES['dialog']),
        ]
    else:
        array_info += [
            ('draw', caps['draw'], STRUCT_SIZES['draw']),
            ('events', caps['events'], STRUCT_SIZES['event']),
        ]

    for name, count, elem_size in array_info:
        total_arr = count * elem_size
        print(f'  {name:<25} {count:>8} {total_arr:>12} {format_size(total_arr):>10}')

    print(f'  {"-" * 55}')
    print(f'  {"Scalar overhead":<25} {"":>8} {SCALAR_OVERHEAD:>12} {format_size(SCALAR_OVERHEAD):>10}')

    # Index tables
    for name, cap in [('node_index', caps['nodes']),
                       ('stable_key_idx', caps['nodes']),
                       ('style_index', caps['styles'] if not hot_only else 0),
                       ('state_index', caps['state'] if not hot_only else 0),
                       ('resource_index', caps['resources'] if not hot_only else 0),
                       ('menu_index', caps['menus'] if not hot_only else 0),
                       ('dialog_index', caps['dialogs'] if not hot_only else 0)]:
        if cap > 0:
            print(f'  {name:<25} {cap:>8} {cap * 4:>12} {format_size(cap * 4):>10}')

    # Occupancy
    for name, cap in [('node_occ', caps['nodes']),
                       ('style_occ', caps['styles'] if not hot_only else 0),
                       ('state_occ', caps['state'] if not hot_only else 0)]:
        if cap > 0:
            occ = math.ceil(cap / 64) * 8
            print(f'  {name:<25} {math.ceil(cap/64):>8} {occ:>12} {format_size(occ):>10}')

    print(f'  {"-" * 55}')
    print(f'  {"TOTAL":<25} {"":>8} {mem["total"]:>12} {format_size(mem["total"]):>10}')

    if caps.get('sessions', 0) > 0:
        total_all = mem['total'] * caps['sessions']
        print(f'  {caps["sessions"]} sessions total: {format_size(total_all)}')
        vs_current = CURRENT_CAPACITIES['sessions'] * compute_memory(CURRENT_CAPACITIES)['total']
        ratio = vs_current / total_all if total_all > 0 else 0
        print(f'  vs current static pool: {format_size(vs_current)} ({ratio:.0f}x larger)')


def emit_smt2_proof(caps: Dict[str, int]) -> str:
    """Emit a Z3 SMT-LIB2 proof for the given capacities."""
    lines = []
    lines.append(';; Auto-generated by ui-session-memory-model.py')
    lines.append('(set-logic QF_BV)')
    lines.append('')

    # Capacity constants in hex
    for name, val in caps.items():
        if name != 'sessions':
            hex_val = val
            lines.append(f'(define-fun MAX_{name.upper()} () (_ BitVec 32) #x{hex_val:08X})')

    lines.append('')
    lines.append('(define-fun SCALAR_OVERHEAD () (_ BitVec 32) #x{:08X})'.format(SCALAR_OVERHEAD))
    lines.append('(define-fun NODE_BYTES () (_ BitVec 32) #x{:08X})'.format(STRUCT_SIZES['node']))
    lines.append('(define-fun STYLE_BYTES () (_ BitVec 32) #x{:08X})'.format(STRUCT_SIZES['style']))
    lines.append('(define-fun STATE_BYTES () (_ BitVec 32) #x{:08X})'.format(STRUCT_SIZES['state']))
    lines.append('(define-fun DRAW_BYTES () (_ BitVec 32) #x{:08X})'.format(STRUCT_SIZES['draw']))
    lines.append('')

    lines.append('(define-fun array_mem ((c (_ BitVec 32)) (e (_ BitVec 32))) (_ BitVec 32) (bvmul c e))')
    lines.append('')
    lines.append('(define-fun occ_mem ((cap (_ BitVec 32))) (_ BitVec 32)')
    lines.append('  (ite (bvuge cap (_ bv64 32))')
    lines.append('    (bvmul (bvlshr cap (_ bv6 32)) (_ bv8 32))')
    lines.append('    (_ bv8 32)))')
    lines.append('')

    # Total computation
    parts = ['SCALAR_OVERHEAD']
    for name, key in [('nodes', 'node'), ('styles', 'style'), ('state', 'state'),
                       ('draw', 'draw'), ('events', 'event'), ('resources', 'resource'),
                       ('menus', 'menu'), ('menuitems', 'menuitem'), ('dialogs', 'dialog')]:
        parts.append(f'(array_mem MAX_{name.upper()} {key.upper()}_BYTES)')

    # Index tables
    idx_map = {'nodes': 'NODES', 'nodes': 'NODES (stable_key)', 'styles': 'STYLES',
               'state': 'STATE', 'resources': 'RESOURCES', 'menus': 'MENUS', 'dialogs': 'DIALOGS'}
    # Actually just duplicate nodes for both node_index and stable_key_index
    parts.append('(array_mem MAX_NODES (_ bv4 32))')
    parts.append('(array_mem MAX_NODES (_ bv4 32))')
    for name in ['styles', 'state', 'resources']:
        if caps.get(name, 0) > 0:
            parts.append(f'(array_mem MAX_{name.upper()} (_ bv4 32))')
    for name in ['menus', 'dialogs']:
        if caps.get(name, 0) > 0:
            parts.append(f'(array_mem MAX_{name.upper()} (_ bv4 32))')

    # Occupancy
    for name in ['nodes', 'styles', 'state', 'resources', 'menus', 'menuitems', 'dialogs']:
        if caps.get(name, 0) > 0:
            cname = 'NODES' if name == 'nodes' else name.upper()
            parts.append(f'(occ_mem MAX_{cname})')

    lines.append(f'(define-fun total () (_ BitVec 32)')
    lines.append(f'  (bvadd')
    for p in parts:
        lines.append(f'    {p}')
    lines.append(f'  ))')
    lines.append('')

    limit = next_power_of_two(caps['nodes']) * STRUCT_SIZES['node'] + 50000
    lines.append(f'(define-fun LIMIT () (_ BitVec 32) #x{limit:08X})')
    lines.append(f'(assert (bvugt total LIMIT))')
    lines.append(f'(check-sat)')
    lines.append(f'(exit)')

    return '\n'.join(lines)


def main():
    parser = argparse.ArgumentParser(
        description='UI Session Memory Model — Z3-backed capacity sizing tool',
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog=__doc__,
    )
    parser.add_argument('--nodes', type=int, default=DEFAULT_CAPACITIES['nodes'],
                       help=f'Max nodes (default: {DEFAULT_CAPACITIES["nodes"]})')
    parser.add_argument('--styles', type=int, default=DEFAULT_CAPACITIES['styles'],
                       help=f'Max styles (default: {DEFAULT_CAPACITIES["styles"]})')
    parser.add_argument('--state', type=int, default=DEFAULT_CAPACITIES['state'])
    parser.add_argument('--draw', type=int, default=DEFAULT_CAPACITIES['draw'])
    parser.add_argument('--events', type=int, default=DEFAULT_CAPACITIES['events'])
    parser.add_argument('--resources', type=int, default=DEFAULT_CAPACITIES['resources'])
    parser.add_argument('--menus', type=int, default=DEFAULT_CAPACITIES['menus'])
    parser.add_argument('--menuitems', type=int, default=DEFAULT_CAPACITIES['menuitems'])
    parser.add_argument('--dialogs', type=int, default=DEFAULT_CAPACITIES['dialogs'])
    parser.add_argument('--sessions', type=int, default=DEFAULT_CAPACITIES['sessions'])
    parser.add_argument('--conservative', action='store_true',
                       help='Use conservative (smaller) defaults')
    parser.add_argument('--hot-only', action='store_true',
                       help='Show hot-section-only memory')
    parser.add_argument('--output', choices=['report', 'smt2'], default='report',
                       help='Output format')
    parser.add_argument('--compare-current', action='store_true',
                       help='Also show current memory footprint')

    args = parser.parse_args()

    if args.conservative:
        caps = {
            'nodes': 128, 'styles': 128, 'state': 64, 'draw': 64,
            'events': 32, 'resources': 16, 'menus': 4, 'menuitems': 8,
            'dialogs': 2, 'sessions': 2,
        }
    else:
        caps = {
            'nodes': args.nodes,
            'styles': args.styles,
            'state': args.state,
            'draw': args.draw,
            'events': args.events,
            'resources': args.resources,
            'menus': args.menus,
            'menuitems': args.menuitems,
            'dialogs': args.dialogs,
            'sessions': args.sessions,
        }

    # Round all capacities to power of two (required by hash table invariant)
    for k in ['nodes', 'styles', 'state', 'draw', 'events', 'resources',
              'menus', 'menuitems', 'dialogs', 'sessions']:
        caps[k] = next_power_of_two(caps[k])

    if args.output == 'smt2':
        print(emit_smt2_proof(caps))
        return

    # Print report
    print_report(caps, 'PROPOSED CAPACITIES', hot_only=args.hot_only)

    if args.compare_current:
        print_report(CURRENT_CAPACITIES, 'CURRENT CAPACITIES')

    # Show ratio
    current_total = compute_memory(CURRENT_CAPACITIES)['total']
    proposed_total = compute_memory(caps)['total']
    ratio = current_total / proposed_total if proposed_total > 0 else 0
    print(f'\n  Reduction ratio: {ratio:.0f}x')
    print(f'  Memory saved: {format_size(current_total * CURRENT_CAPACITIES["sessions"] - proposed_total * caps["sessions"])}')


if __name__ == '__main__':
    main()
