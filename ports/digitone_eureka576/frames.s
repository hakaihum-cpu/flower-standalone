/* SPDX-License-Identifier: GPL-2.0-or-later */
        .section .run, "ax"
        .balign 4
        .globl eureka576_frames
eureka576_frames:
        .incbin "frames.bin"
