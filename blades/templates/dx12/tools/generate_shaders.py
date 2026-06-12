#!/usr/bin/env python3
"""Generate minimal SPIR-V hex strings for the DX12 starter template.
   These are passthrough shaders: vertex passes position+color, fragment outputs color.
   Replace with properly compiled SPIR-V from dxc + spirv-cross or glslangValidator."""

import struct

def w(v: int) -> bytes:
    return struct.pack('<I', v & 0xFFFFFFFF)

def encode_str(s: str) -> tuple:
    """Encode null-terminated, word-padded string. Returns (words, byte_count)."""
    b = s.encode('utf-8') + b'\x00'
    while len(b) % 4 != 0:
        b += b'\x00'
    words_needed = len(b) // 4
    word_vals = struct.unpack(f'<{words_needed}I', b)
    return word_vals, words_needed

def str_words(s: str) -> list:
    vals, _ = encode_str(s)
    return list(vals)

def make_vertex_shader() -> str:
    """Minimal vertex shader:
       layout(location=0) in vec3 position;
       layout(location=1) in vec3 color;
       out gl_PerVertex { vec4 gl_Position; };
       layout(location=0) out vec3 out_color;
       void main() { gl_Position = vec4(position, 1.0); out_color = color; }
    """
    parts = []
    def emit(v: int):
        parts.append(w(v))
    def emit_words(vals):
        for v in vals:
            parts.append(w(v))

    # IDs
    EII, TVoid, TFnVoid = 1, 2, 3
    MainFn, Label = 4, 5
    TFloat, TVec3, TPtrIn3 = 6, 7, 8
    TVec4, TPtrOut4, TPtrOut3 = 9, 10, 11
    PosVar, ColVar = 12, 13
    GlPosVar, OutColVar = 14, 15
    Float1 = 16
    TmpPos, TmpCol = 17, 18

    BOUND = 19  # max ID + 1

    # Header: 5 words
    emit(0x07230203)   # Magic
    emit(0x00010000)   # Version 1.0
    emit(0x00080000)   # Generator
    emit(BOUND)
    emit(0)            # Reserved

    # OpCapability Shader (wordcount=2, op=17)
    emit(0x00020011)
    emit(0)

    # OpExtInstImport %1 "GLSL.std.450" (wc=2+4=6, op=11)
    glsl_w = str_words("GLSL.std.450")  # 4 words
    emit((6 << 16) | 11)
    emit(EII)
    emit_words(glsl_w)

    # OpMemoryModel Logical GLSL450 (wc=3, op=14)
    emit(0x0003000e)
    emit(0)   # Logical
    emit(1)   # GLSL450

    # OpEntryPoint Vertex %4 "main" %12 %13 %14 %15 (wc=4+2+4=10, op=15)
    main_w = str_words("main")  # 2 words
    emit((10 << 16) | 15)
    emit(0)          # Vertex
    emit(MainFn)     # Function %4
    emit_words(main_w)
    emit(PosVar)     # interface %12
    emit(ColVar)     # interface %13
    emit(GlPosVar)   # interface %14
    emit(OutColVar)  # interface %15

    # OpSource GLSL 450 (wc=3, op=9)
    emit(0x00030009)
    emit(2)    # GLSL
    emit(450)

    # Names
    # OpName %MainFn "main"
    emit(0x00030005); emit(MainFn); emit_words(main_w)
    # OpName %PosVar "position"
    emit((3+2) << 16 | 5); emit(PosVar); emit_words(str_words("position"))
    # OpName %ColVar "color"
    emit((3+1) << 16 | 5); emit(ColVar); emit_words(str_words("color"))
    # OpName %GlPosVar "gl_Position"
    emit((3+3) << 16 | 5); emit(GlPosVar); emit_words(str_words("gl_Position"))
    # OpName %OutColVar "out_color"
    emit((3+2) << 16 | 5); emit(OutColVar); emit_words(str_words("out_color"))

    # Decorations: OpDecorate (wc=4, op=71=0x47)
    # %12 Location 0
    emit(0x00040047); emit(PosVar); emit(30); emit(0)
    # %13 Location 1
    emit(0x00040047); emit(ColVar); emit(30); emit(1)
    # %14 BuiltIn Position (BuiltIn=11, Position=0)
    emit(0x00040047); emit(GlPosVar); emit(11); emit(0)
    # %15 Location 0
    emit(0x00040047); emit(OutColVar); emit(30); emit(0)

    # Type declarations
    # OpTypeVoid %2 (wc=2, op=19=0x13)
    emit(0x00020013); emit(TVoid)

    # OpTypeFunction %3 %2 (wc=3, op=33=0x21)
    emit(0x00030021); emit(TFnVoid); emit(TVoid)

    # OpTypeFloat %6 32 (wc=3, op=22=0x16)
    emit(0x00030016); emit(TFloat); emit(32)

    # OpTypeVector %7 %6 3 (wc=4, op=23=0x17)
    emit(0x00040017); emit(TVec3); emit(TFloat); emit(3)

    # OpTypePointer %8 Input %7 (wc=4, op=32=0x20)
    emit(0x00040020); emit(TPtrIn3); emit(1); emit(TVec3)

    # OpTypeVector %9 %6 4 (wc=4, op=23)
    emit(0x00040017); emit(TVec4); emit(TFloat); emit(4)

    # OpTypePointer %10 Output %9 (wc=4, op=32)
    emit(0x00040020); emit(TPtrOut4); emit(3); emit(TVec4)

    # OpTypePointer %11 Output %7 (wc=4, op=32)
    emit(0x00040020); emit(TPtrOut3); emit(3); emit(TVec3)

    # Variables: OpVariable (wc=4, op=59=0x3b)
    # %12 = OpVariable %8 Input
    emit(0x0004003b); emit(PosVar); emit(TPtrIn3); emit(1)
    # %13 = OpVariable %8 Input
    emit(0x0004003b); emit(ColVar); emit(TPtrIn3); emit(1)
    # %14 = OpVariable %10 Output
    emit(0x0004003b); emit(GlPosVar); emit(TPtrOut4); emit(3)
    # %15 = OpVariable %11 Output
    emit(0x0004003b); emit(OutColVar); emit(TPtrOut3); emit(3)

    # %16 = OpConstant %6 1.0 (wc=4, op=43=0x2b)
    emit(0x0004002b); emit(Float1); emit(TFloat); emit(0x3f800000)

    # Function: OpFunction %2 %4 None %3 (wc=5, op=54=0x36)
    emit(0x00050036); emit(TVoid); emit(MainFn); emit(0); emit(TFnVoid)

    # OpLabel %5 (wc=2, op=248=0xf8)
    emit(0x000200f8); emit(Label)

    # %17 = OpLoad %7 %12 (wc=4, op=62=0x3e)
    emit(0x0004003e); emit(TmpPos); emit(TVec3); emit(PosVar)
    # %18 = OpLoad %7 %13
    emit(0x0004003e); emit(TmpCol); emit(TVec3); emit(ColVar)

    # Extract position components: OpCompositeExtract (wc=5, op=50=0x32)
    # Actually OpCompositeExtract = 50 = 0x32
    emit(0x00050032); emit(19); emit(TFloat); emit(TmpPos); emit(0)  # pos.x
    emit(0x00050032); emit(20); emit(TFloat); emit(TmpPos); emit(1)  # pos.y
    emit(0x00050032); emit(21); emit(TFloat); emit(TmpPos); emit(2)  # pos.z

    # construct vec4: OpCompositeConstruct (wc=6, op=80=0x50)
    # Result %22, Type %TVec4, constituents: %19 %20 %21 %16
    emit((6 << 16) | 80)
    emit(22)
    emit(TVec4)
    emit(19)
    emit(20)
    emit(21)
    emit(Float1)

    # OpStore %14 %22 (wc=3, op=62=0x3e) — wait, OpStore is 62?
    # OpStore = 62 = 0x3e? Let me check: 
    # OpStore = 62 = 0x3e  — YES
    emit(0x0003003e); emit(GlPosVar); emit(22)

    # OpStore %15 %18
    emit(0x0003003e); emit(OutColVar); emit(TmpCol)

    # OpReturn (wc=1, op=253=0xfd)
    emit(0x000100fd)

    # OpFunctionEnd (wc=1, op=56=0x38)
    emit(0x00010038)

    return b''.join(parts).hex()

