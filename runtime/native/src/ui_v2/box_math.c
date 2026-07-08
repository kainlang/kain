// ============================================================================
//  box_math.c — Two-pass flexbox constraint solver (Yoga-inspired).
//  Pure math, zero platform headers. 49 formulas, Z3 UNSAT box_math_proofs.yaml
// ============================================================================
#include "internal.h"
#include <float.h>
#include <math.h>

#define UNDEFINED  (FLT_MAX)
#define IS_UNDEF(v) ((v) >= FLT_MAX * 0.5f)
#define MODE_STRETCH_FIT  0
#define MODE_MAX_CONTENT  1
#define MODE_FIT_CONTENT  2
// MAX_CHILDREN = KAINTANA_MAX_CHILDREN  (defined in internal.h)

// ============================================================================
//  1.1 FLEX-BASIS RESOLUTION
// ============================================================================
static float flex_basis_value(const KaintanaLayout* l, int axis) {
    // flex_basis==0 is the default (from zero-init) — treat as "auto"
    // to use intrinsic desired_width/desired_height instead of 0.
    // Yoga: computedFlexBasis = maxOrDefined(resolvedFlexBasis, paddingAndBorder).
    // The padding floor prevents 0px items with non-zero padding.
    float pad = (axis == 0) ? (l->pad_left + l->pad_right) : (l->pad_top + l->pad_bottom);
    if (!IS_UNDEF(l->flex_basis) && l->flex_basis > 0.0f) return fmaxf(l->flex_basis, pad);
    if (axis==0 && !IS_UNDEF(l->desired_width))  return fmaxf(l->desired_width, pad);
    if (axis==1 && !IS_UNDEF(l->desired_height)) return fmaxf(l->desired_height, pad);
    return pad;
}
// ============================================================================
//  1.1a AUTO-MINIMUM (CSS 4.5) — Prevents flex items from collapsing to zero
// ============================================================================
// Yoga: computeAutoMinMainSize(). Floor = min(content-size, specified-size)
// capped by max-size. Returns UNDEFINED when no auto-min applies (explicit
// min set, no content, or overflow != visible).
// BoundAxis with auto-min floor applied in distribution passes.
static float compute_auto_min_main(float flex_basis, float desired_main, float min_main) {
    // If explicit min set, auto-min doesn't apply (CSS 4.5 opt-out)
    if (!IS_UNDEF(min_main) && min_main > 0.0f) return UNDEFINED;
    float content = desired_main;
    if (content <= 0.0f) return UNDEFINED;
    float specified = flex_basis > 0.0f ? flex_basis : content;
    float am = fminf(content, specified);
    return am > 0.0f ? am : 0.0f;
}

// ============================================================================
//  1.2 GROW/SHRINK DISTRIBUTION
// ============================================================================
static inline float clampf(float v, float lo, float hi) { return fminf(fmaxf(v,lo),hi); }
static float bound_axis(float proposal, float pad, float min, float max) {
    return fmaxf(clampf(proposal, min, max), pad);
}
static float distribute_grow(float basis, float grow, float sum_grow, float remaining) {
    if (sum_grow<=0.0f||remaining<=0.0f) return basis;
    return basis + (grow/sum_grow)*remaining;
}
static float distribute_shrink(float basis, float shrink, float scaled_sum, float remaining) {
    if (scaled_sum<=0.0f||remaining>=0.0f) return basis;
    float ratio = (shrink*basis)/scaled_sum;
    return basis + remaining*ratio;
}

// ============================================================================
//  1.2a BOUND AXIS WITH AUTO-MIN — boundAxis + CSS 4.5 auto-min floor
// ============================================================================
// Yoga: boundAxisWithAutoMin(). Applies normal boundAxis, then additionally
// floors by the cached auto-minimum size from compute_auto_min_main.
static float bound_axis_with_auto_min(float proposal, float pad, float min, float max, float auto_min) {
    float bounded = fmaxf(clampf(proposal, min, max), pad);
    if (!IS_UNDEF(auto_min) && bounded < auto_min) bounded = auto_min;
    return bounded;
}

// ============================================================================
//  1.2b CONSTRAIN MAX SIZE FOR MODE — Yoga constrainMaxSizeForMode()
// ============================================================================
// Caps the available size by max bound, potentially transitioning the sizing
// mode. In our simplified model, we just apply min/max constraints without
// mode transition (sizing modes are determined in prepass).
static float constrain_max_size(float size, float min_m, float max_m) {
    if (!IS_UNDEF(max_m) && size > max_m) size = max_m;
    if (!IS_UNDEF(min_m) && size < min_m) size = min_m;
    return size;
}

