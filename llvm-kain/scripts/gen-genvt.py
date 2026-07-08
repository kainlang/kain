#!/usr/bin/env python3
"""Generate GenVT.inc from ValueTypes.td for llvm-kain
Each range marker is individually #ifdef-guarded and placed AFTER the entry it references."""
import re, sys

def parse_vt_td(path):
    with open(path) as f:
        content = f.read()
    # Pre-process: join multi-line defs (e.g. aarch64svcount spans 2 lines)
    content = re.sub(r'\n\s*:\s*', ' : ', content)
    m = re.search(r'defset list<ValueType> ValueTypes\s*=\s*{(.*?)^}', content, re.DOTALL | re.MULTILINE)
    if not m: return [], []
    body = m.group(1)
    
    pseudo_start = body.find('let isNormalValueType = false in {')
    if pseudo_start >= 0:
        brace_start = body.index('{', pseudo_start)
        depth = 1; i = brace_start + 1
        while i < len(body) and depth > 0:
            if body[i] == '{': depth += 1
            elif body[i] == '}': depth -= 1
            i += 1
        pseudo_body = body[brace_start+1:i-1]
        normal_body = body[:pseudo_start] + body[i:]
    else:
        pseudo_body = ""; normal_body = body
    
    scalar_map = {}
    
    def parse_defs(text, is_normal):
        entries = []
        for line in text.split('\n'):
            line = line.strip()
            if not line or line.startswith('//') or line.startswith('/*') or line.startswith('*'):
                continue
            m = re.match(r'def\s+(\w+)\s*:\s*(\w+)\s*(?:<([^>]*)>)?\s*;', line)
            if not m: continue
            td_name, cls, args = m.group(1), m.group(2), m.group(3) or ''
            name = td_name; sz = 0
            ov=False; ii=False; ff=False; vec=False; sc=False; tup=False
            nf=0; nelem=1; eltty='INVALID_SIMPLE_VALUE_TYPE'
            
            if cls == 'VTAny': ov = True
            elif cls == 'VTInt':
                ii = True; sm = re.search(r'\d+', args)
                if sm: sz = int(sm.group(0))
            elif cls == 'VTFP':
                ff = True; sm = re.search(r'\d+', args)
                if sm: sz = int(sm.group(0))
            elif cls == 'VTVec':
                vec = True; vm = re.search(r'(\d+)\s*,\s*(\w+)', args)
                if vm: nelem=int(vm.group(1)); eltty=vm.group(2)
            elif cls == 'VTScalableVec':
                vec=sc=True; vm = re.search(r'(\d+)\s*,\s*(\w+)', args)
                if vm: nelem=int(vm.group(1)); eltty=vm.group(2)
            elif cls == 'VTVecTup':
                vec=sc=tup=True; vm = re.search(r'(\d+)\s*,\s*(\d+)\s*,\s*(\w+)', args)
                if vm: sz=int(vm.group(1)); nf=int(vm.group(2)); eltty=vm.group(3)
            elif cls == 'VTCheriCapability':
                sm = re.search(r'\d+', args)
                if sm: sz = int(sm.group(0))
            elif cls == 'ValueType':
                sm = re.search(r'(\d+)', args)
                if sm: sz = int(sm.group(0))
                vtm = re.search(r'ValueType<\d+\s*,\s*"(\w+)"', line)
                if vtm: name = vtm.group(1)
            elif cls == 'PtrValueType': pass
            
            vtm2 = re.search(r'"(\w+)"', line)
            if vtm2 and cls == 'ValueType': name = vtm2.group(1)
            
            entries.append(dict(name=name, size=sz, overloaded=ov,
                is_int=ii, is_fp=ff, is_vec=vec, is_scalable=sc, is_tuple=tup,
                nf=nf, nelem=nelem, eltty=eltty, normal=is_normal))
        return entries
    
    norm = parse_defs(normal_body, True)
    pseudo = parse_defs(pseudo_body, False)
    
    for e in norm:
        if not e['is_vec'] and not e['is_tuple'] and not e['overloaded']:
            scalar_map[e['name']] = (e['size'], e['is_int'], e['is_fp'])
    for e in norm + pseudo:
        if e['is_vec'] and e['eltty'] != 'INVALID_SIMPLE_VALUE_TYPE':
            elt = e['eltty']
            if elt in scalar_map:
                esz, eii, eff = scalar_map[elt]
                if not e['is_tuple']: e['size'] = esz * e['nelem']
                e['is_int'] = eii; e['is_fp'] = eff
    return norm, pseudo

