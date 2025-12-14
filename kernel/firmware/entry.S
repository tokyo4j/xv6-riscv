        # qemu -kernel loads the kernel at 0x80000000
        # and causes each hart (i.e. CPU) to jump there.
        # kernel.ld causes the following code to
        # be placed at 0x80000000.
.section .text
.align 4
.global _entry
_entry:
        # sp = m_stack + ((hartid + 1) * 4096)
        la sp, m_stack
        li a0, 4096
        csrr a1, mhartid
        addi a1, a1, 1
        mul a0, a0, a1
        add sp, sp, a0
        # jump to start() in start.c
        call start
spin:
        j spin

.align 4
.global world_enter
world_enter:
        sd s0, 0*8(a0)
        sd s1, 1*8(a0)
        sd s2, 2*8(a0)
        sd s3, 3*8(a0)
        sd s4, 4*8(a0)
        sd s5, 5*8(a0)
        sd s6, 6*8(a0)
        sd s7, 7*8(a0)
        sd s8, 8*8(a0)
        sd s9, 9*8(a0)
        sd s10, 10*8(a0)
        sd s11, 11*8(a0)
        sd sp, 12*8(a0)
        sd ra, 13*8(a0)

        la t0, 1f
        csrw mtvec, t0

        ld t0, 46*8(a0)
        csrw mstatus, t0
        ld t0, 47*8(a0)
        csrw sip, t0
        ld t0, 48*8(a0)
        csrw sie, t0
        ld t0, 49*8(a0)
        csrw stvec, t0
        ld t0, 50*8(a0)
        csrw sscratch, t0
        ld t0, 51*8(a0)
        csrw sepc, t0
        ld t0, 52*8(a0)
        csrw scause, t0
        ld t0, 53*8(a0)
        csrw stval, t0
        ld t0, 54*8(a0)
        csrw satp, t0
        ld t0, 55*8(a0)
        csrw scounteren, t0
        ld t0, 56*8(a0)
        csrw senvcfg, t0
        ld t0, 57*8(a0)
        csrw vsstatus, t0
        ld t0, 58*8(a0)
        csrw vsie, t0
        ld t0, 59*8(a0)
        csrw vstvec, t0
        ld t0, 60*8(a0)
        csrw vsscratch, t0
        ld t0, 61*8(a0)
        csrw vsepc, t0
        ld t0, 62*8(a0)
        csrw vscause, t0
        ld t0, 63*8(a0)
        csrw vstval, t0
        ld t0, 64*8(a0)
        csrw vsatp, t0
        ld t0, 65*8(a0)
        csrw hvip, t0
        ld t0, 66*8(a0)
        csrw mepc, t0

        ld t0, 24*8(a0)
        csrw mscratch, t0

        #ld x0, 14*8(a0)
        ld x1, 15*8(a0)
        ld x2, 16*8(a0)
        ld x3, 17*8(a0)
        ld x4, 18*8(a0)
        ld x5, 19*8(a0)
        ld x6, 20*8(a0)
        ld x7, 21*8(a0)
        ld x8, 22*8(a0)
        ld x9, 23*8(a0)
        # skip a0
        ld x11, 25*8(a0)
        ld x12, 26*8(a0)
        ld x13, 27*8(a0)
        ld x14, 28*8(a0)
        ld x15, 29*8(a0)
        ld x16, 30*8(a0)
        ld x17, 31*8(a0)
        ld x18, 32*8(a0)
        ld x19, 33*8(a0)
        ld x20, 34*8(a0)
        ld x21, 35*8(a0)
        ld x22, 36*8(a0)
        ld x23, 37*8(a0)
        ld x24, 38*8(a0)
        ld x25, 39*8(a0)
        ld x26, 40*8(a0)
        ld x27, 41*8(a0)
        ld x28, 42*8(a0)
        ld x29, 43*8(a0)
        ld x30, 44*8(a0)
        ld x31, 45*8(a0)
        csrrw a0, mscratch, a0
        mret
.align 4
world_enter_ret:
1:
        csrrw a0, mscratch, a0
        sd x1, 15*8(a0)
        sd x2, 16*8(a0)
        sd x3, 17*8(a0)
        sd x4, 18*8(a0)
        sd x5, 19*8(a0)
        sd x6, 20*8(a0)
        sd x7, 21*8(a0)
        sd x8, 22*8(a0)
        sd x9, 23*8(a0)
        # skip a0
        sd x11, 25*8(a0)
        sd x12, 26*8(a0)
        sd x13, 27*8(a0)
        sd x14, 28*8(a0)
        sd x15, 29*8(a0)
        sd x16, 30*8(a0)
        sd x17, 31*8(a0)
        sd x18, 32*8(a0)
        sd x19, 33*8(a0)
        sd x20, 34*8(a0)
        sd x21, 35*8(a0)
        sd x22, 36*8(a0)
        sd x23, 37*8(a0)
        sd x24, 38*8(a0)
        sd x25, 39*8(a0)
        sd x26, 40*8(a0)
        sd x27, 41*8(a0)
        sd x28, 42*8(a0)
        sd x29, 43*8(a0)
        sd x30, 44*8(a0)
        sd x31, 45*8(a0)

        csrr t0, mscratch
        sd t0, 24*8(a0)

        csrr t0, mstatus
        sd t0, 46*8(a0)
        csrr t0, sip
        sd t0, 47*8(a0)
        csrr t0, sie
        sd t0, 48*8(a0)
        csrr t0, stvec
        sd t0, 49*8(a0)
        csrr t0, sscratch
        sd t0, 50*8(a0)
        csrr t0, sepc
        sd t0, 51*8(a0)
        csrr t0, scause
        sd t0, 52*8(a0)
        csrr t0, stval
        sd t0, 53*8(a0)
        csrr t0, satp
        sd t0, 54*8(a0)
        csrr t0, scounteren
        sd t0, 55*8(a0)
        csrr t0, senvcfg
        sd t0, 56*8(a0)
        csrr t0, vsstatus
        sd t0, 57*8(a0)
        csrr t0, vsie
        sd t0, 58*8(a0)
        csrr t0, vstvec
        sd t0, 59*8(a0)
        csrr t0, vsscratch
        sd t0, 60*8(a0)
        csrr t0, vsepc
        sd t0, 61*8(a0)
        csrr t0, vscause
        sd t0, 62*8(a0)
        csrr t0, vstval
        sd t0, 63*8(a0)
        csrr t0, vsatp
        sd t0, 64*8(a0)
        csrr t0, hvip
        sd t0, 65*8(a0)
        csrr t0, mepc
        sd t0, 66*8(a0)

        ld s0, 0*8(a0)
        ld s1, 1*8(a0)
        ld s2, 2*8(a0)
        ld s3, 3*8(a0)
        ld s4, 4*8(a0)
        ld s5, 5*8(a0)
        ld s6, 6*8(a0)
        ld s7, 7*8(a0)
        ld s8, 8*8(a0)
        ld s9, 9*8(a0)
        ld s10, 10*8(a0)
        ld s11, 11*8(a0)
        ld sp, 12*8(a0)
        ld ra, 13*8(a0)

        csrr tp, mhartid

        ret