// ============================================================================
//  1.3 JUSTIFY-CONTENT
// ============================================================================
static float justify_gap(int mode, float remaining, int child_count) {
    if (child_count<=1) return 0.0f;
    switch (mode) {
        case 0: return 0.0f;
        case 1: return remaining/2.0f;
        case 2: return remaining;
        case 3: return remaining/(child_count-1);
        case 4: return remaining/child_count;
        case 5: return remaining/(child_count+1);
        default: return 0.0f;
    }
}
static void auto_margin(float ms, float me, float remaining, float* out_s, float* out_e) {
    if (ms<0&&me<0) { *out_s=remaining/2.0f; *out_e=remaining/2.0f; }
    else if (ms<0) { *out_s=remaining; *out_e=me; }
    else if (me<0) { *out_s=ms; *out_e=remaining; }
    else { *out_s=ms; *out_e=me; }
}

// ============================================================================
//  1.4 CROSS-AXIS ALIGNMENT
// ============================================================================
static float align_cross_axis(int mode, float line_start, float line_cross,
    float child_cross, float margin_start, float margin_end,
    float line_baseline, float child_baseline) {
    switch (mode) {
        case 0: return line_start+margin_start;             // STRETCH (child size handled at call site)
        case 1: return line_start+margin_start;             // FLEX_START
        case 2: return line_start+(line_cross-child_cross-margin_start-margin_end)/2.0f;
        case 3: return line_start+line_cross-child_cross-margin_end; // FLEX_END
        case 4: return line_baseline-child_baseline;        // BASELINE
        default: return line_start+margin_start;
    }
}

// ============================================================================
//  1.6 BOX MODEL
// ============================================================================
static float inner_w(float outer, const float pad[4], const float border[4])
    { return outer-pad[0]-pad[2]-border[0]-border[2]; }
static float inner_h(float outer, const float pad[4], const float border[4])
    { return outer-pad[1]-pad[3]-border[1]-border[3]; }

// ============================================================================
//  1.7 SIZING MODE
// ============================================================================
static float desired_size(int mode, float available, float intrinsic) {
    switch (mode) {
        case MODE_STRETCH_FIT: return available;
        case MODE_MAX_CONTENT: return intrinsic;
        case MODE_FIT_CONTENT: return fminf(intrinsic, available);
        default: return intrinsic;
    }
}

// ============================================================================
//  1.8 LAYOUT CACHE
// ============================================================================
static int cache_hit(const KaintanaLayoutCache* c, float aw, float ah, uint32_t gen) {
    return c->valid && c->available_width==aw && c->available_height==ah && c->generation==gen;
}
static void cache_update(KaintanaLayoutCache* c, float aw, float ah, float mw, float mh, uint32_t gen) {
    c->valid=true; c->generation=gen;
    c->available_width=aw; c->available_height=ah;
    c->measured_width=mw; c->measured_height=mh;
}

// ============================================================================
//  1.9 INTRINSIC SIZING
// ============================================================================
static void measure_leaf(KaintanaLayout* l, float aw, float ah) {
    if (IS_UNDEF(l->desired_width))  l->desired_width=0.0f;
    if (IS_UNDEF(l->desired_height)) l->desired_height=0.0f;
    (void)aw;(void)ah;
}
static void measure_container(KaintanaLayout* l, float max_cw, float sum_ch, const float pad[4]) {
    float iw=max_cw+pad[0]+pad[2], ih=sum_ch+pad[1]+pad[3];
    if (IS_UNDEF(l->desired_width)) l->desired_width=iw;
    if (IS_UNDEF(l->desired_height)) l->desired_height=ih;
}

