; Experimental Z3 reference proof for the service alias fast path in
; services.c.
; Claim: the lowercase-folded 64-bit first-32-byte token state used by the
; branchy switch is collision-free across the current alias universe.
(set-logic QF_BV)

; native.app-host
(define-fun alias_token_00 () (_ BitVec 64) (_ bv16815801820913011975 64))
; native.input
(define-fun alias_token_01 () (_ BitVec 64) (_ bv2063835546579788664 64))
; native.viewport
(define-fun alias_token_02 () (_ BitVec 64) (_ bv9314418944132882532 64))
; native.graphics
(define-fun alias_token_03 () (_ BitVec 64) (_ bv5961418047458984955 64))
; native.scene
(define-fun alias_token_04 () (_ BitVec 64) (_ bv11199191916633899485 64))
; native.scene.query
(define-fun alias_token_05 () (_ BitVec 64) (_ bv14760019167419433561 64))
; native.scene.mutation
(define-fun alias_token_06 () (_ BitVec 64) (_ bv17471130565421791714 64))
; native.runtime.inspection
(define-fun alias_token_07 () (_ BitVec 64) (_ bv17598020772866527933 64))
; native.device.reflection
(define-fun alias_token_08 () (_ BitVec 64) (_ bv8808830704429135063 64))
; native.asset.gltf
(define-fun alias_token_09 () (_ BitVec 64) (_ bv6569006360621363000 64))
; native.asset.ingestion
(define-fun alias_token_10 () (_ BitVec 64) (_ bv4624296333135899223 64))
; native.ui.compiled-bundle
(define-fun alias_token_11 () (_ BitVec 64) (_ bv16677789327103776187 64))
; native.compute
(define-fun alias_token_12 () (_ BitVec 64) (_ bv9454214849794350712 64))
; native.shader.spirv
(define-fun alias_token_13 () (_ BitVec 64) (_ bv2714616455172797057 64))
; native.vulkan
(define-fun alias_token_14 () (_ BitVec 64) (_ bv950743410914231920 64))
; native.dx12
(define-fun alias_token_15 () (_ BitVec 64) (_ bv2630516587458683975 64))
; native.d3d12
(define-fun alias_token_16 () (_ BitVec 64) (_ bv6507077043433421494 64))

(assert
  (not
    (distinct
      alias_token_00 alias_token_01 alias_token_02 alias_token_03
      alias_token_04 alias_token_05 alias_token_06 alias_token_07
      alias_token_08 alias_token_09 alias_token_10 alias_token_11
      alias_token_12 alias_token_13 alias_token_14 alias_token_15
      alias_token_16)))
(check-sat)
