# Nuklear Natural Include

This blade proves that Kain can import a header-first C library naturally:

```kn
include nuklear.h as nk
```

`nuklear.c` exists only to provide the required `NK_IMPLEMENTATION` translation
unit. The Kain surface uses the same alias as the library, so C symbols like
`nk_strlen` stay naturally named instead of becoming `nk_nk_strlen`.