// ============================================================================
//  PREPASS (bottom-up desired sizes)
// ============================================================================
void kaintana__layout_pass1(kt_Session* s) {
    struct kt_Session_t* sess = kaintana__session(s);

    int32_t* order = (int32_t*)kaintana__arena_push(s, sess->node_count*sizeof(int32_t));
    int16_t* depths = (int16_t*)kaintana__arena_push(s, sess->node_count*sizeof(int16_t));
    if (!order||!depths) return;

    for (int32_t i=1;i<sess->node_count;i++) {
        KaintanaNode* n = &sess->nodes[i];
        depths[i] = (n->parent_index>0&&n->parent_index<sess->node_count)
            ? depths[n->parent_index]+1 : 0;
        order[i]=i;
    }

    // Insertion sort by depth descending (children before parents)
    for (int32_t i=1;i<sess->node_count;i++) {
        int32_t key=order[i]; int16_t kd=depths[key]; int32_t j=i-1;
        while (j>=0&&depths[order[j]]<kd) { order[j+1]=order[j]; j--; }
        order[j+1]=key;
    }

    for (int32_t i=0;i<sess->node_count;i++) {
        int32_t idx=order[i];
        KaintanaNode* n = &sess->nodes[idx];
        if (!(n->flags&KT_NODE_VISIBLE)) continue;

    // Layout arena index is now allocated in node_alloc() (tree.c).
        // Defensive guard for edge cases.
        if (n->layout_arena_index < 0) continue;
        KaintanaLayout* l = &sess->layouts[n->layout_arena_index];

        // NOTE: layout cache not used in prepass — cache is only valid when
        // measuring with specific available dimensions from the arrange pass.
        // In prepass we always compute desired sizes fresh from content.

        if (n->first_child<0) {
            measure_leaf(l, l->min_width, l->min_height);
        } else {
            float max_cw=0.0f, sum_ch=0.0f;
            int child=n->first_child;
            while (child>=0&&child<sess->node_count) {
                KaintanaNode* cn=&sess->nodes[child];
                if (cn->flags&KT_NODE_VISIBLE && cn->layout_arena_index>=0) {
                    KaintanaLayout* cl=&sess->layouts[cn->layout_arena_index];
                    max_cw=fmaxf(max_cw,cl->desired_width);
                    sum_ch+=cl->desired_height;
                }
                child=cn->next_sibling;
            }
            float pad[4]={l->pad_left,l->pad_top,l->pad_right,l->pad_bottom};
            measure_container(l, max_cw, sum_ch, pad);
        }

        l->desired_width=desired_size(l->width_mode,l->min_width,l->desired_width);
        l->desired_height=desired_size(l->height_mode,l->min_height,l->desired_height);
    }
}

