/* SPDX-License-Identifier: GPL-2.0-or-later */
        .section .run, "ax"
        .balign 4
        .globl eureka576_frames
        .globl eureka576_frames_end
eureka576_frames:
        .incbin "frames.bin"
eureka576_frames_end:
        .balign 4
