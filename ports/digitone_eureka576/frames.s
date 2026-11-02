/* SPDX-License-Identifier: GPL-2.0-or-later */
        .section .run, "ax"
        .balign 4
        .globl eureka576_frames
        .globl eureka576_frames_end
        .globl eureka576_seek_table
        .globl eureka576_seek_table_end
eureka576_frames:
        .incbin "frames.bin"
eureka576_frames_end:
        .balign 4
eureka576_seek_table:
        .incbin "seek.bin"
eureka576_seek_table_end:
        .balign 4