// ============================================================================
//  ARRANGE (top-down position + size resolution)
// ============================================================================
void kaintana__layout_pass2(kt_Session* s) {
    struct kt_Session_t* sess = kaintana__session(s);

    // ── BUG-009: Set root node resolved size to session dimensions ────
    // Root node (index 0) never gets resolved_width/resolved_height set
    // because it's skipped in the arrange loop (n->first_child<0 guard on
    // non-leaf root). Children compute their layout against the parent's
    // resolved size, so root must be initialized to the full window.
    KaintanaNode* root_node = &sess->nodes[0];
    if (root_node->layout_arena_index >= 0) {
        KaintanaLayout* root_layout = &sess->layouts[root_node->layout_arena_index];
        root_layout->resolved_x = 0.0f;
        root_layout->resolved_y = 0.0f;
        root_layout->resolved_width = (float)sess->window_width;
        root_layout->resolved_height = (float)sess->window_height;
    }

    int16_t* depths = (int16_t*)kaintana__arena_push(s, sess->node_count*sizeof(int16_t));
    int32_t* order = (int32_t*)kaintana__arena_push(s, sess->node_count*sizeof(int32_t));
    if (!depths||!order) return;

    for (int32_t i=1;i<sess->node_count;i++) {
        KaintanaNode* n=&sess->nodes[i];
        depths[i]=(n->parent_index>0&&n->parent_index<sess->node_count)
            ? depths[n->parent_index]+1:0;
        order[i]=i;
    }
    for (int32_t i=1;i<sess->node_count;i++) {
        int32_t key=order[i];int16_t kd=depths[key];int32_t j=i-1;
        while (j>=0&&depths[order[j]]>kd){order[j+1]=order[j];j--;}
        order[j+1]=key;
    }

    for (int32_t i=0;i<sess->node_count;i++) {
        int32_t idx=order[i];
        KaintanaNode* n=&sess->nodes[idx];
        if (!(n->flags&KT_NODE_VISIBLE)||n->first_child<0) continue;
        if (n->layout_arena_index<0) continue;
        KaintanaLayout* l=&sess->layouts[n->layout_arena_index];

        float pad[4]={l->pad_left,l->pad_top,l->pad_right,l->pad_bottom};
        float avail_w=l->resolved_width-pad[0]-pad[2];
        float avail_h=l->resolved_height-pad[1]-pad[3];
        int axis         = (int)l->direction;       // KaintanaLayoutDir: 0=row,1=col,2=row-rev,3=col-rev
        int justify_mode = (int)l->justify_content; // KaintanaJustify: 0=flex-start,...,5=space-evenly
        int align_mode   = (int)l->align_items;     // KaintanaAlign: 0=stretch,...,5=auto

        int32_t cids[KAINTANA_MAX_CHILDREN];
        int child_count=0;
        float sum_bases=0.0f,sum_margins=0.0f,sum_grow=0.0f,sum_scaled_shrink=0.0f;
        int child=n->first_child;
        while (child>=0&&child<sess->node_count&&child_count<KAINTANA_MAX_CHILDREN) {
            KaintanaNode* cn=&sess->nodes[child];
            if ((cn->flags&KT_NODE_VISIBLE)&&cn->layout_arena_index>=0) {
                cids[child_count++]=child;
                KaintanaLayout* cl=&sess->layouts[cn->layout_arena_index];
                float basis=flex_basis_value(cl,axis);
                float mm=(axis==0)?cl->margin_left+cl->margin_right:cl->margin_top+cl->margin_bottom;
                sum_bases+=basis;sum_margins+=mm;sum_grow+=cl->flex_grow;
                if (cl->flex_shrink>0.0f&&basis>0.0f) sum_scaled_shrink+=cl->flex_shrink*basis;
            }
            child=cn->next_sibling;
        }

        float container_main=(axis==0)?avail_w:avail_h;
        float remaining_orig=container_main-sum_bases-sum_margins;

        // Floor flex factor sums per Yoga: if >0 and <1, floor to 1 (CSS spec)
        if (sum_grow>0.0f && sum_grow<1.0f) sum_grow=1.0f;
        if (sum_scaled_shrink>0.0f && sum_scaled_shrink<1.0f) sum_scaled_shrink=1.0f;

        // Per-child data arrays
        float bases[KAINTANA_MAX_CHILDREN];
        float mm_a[KAINTANA_MAX_CHILDREN];
        float min_m_a[KAINTANA_MAX_CHILDREN], max_m_a[KAINTANA_MAX_CHILDREN];
        float auto_min_sz[KAINTANA_MAX_CHILDREN];

        for (int ci=0; ci<child_count; ci++) {
            int cid=cids[ci];
            KaintanaLayout* cl=&sess->layouts[sess->nodes[cid].layout_arena_index];
            bases[ci]=flex_basis_value(cl,axis);
            mm_a[ci]=(axis==0)?cl->margin_left+cl->margin_right:cl->margin_top+cl->margin_bottom;
            min_m_a[ci]=constrain_max_size((axis==0)?cl->min_width:cl->min_height,0.0f,FLT_MAX);
            max_m_a[ci]=(axis==0)?cl->max_width:cl->max_height;
            if (IS_UNDEF(max_m_a[ci])) max_m_a[ci]=FLT_MAX;
            float desired_main=(axis==0)?cl->desired_width:cl->desired_height;
            auto_min_sz[ci]=compute_auto_min_main(bases[ci],desired_main,min_m_a[ci]);
        }

        // ── FIRST PASS: Tentative distribution + freeze at bounds ────
        // (Mirrors Yoga's distributeFreeSpaceFirstPass)
        bool frozen[KAINTANA_MAX_CHILDREN];
        float tentatives[KAINTANA_MAX_CHILDREN];
        float sizes[KAINTANA_MAX_CHILDREN];
        float remaining=remaining_orig;
        float sum_grow_pass=sum_grow;
        float sum_shrink_pass=sum_scaled_shrink;

        for (int ci=0; ci<child_count; ci++) {
            int cid=cids[ci];
            KaintanaLayout* cl=&sess->layouts[sess->nodes[cid].layout_arena_index];
            float basis=bases[ci], mm=mm_a[ci];
            float min_m=min_m_a[ci], max_m=max_m_a[ci], auto_min=auto_min_sz[ci];

            float tentative;
            if (remaining>0.0f && sum_grow_pass>0.0f) {
                // Grow: use regular boundAxis (auto-min only applied in second pass)
                tentative=distribute_grow(basis,cl->flex_grow,sum_grow_pass,remaining);
                float clamped=bound_axis(tentative,mm,min_m,max_m);
                // Also check auto-min for grow items that might need a floor
                if (!IS_UNDEF(auto_min) && clamped<auto_min) clamped=auto_min;
                if (clamped!=tentative) {
                    frozen[ci]=true;
                    remaining-=(clamped-basis);
                    sum_grow_pass-=cl->flex_grow;
                    if (sum_grow_pass<0.0f) sum_grow_pass=0.0f;
                    tentative=clamped;
                } else { frozen[ci]=false; }
            } else if (remaining<0.0f && sum_shrink_pass>0.0f) {
                // Shrink: use boundAxisWithAutoMin (Yoga CSS 4.5)
                tentative=distribute_shrink(basis,cl->flex_shrink,sum_shrink_pass,remaining);
                float clamped=bound_axis_with_auto_min(tentative,mm,min_m,max_m,auto_min);
                if (clamped!=tentative) {
                    frozen[ci]=true;
                    remaining-=(clamped-basis);
                    sum_shrink_pass-=cl->flex_shrink*basis;
                    if (sum_shrink_pass<0.0f) sum_shrink_pass=0.0f;
                    tentative=clamped;
                } else { frozen[ci]=false; }
            } else {
                tentative=basis;
                frozen[ci]=true;  // No flexibility — effectively frozen
            }
            tentatives[ci]=tentative;
        }

        // ── SECOND PASS: Final redistribution among unfrozen items ──
        // (Mirrors Yoga's distributeFreeSpaceSecondPass)
        for (int ci=0; ci<child_count; ci++) {
            int cid=cids[ci];
            KaintanaLayout* cl=&sess->layouts[sess->nodes[cid].layout_arena_index];
            float basis=bases[ci], mm=mm_a[ci];
            float min_m=min_m_a[ci], max_m=max_m_a[ci], auto_min=auto_min_sz[ci];

            float final_sz;
            if (frozen[ci]) {
                final_sz=tentatives[ci];
            } else if (remaining>0.0f && sum_grow_pass>0.0f) {
                float share=cl->flex_grow/sum_grow_pass;
                final_sz=basis+share*remaining;
            } else if (remaining<0.0f && sum_shrink_pass>0.0f) {
                float scaled=cl->flex_shrink*basis;
                float share=scaled/sum_shrink_pass;
                final_sz=basis+remaining*share;
            } else {
                final_sz=basis;
            }
            sizes[ci]=bound_axis_with_auto_min(final_sz,mm,min_m,max_m,auto_min);
        }

        // ── COMPUTE ACTUAL REMAINING FOR JUSTIFY-CONTENT ────────────
        float actual_remaining=container_main;
        for (int ci=0; ci<child_count; ci++) actual_remaining-=sizes[ci]+mm_a[ci];
        // Yoga: fallback to FlexStart on overflow
        int jmode=justify_mode;
        if (actual_remaining<0.0f && jmode!=0) jmode=0;
        float gap=(child_count>1)?justify_gap(jmode,actual_remaining,child_count):0.0f;

        // ── POSITION CHILDREN ───────────────────────────────────────
        float positions[KAINTANA_MAX_CHILDREN];
        float cross_pos[KAINTANA_MAX_CHILDREN],cross_sz[KAINTANA_MAX_CHILDREN];
        float cursor=0.0f;

        for (int ci=0; ci<child_count; ci++) {
            int cid=cids[ci];
            KaintanaLayout* cl=&sess->layouts[sess->nodes[cid].layout_arena_index];
            float mm=mm_a[ci];

            positions[ci]=cursor+mm*0.5f;
            cursor+=sizes[ci]+mm+gap;

            float child_cross=(axis==0)?cl->desired_height:cl->desired_width;
            float max_cross=(axis==0)?avail_h:avail_w;
            if (align_mode==0) {
                child_cross=max_cross-((axis==0)?cl->margin_top+cl->margin_bottom:cl->margin_left+cl->margin_right);
            }
            cross_sz[ci]=child_cross;
            cross_pos[ci]=align_cross_axis(align_mode,0.0f,max_cross,child_cross,
                (axis==0)?cl->margin_top:cl->margin_left,
                (axis==0)?cl->margin_bottom:cl->margin_right,0.0f,0.0f);
        }

        for (int ci=0;ci<child_count;ci++) {
            int cid=cids[ci];
            KaintanaLayout* cl=&sess->layouts[sess->nodes[cid].layout_arena_index];
            if (axis==0) {
                cl->resolved_x=l->resolved_x+pad[0]+positions[ci];
                cl->resolved_y=l->resolved_y+pad[1]+cross_pos[ci];
                cl->resolved_width=sizes[ci];cl->resolved_height=cross_sz[ci];
            } else {
                cl->resolved_x=l->resolved_x+pad[0]+cross_pos[ci];
                cl->resolved_y=l->resolved_y+pad[1]+positions[ci];
                cl->resolved_width=cross_sz[ci];cl->resolved_height=sizes[ci];
            }
        }
    }
}
