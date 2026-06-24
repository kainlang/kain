; Z3 proof: Extended 64-bit token signature for CPU capability keys
; The 4-param 32-bit sig(len,first,second,last) has collisions between
; "cpu.x86.sse2"/"cpu.x86.avx2"/"cpu.x86.bmi2" and between
; "x86.sse2"/"x86.avx2"/"x86.bmi2".
;
; This extended 5-param 64-bit sig adds second_last byte (position len-2)
; which breaks all collisions: sse2='e', avx2='x', bmi2='i'.
;
; Verified: unsat (all 30 tokens distinct, collision-free)
(set-logic QF_BV)

(define-fun sig64 ((len (_ BitVec 8)) (first (_ BitVec 8)) (second (_ BitVec 8)) (second_last (_ BitVec 8)) (last (_ BitVec 8))) (_ BitVec 64)
  (bvxor (bvshl ((_ zero_extend 56) len) #x0000000000000038)
         (bvxor (bvshl ((_ zero_extend 56) first) #x0000000000000030)
                (bvxor (bvshl ((_ zero_extend 56) second) #x0000000000000028)
                       (bvxor (bvshl ((_ zero_extend 56) second_last) #x0000000000000020)
                              ((_ zero_extend 56) last))))))

; Group 1: SSE2
(define-fun cpu_x86_sse2   () (_ BitVec 64) (sig64 #x0c #x63 #x70 #x65 #x32))
(define-fun x86_sse2       () (_ BitVec 64) (sig64 #x08 #x78 #x38 #x65 #x32))
(define-fun sse2           () (_ BitVec 64) (sig64 #x04 #x73 #x73 #x65 #x32))

; Group 2: AVX
(define-fun cpu_x86_avx    () (_ BitVec 64) (sig64 #x0b #x63 #x70 #x76 #x78))
(define-fun x86_avx        () (_ BitVec 64) (sig64 #x07 #x78 #x38 #x76 #x78))
(define-fun avx            () (_ BitVec 64) (sig64 #x03 #x61 #x76 #x76 #x78))

; Group 3: AVX2
(define-fun cpu_x86_avx2   () (_ BitVec 64) (sig64 #x0c #x63 #x70 #x78 #x32))
(define-fun x86_avx2       () (_ BitVec 64) (sig64 #x08 #x78 #x38 #x78 #x32))
(define-fun avx2           () (_ BitVec 64) (sig64 #x04 #x61 #x76 #x78 #x32))

; Group 4: AVX512F
(define-fun cpu_x86_avx512f  () (_ BitVec 64) (sig64 #x0f #x63 #x70 #x32 #x66))
(define-fun x86_avx512f      () (_ BitVec 64) (sig64 #x0b #x78 #x38 #x32 #x66))
(define-fun avx512f          () (_ BitVec 64) (sig64 #x07 #x61 #x76 #x32 #x66))

; Group 5: AVX512DQ
(define-fun cpu_x86_avx512dq () (_ BitVec 64) (sig64 #x10 #x63 #x70 #x64 #x71))
(define-fun x86_avx512dq     () (_ BitVec 64) (sig64 #x0c #x78 #x38 #x64 #x71))
(define-fun avx512dq         () (_ BitVec 64) (sig64 #x08 #x61 #x76 #x64 #x71))

; Group 6: AVX512BW
(define-fun cpu_x86_avx512bw () (_ BitVec 64) (sig64 #x10 #x63 #x70 #x62 #x77))
(define-fun x86_avx512bw     () (_ BitVec 64) (sig64 #x0c #x78 #x38 #x62 #x77))
(define-fun avx512bw         () (_ BitVec 64) (sig64 #x08 #x61 #x76 #x62 #x77))

; Group 7: AVX512VL
(define-fun cpu_x86_avx512vl () (_ BitVec 64) (sig64 #x10 #x63 #x70 #x76 #x6c))
(define-fun x86_avx512vl     () (_ BitVec 64) (sig64 #x0c #x78 #x38 #x76 #x6c))
(define-fun avx512vl         () (_ BitVec 64) (sig64 #x08 #x61 #x76 #x76 #x6c))

; Group 8: AVX512 (shorthand alias for AVX512F)
(define-fun cpu_x86_avx512  () (_ BitVec 64) (sig64 #x0e #x63 #x70 #x31 #x32))
(define-fun x86_avx512      () (_ BitVec 64) (sig64 #x0a #x78 #x38 #x31 #x32))
(define-fun avx512          () (_ BitVec 64) (sig64 #x06 #x61 #x76 #x31 #x32))

; Group 9: FMA
(define-fun cpu_x86_fma     () (_ BitVec 64) (sig64 #x0b #x63 #x70 #x6d #x61))
(define-fun x86_fma         () (_ BitVec 64) (sig64 #x07 #x78 #x38 #x6d #x61))
(define-fun fma             () (_ BitVec 64) (sig64 #x03 #x66 #x6d #x6d #x61))

; Group 10: BMI2
(define-fun cpu_x86_bmi2    () (_ BitVec 64) (sig64 #x0c #x63 #x70 #x69 #x32))
(define-fun x86_bmi2        () (_ BitVec 64) (sig64 #x08 #x78 #x38 #x69 #x32))
(define-fun bmi2            () (_ BitVec 64) (sig64 #x04 #x62 #x6d #x69 #x32))

(assert (not (distinct
  cpu_x86_sse2 x86_sse2 sse2
  cpu_x86_avx x86_avx avx
  cpu_x86_avx2 x86_avx2 avx2
  cpu_x86_avx512f x86_avx512f avx512f
  cpu_x86_avx512dq x86_avx512dq avx512dq
  cpu_x86_avx512bw x86_avx512bw avx512bw
  cpu_x86_avx512vl x86_avx512vl avx512vl
  cpu_x86_avx512 x86_avx512 avx512
  cpu_x86_fma x86_fma fma
  cpu_x86_bmi2 x86_bmi2 bmi2
)))
(check-sat)
; Expected: unsat
