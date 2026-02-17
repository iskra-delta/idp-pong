        .module sdcc_compat

        .globl ___sdcc_call_iy

        .area _CODE

; SDCC helper thunk for indirect calls through IY.
___sdcc_call_iy::
        jp      (iy)