def gen_genvt(normal, pseudo):
    lines = []
    
    def fmt(e):
        iv = "3" if (e["is_int"] and not e["is_vec"]) else "0"
        fv = "3" if (e["is_fp"] and not e["is_vec"]) else "0"
        return (f'GET_VT_ATTR({e["name"]}, {e["size"]}, '
                f'{"true" if e["overloaded"] else "false"}, '
                f'{iv}, {fv}, '
                f'{"true" if e["is_vec"] else "false"}, '
                f'{"true" if e["is_scalable"] else "false"}, '
                f'{"true" if e["is_tuple"] else "false"}, '
                f'{e["nf"]}, {e["nelem"]}, {e["eltty"]})')
    
    def rng(marker, name):
        return (f'#ifdef GET_VT_RANGES\n'
                f'  {marker} = {name},\n'
                f'#endif')
    
    def fl(lst):
        if lst: return (lst[0]['name'], lst[-1]['name'])
        return (None, None)
    
    # Precompute ranges
    IS_F, IS_L = fl([e for e in normal if e['is_int'] and not e['is_vec'] and not e['is_tuple']])
    FS_F, FS_L = fl([e for e in normal if e['is_fp'] and not e['is_vec'] and not e['is_tuple']])
    AV_F, AV_L = fl([e for e in normal if e['is_vec'] and not e['is_tuple']])
    FV_F, FV_L = fl([e for e in normal if e['is_vec'] and not e['is_scalable'] and not e['is_tuple']])
    SV_F, SV_L = fl([e for e in normal if e['is_vec'] and e['is_scalable'] and not e['is_tuple']])
    IF_F, IF_L = fl([e for e in normal if e['is_vec'] and not e['is_scalable'] and e['is_int'] and not e['is_tuple']])
    FF_F, FF_L = fl([e for e in normal if e['is_vec'] and not e['is_scalable'] and e['is_fp'] and not e['is_tuple']])
    ISC_F, ISC_L = fl([e for e in normal if e['is_vec'] and e['is_scalable'] and e['is_int'] and not e['is_tuple']])
    FSC_F, FSC_L = fl([e for e in normal if e['is_vec'] and e['is_scalable'] and e['is_fp'] and not e['is_tuple']])
    TUP_F, TUP_L = fl([e for e in normal if e['is_tuple']])
    CH_F, CH_L = fl([e for e in normal if e['name'] in ('c64', 'c128')])
    
    FV = normal[0]['name'] if normal else None
    LV = normal[-1]['name'] if normal else None
    
    for e in normal:
        n = e['name']
    for e in normal:
        n = e['name']
        
        # Wrap each GET_VT_ATTR in #ifdef guard
        lines.append('#ifdef GET_VT_ATTR')
        lines.append(fmt(e))
        lines.append('#endif')
        
        # Range markers after this entry (already individually #ifdef-guarded)
        if n == FV:
            lines.append(rng('FIRST_VALUETYPE', n))
        if n == LV:
            lines.append(rng('LAST_VALUETYPE', n))
        
        if n == IS_F: lines.append(rng('FIRST_INTEGER_VALUETYPE', n))
        if n == IS_L: lines.append(rng('LAST_INTEGER_VALUETYPE', n))
        if n == FS_F: lines.append(rng('FIRST_FP_VALUETYPE', n))
        if n == FS_L: lines.append(rng('LAST_FP_VALUETYPE', n))
        if n == AV_F: lines.append(rng('FIRST_VECTOR_VALUETYPE', n))
        if n == AV_L: lines.append(rng('LAST_VECTOR_VALUETYPE', n))
        if n == FV_F: lines.append(rng('FIRST_FIXEDLEN_VECTOR_VALUETYPE', n))
        if n == FV_L: lines.append(rng('LAST_FIXEDLEN_VECTOR_VALUETYPE', n))
        if n == SV_F: lines.append(rng('FIRST_SCALABLE_VECTOR_VALUETYPE', n))
        if n == SV_L: lines.append(rng('LAST_SCALABLE_VECTOR_VALUETYPE', n))
        if n == IF_F: lines.append(rng('FIRST_INTEGER_FIXEDLEN_VECTOR_VALUETYPE', n))
        if n == IF_L: lines.append(rng('LAST_INTEGER_FIXEDLEN_VECTOR_VALUETYPE', n))
        if n == FF_F: lines.append(rng('FIRST_FP_FIXEDLEN_VECTOR_VALUETYPE', n))
        if n == FF_L: lines.append(rng('LAST_FP_FIXEDLEN_VECTOR_VALUETYPE', n))
        if n == ISC_F: lines.append(rng('FIRST_INTEGER_SCALABLE_VECTOR_VALUETYPE', n))
        if n == ISC_L: lines.append(rng('LAST_INTEGER_SCALABLE_VECTOR_VALUETYPE', n))
        if n == FSC_F: lines.append(rng('FIRST_FP_SCALABLE_VECTOR_VALUETYPE', n))
        if n == FSC_L: lines.append(rng('LAST_FP_SCALABLE_VECTOR_VALUETYPE', n))
        if n == TUP_F: lines.append(rng('FIRST_RISCV_VECTOR_TUPLE_VALUETYPE', n))
        if n == TUP_L: lines.append(rng('LAST_RISCV_VECTOR_TUPLE_VALUETYPE', n))
        if n == CH_F: lines.append(rng('FIRST_CHERI_CAPABILITY_VALUETYPE', n))
        if n == CH_L: lines.append(rng('LAST_CHERI_CAPABILITY_VALUETYPE', n))
    
    for e in pseudo:
        lines.append('#ifdef GET_VT_ATTR')
        lines.append(fmt(e))
        lines.append('#endif')
    
    lines.append('')
    lines.append('#ifdef GET_VT_VECATTR')
    for e in normal + pseudo:
        if e['is_vec']:
            lines.append(f'GET_VT_VECATTR({e["name"]}, '
                        f'{"true" if e["is_scalable"] else "false"}, '
                        f'{"true" if e["is_tuple"] else "false"}, '
                        f'{e["nelem"]}, {e["eltty"]})')
    lines.append('#endif // GET_VT_VECATTR')
    return '\n'.join(lines)

if __name__ == '__main__':
    td = sys.argv[1] if len(sys.argv) > 1 else 'include/target/shared/codegen/ValueTypes.td'
    normal, pseudo = parse_vt_td(td)
    print(f'Normal: {len(normal)}, Pseudo: {len(pseudo)}', file=sys.stderr)
    print(gen_genvt(normal, pseudo))
