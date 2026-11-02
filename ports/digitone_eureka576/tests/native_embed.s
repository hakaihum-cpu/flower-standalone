        .section .rodata, "a"
        .balign 4
        .globl eureka576_frames
        .globl eureka576_frames_end
        .globl eureka576_reference
        .globl eureka576_reference_end
eureka576_frames:
        .incbin "frames.bin"
eureka576_frames_end:
        .balign 4
eureka576_reference:
        .incbin "reference.bin"
eureka576_reference_end:
        .section .note.GNU-stack,"",@progbits
