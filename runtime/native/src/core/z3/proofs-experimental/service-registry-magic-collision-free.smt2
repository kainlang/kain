; Experimental Z3 reference proof for the lean native runtime service catalog.
; Canonical service keys plus active alias spellings used by the runtime canonicalizer.
; The claim is collision freedom over the current vendor-free token universe for
; the same 64-bit first-32-byte magic-state polynomial used by the native map pass.
(set-logic QF_BV)
; service_token_00: "base.memory"
(define-fun service_token_00 () (_ BitVec 64) #x40ff954815c29c3c)
; service_token_01: "memory.ownership"
(define-fun service_token_01 () (_ BitVec 64) #x996e110347da4ecb)
; service_token_02: "base.diagnostics"
(define-fun service_token_02 () (_ BitVec 64) #xc8cb120ba774d42c)
; service_token_03: "contract"
(define-fun service_token_03 () (_ BitVec 64) #x698874cc51cd9a1a)
; service_token_04: "platform.app-host"
(define-fun service_token_04 () (_ BitVec 64) #x9ddb6ede77c15466)
; service_token_05: "platform.input"
(define-fun service_token_05 () (_ BitVec 64) #xe093a52140179233)
; service_token_06: "gfx.viewport"
(define-fun service_token_06 () (_ BitVec 64) #xbdee771d7a52ed8b)
; service_token_07: "gfx.raw-native"
(define-fun service_token_07 () (_ BitVec 64) #x6afe64f6fc93ec0a)
; service_token_08: "gfx.shader.spirv"
(define-fun service_token_08 () (_ BitVec 64) #x0fb1512b913fcf4e)
; service_token_09: "gfx.backend.vulkan"
(define-fun service_token_09 () (_ BitVec 64) #xe36f7fad9871cc54)
; service_token_10: "gfx.backend.d3d12"
(define-fun service_token_10 () (_ BitVec 64) #xeaa6564c42ba0b6e)
; service_token_11: "scene.runtime"
(define-fun service_token_11 () (_ BitVec 64) #x776dd99f54c1c0ff)
; service_token_12: "scene.query"
(define-fun service_token_12 () (_ BitVec 64) #x06bd309e3c1efed3)
; service_token_13: "scene.mutation"
(define-fun service_token_13 () (_ BitVec 64) #xa93bcf1323d0f538)
; service_token_14: "asset.gltf"
(define-fun service_token_14 () (_ BitVec 64) #xbf8f73e53cfc8f2f)
; service_token_15: "asset.ingestion"
(define-fun service_token_15 () (_ BitVec 64) #xe25621a63df6f8ff)
; service_token_16: "asset.realtime"
(define-fun service_token_16 () (_ BitVec 64) #x727f805c552fa603)
; service_token_17: "ui.bundle"
(define-fun service_token_17 () (_ BitVec 64) #xc406578ad44bcf06)
; service_token_18: "reflection"
(define-fun service_token_18 () (_ BitVec 64) #xc12b1f6e1af9364e)
; service_token_19: "runtime.inspection"
(define-fun service_token_19 () (_ BitVec 64) #xe1abfe67f1428028)
; service_token_20: "device.reflection"
(define-fun service_token_20 () (_ BitVec 64) #x28e6497b81e13e6d)
; service_token_21: "actor.runtime"
(define-fun service_token_21 () (_ BitVec 64) #xe8609817aa70bec8)
; service_token_22: "actor.registry"
(define-fun service_token_22 () (_ BitVec 64) #x3d8b22a2108b2abe)
; service_token_23: "async.runtime"
(define-fun service_token_23 () (_ BitVec 64) #x6371b1a866d592dc)
; service_token_24: "async.timers"
(define-fun service_token_24 () (_ BitVec 64) #x3936858b89656917)
; service_token_25: "io.net"
(define-fun service_token_25 () (_ BitVec 64) #xbadf8e5201528361)
; service_token_26: "io.process"
(define-fun service_token_26 () (_ BitVec 64) #x316e9b66b64ff4a5)
; service_token_27: "gfx.compute"
(define-fun service_token_27 () (_ BitVec 64) #x5c0e2517e24062ee)
; service_token_28: "ui.component"
(define-fun service_token_28 () (_ BitVec 64) #x1f16ff96f558fce0)
; service_token_29: "compatibility"
(define-fun service_token_29 () (_ BitVec 64) #x378e0a4d70394a9f)
; service_token_30: "host.bridge"
(define-fun service_token_30 () (_ BitVec 64) #xeccb11f598278cf6)
; service_token_31: "native.app-host"
(define-fun service_token_31 () (_ BitVec 64) #xe967a2e7a5088d07)
; service_token_32: "native.input"
(define-fun service_token_32 () (_ BitVec 64) #x1c9e242eb4645378)
; service_token_33: "native.viewport"
(define-fun service_token_33 () (_ BitVec 64) #x8140fe9573cec064)
; service_token_34: "native.graphics"
(define-fun service_token_34 () (_ BitVec 64) #x52b4f4dbb3337bfb)
; service_token_35: "native.scene"
(define-fun service_token_35 () (_ BitVec 64) #x9b6bbed0fbf8a1dd)
; service_token_36: "native.scene.query"
(define-fun service_token_36 () (_ BitVec 64) #xcccf3d4aaed22219)
; service_token_37: "native.scene.mutation"
(define-fun service_token_37 () (_ BitVec 64) #xf26120689e22a9e2)
; service_token_38: "native.runtime.inspection"
(define-fun service_token_38 () (_ BitVec 64) #xf42f6791bc7ef2bd)
; service_token_39: "native.device.reflection"
(define-fun service_token_39 () (_ BitVec 64) #x7a425942690ea4d7)
; service_token_40: "native.asset.gltf"
(define-fun service_token_40 () (_ BitVec 64) #x5b2990da90ab1f38)
; service_token_41: "native.asset.ingestion"
(define-fun service_token_41 () (_ BitVec 64) #x403bc9addf0d3a57)
; service_token_42: "native.ui.compiled-bundle"
(define-fun service_token_42 () (_ BitVec 64) #xe764215896fc05bb)
; service_token_43: "native.compute"
(define-fun service_token_43 () (_ BitVec 64) #x83303d876aa8e678)
; service_token_44: "native.shader.spirv"
(define-fun service_token_44 () (_ BitVec 64) #x25be923470113a81)
; service_token_45: "native.vulkan"
(define-fun service_token_45 () (_ BitVec 64) #x0d2f647f2745c670)
; service_token_46: "native.dx12"
(define-fun service_token_46 () (_ BitVec 64) #x249604c6dc88fc47)
; service_token_47: "native.d3d12"
(define-fun service_token_47 () (_ BitVec 64) #x5a3a87a1ea23aab6)
(assert (not (distinct
  service_token_00 service_token_01 service_token_02 service_token_03 service_token_04 service_token_05 service_token_06 service_token_07 service_token_08 service_token_09 service_token_10 service_token_11 service_token_12 service_token_13 service_token_14 service_token_15 service_token_16 service_token_17 service_token_18 service_token_19 service_token_20 service_token_21 service_token_22 service_token_23 service_token_24 service_token_25 service_token_26 service_token_27 service_token_28 service_token_29 service_token_30 service_token_31 service_token_32 service_token_33 service_token_34 service_token_35 service_token_36 service_token_37 service_token_38 service_token_39 service_token_40 service_token_41 service_token_42 service_token_43 service_token_44 service_token_45 service_token_46 service_token_47
)))
(check-sat)