def make_fragment_shader() -> str:
    """Minimal fragment shader:
       layout(location=0) in vec3 in_color;
       layout(location=0) out vec4 out_frag;
       void main() { out_frag = vec4(in_color, 1.0); }
    """
    parts = []
    def emit(v: int):
        parts.append(w(v))
    def emit_words(vals):
        for v in vals:
            parts.append(w(v))

    EII, TVoid, TFnVoid = 1, 2, 3
    MainFn, Label = 4, 5
    TFloat, TVec3, TPtrIn3 = 6, 7, 8
    TVec4, TPtrOut4 = 9, 10
    InColVar = 11
    OutFragVar = 12
    Float1 = 13
    TmpInCol = 14

    BOUND = 16

    # Header
    emit(0x07230203)
    emit(0x00010000)
    emit(0x00080000)
    emit(BOUND)
    emit(0)

    # OpCapability Shader
    emit(0x00020011); emit(0)

    # OpExtInstImport %1 "GLSL.std.450"
    glsl_w = str_words("GLSL.std.450")
    emit((6 << 16) | 11); emit(EII); emit_words(glsl_w)

    # OpMemoryModel Logical GLSL450
    emit(0x0003000e); emit(0); emit(1)

    # OpEntryPoint Fragment %4 "main" %11 %12 (wc=4+2+2=8, op=15)
    main_w = str_words("main")
    emit((8 << 16) | 15)
    emit(4)          # Fragment
    emit(MainFn)
    emit_words(main_w)
    emit(InColVar)
    emit(OutFragVar)

    # OpExecutionMode %4 OriginUpperLeft (wc=3, op=16=0x10)
    emit(0x00030010); emit(MainFn); emit(0)  # OriginUpperLeft=0

    # OpSource GLSL 450
    emit(0x00030009); emit(2); emit(450)

    # Names
    emit((3+1) << 16 | 5); emit(MainFn); emit_words(main_w)
    emit((3+2) << 16 | 5); emit(InColVar); emit_words(str_words("in_color"))
    emit((3+2) << 16 | 5); emit(OutFragVar); emit_words(str_words("out_frag"))

    # Decorations
    emit(0x00040047); emit(InColVar); emit(30); emit(0)
    emit(0x00040047); emit(OutFragVar); emit(30); emit(0)

    # Types
    emit(0x00020013); emit(TVoid)
    emit(0x00030021); emit(TFnVoid); emit(TVoid)
    emit(0x00030016); emit(TFloat); emit(32)
    emit(0x00040017); emit(TVec3); emit(TFloat); emit(3)
    emit(0x00040020); emit(TPtrIn3); emit(1); emit(TVec3)
    emit(0x00040017); emit(TVec4); emit(TFloat); emit(4)
    emit(0x00040020); emit(TPtrOut4); emit(3); emit(TVec4)

    # Variables
    emit(0x0004003b); emit(InColVar); emit(TPtrIn3); emit(1)
    emit(0x0004003b); emit(OutFragVar); emit(TPtrOut4); emit(3)

    # Float constant 1.0
    emit(0x0004002b); emit(Float1); emit(TFloat); emit(0x3f800000)

    # Function
    emit(0x00050036); emit(TVoid); emit(MainFn); emit(0); emit(TFnVoid)
    emit(0x000200f8); emit(Label)

    # %14 = OpLoad %7 %11
    emit(0x0004003e); emit(TmpInCol); emit(TVec3); emit(InColVar)

    # Extract components
    emit(0x00050032); emit(15); emit(TFloat); emit(TmpInCol); emit(0)  # r
    emit(0x00050032); emit(16); emit(TFloat); emit(TmpInCol); emit(1)  # g  
    emit(0x00050032); emit(17); emit(TFloat); emit(TmpInCol); emit(2)  # b

    # OpCompositeConstruct %9 %15 %16 %17 %13 → %18
    emit((6 << 16) | 80)
    emit(18)
    emit(TVec4)
    emit(15)
    emit(16)
    emit(17)
    emit(Float1)

    # OpStore %12 %18
    emit(0x0003003e); emit(OutFragVar); emit(18)

    emit(0x000100fd)
    emit(0x00010038)

    return b''.join(parts).hex()


if __name__ == '__main__':
    vert_hex = make_vertex_shader()
    frag_hex = make_fragment_shader()
    print(f"// Vertex shader SPIR-V hex ({len(vert_hex)} chars, {len(vert_hex)//2} bytes)")
    print(f"VERTEX_HEX = \"{vert_hex}\"")
    print()
    print(f"// Fragment shader SPIR-V hex ({len(frag_hex)} chars, {len(frag_hex)//2} bytes)")
    print(f"FRAGMENT_HEX = \"{frag_hex}\"")
