#ifndef KAIN_SEMANTIC_TRAIN_GPT2_H
#define KAIN_SEMANTIC_TRAIN_GPT2_H

#ifdef __cplusplus
extern "C" {
#endif

// Scalar scoring lane for Kain diagnostics.
int train_gpt2_kain_token_score(const char* text);
int train_gpt2_kain_code_score(const char* text, int code_hint);
int train_gpt2_kain_repair_score(const char* text, int lane_mask_hint, int code_hint, int repair_hint);
int train_gpt2_kain_route(const char* text, int lane_mask_hint);
int train_gpt2_kain_similarity(const char* lhs, const char* rhs);

#ifdef __cplusplus
}
#endif

#endif
