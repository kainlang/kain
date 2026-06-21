; Z3 proof: All 33 canonical service key tokens are collision-free (full 64-bit)
; Includes audio.device and audio.midi which are not in the runtime catalog
; Verified: unsat (all tokens distinct)
(set-logic QF_BV)
(define-fun can_00 () (_ BitVec 64) #x40ff954815c29c3c) ; base.memory
(define-fun can_01 () (_ BitVec 64) #x996e110347da4ecb) ; memory.ownership
(define-fun can_02 () (_ BitVec 64) #xc8cb120ba774d42c) ; base.diagnostics
(define-fun can_03 () (_ BitVec 64) #x698874cc51cd9a1a) ; contract
(define-fun can_04 () (_ BitVec 64) #x9ddb6ede77c15466) ; platform.app-host
(define-fun can_05 () (_ BitVec 64) #xe093a52140179233) ; platform.input
(define-fun can_06 () (_ BitVec 64) #xbdee771d7a52ed8b) ; gfx.viewport
(define-fun can_07 () (_ BitVec 64) #x6afe64f6fc93ec0a) ; gfx.raw-native
(define-fun can_08 () (_ BitVec 64) #x0fb1512b913fcf4e) ; gfx.shader.spirv
(define-fun can_09 () (_ BitVec 64) #xe36f7fad9871cc54) ; gfx.backend.vulkan
(define-fun can_10 () (_ BitVec 64) #xeaa6564c42ba0b6e) ; gfx.backend.d3d12
(define-fun can_11 () (_ BitVec 64) #x776dd99f54c1c0ff) ; scene.runtime
(define-fun can_12 () (_ BitVec 64) #x06bd309e3c1efed3) ; scene.query
(define-fun can_13 () (_ BitVec 64) #xa93bcf1323d0f538) ; scene.mutation
(define-fun can_14 () (_ BitVec 64) #xbf8f73e53cfc8f2f) ; asset.gltf
(define-fun can_15 () (_ BitVec 64) #xe25621a63df6f8ff) ; asset.ingestion
(define-fun can_16 () (_ BitVec 64) #x727f805c552fa603) ; asset.realtime
(define-fun can_17 () (_ BitVec 64) #xc406578ad44bcf06) ; ui.bundle
(define-fun can_18 () (_ BitVec 64) #xc12b1f6e1af9364e) ; reflection
(define-fun can_19 () (_ BitVec 64) #xe1abfe67f1428028) ; runtime.inspection
(define-fun can_20 () (_ BitVec 64) #x28e6497b81e13e6d) ; device.reflection
(define-fun can_21 () (_ BitVec 64) #xe8609817aa70bec8) ; actor.runtime
(define-fun can_22 () (_ BitVec 64) #x3d8b22a2108b2abe) ; actor.registry
(define-fun can_23 () (_ BitVec 64) #x6371b1a866d592dc) ; async.runtime
(define-fun can_24 () (_ BitVec 64) #x3936858b89656917) ; async.timers
(define-fun can_25 () (_ BitVec 64) #xbadf8e5201528361) ; io.net
(define-fun can_26 () (_ BitVec 64) #x316e9b66b64ff4a5) ; io.process
(define-fun can_27 () (_ BitVec 64) #x5c0e2517e24062ee) ; gfx.compute
(define-fun can_28 () (_ BitVec 64) #x1f16ff96f558fce0) ; ui.component
(define-fun can_29 () (_ BitVec 64) #x378e0a4d70394a9f) ; compatibility
(define-fun can_30 () (_ BitVec 64) #xeccb11f598278cf6) ; host.bridge
(define-fun can_31 () (_ BitVec 64) #x332425c3a1eb4e0f) ; audio.device
(define-fun can_32 () (_ BitVec 64) #x5cacc74e40f17a7e) ; audio.midi
(assert (not (distinct can_00 can_01 can_02 can_03 can_04 can_05 can_06 can_07 can_08 can_09 can_10 can_11 can_12 can_13 can_14 can_15 can_16 can_17 can_18 can_19 can_20 can_21 can_22 can_23 can_24 can_25 can_26 can_27 can_28 can_29 can_30 can_31 can_32)))
(check-sat)
; Expected: unsat
