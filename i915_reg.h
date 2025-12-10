#ifndef i915_REG_H
#define i915_REG_H



#define GU_CNTL_PROTECTED		_MMIO(0x10100C)
#define   DEPRESENT			REG_BIT(9)

#define GU_CNTL				_MMIO(0x101010)
#define   LMEM_INIT			REG_BIT(7)
#define   DRIVERFLR			REG_BIT(31)
#define GU_DEBUG			_MMIO(0x101018)
#define   DRIVERFLR_STATUS		REG_BIT(31)

#define GEN6_STOLEN_RESERVED		_MMIO(0x1082C0)
#define GEN6_STOLEN_RESERVED_ADDR_MASK	(0xFFF << 20)
#define GEN7_STOLEN_RESERVED_ADDR_MASK	(0x3FFF << 18)
#define GEN6_STOLEN_RESERVED_SIZE_MASK	(3 << 4)
#define GEN6_STOLEN_RESERVED_1M		(0 << 4)
#define GEN6_STOLEN_RESERVED_512K	(1 << 4)
#define GEN6_STOLEN_RESERVED_256K	(2 << 4)
#define GEN6_STOLEN_RESERVED_128K	(3 << 4)
#define GEN7_STOLEN_RESERVED_SIZE_MASK	(1 << 5)
#define GEN7_STOLEN_RESERVED_1M		(0 << 5)
#define GEN7_STOLEN_RESERVED_256K	(1 << 5)
#define GEN8_STOLEN_RESERVED_SIZE_MASK	(3 << 7)
#define GEN8_STOLEN_RESERVED_1M		(0 << 7)
#define GEN8_STOLEN_RESERVED_2M		(1 << 7)
#define GEN8_STOLEN_RESERVED_4M		(2 << 7)
#define GEN8_STOLEN_RESERVED_8M		(3 << 7)
#define GEN6_STOLEN_RESERVED_ENABLE	(1 << 0)
#define GEN11_STOLEN_RESERVED_ADDR_MASK	(0xFFFFFFFFFFFULL << 20)

/*
 * Reset registers
 */
#define DEBUG_RESET_I830		_MMIO(0x6070)
#define  DEBUG_RESET_FULL		(1 << 7)
#define  DEBUG_RESET_RENDER		(1 << 8)
#define  DEBUG_RESET_DISPLAY		(1 << 9)

/*
 * IOSF sideband
 */
#define VLV_IOSF_DOORBELL_REQ			_MMIO(VLV_DISPLAY_BASE + 0x2100)
#define   IOSF_DEVFN_SHIFT			24
#define   IOSF_OPCODE_SHIFT			16
#define   IOSF_PORT_SHIFT			8
#define   IOSF_BYTE_ENABLES_SHIFT		4
#define   IOSF_BAR_SHIFT			1
#define   IOSF_SB_BUSY				(1 << 0)
#define   IOSF_PORT_BUNIT			0x03
#define   IOSF_PORT_PUNIT			0x04
#define   IOSF_PORT_NC				0x11
#define   IOSF_PORT_DPIO			0x12
#define   IOSF_PORT_GPIO_NC			0x13
#define   IOSF_PORT_CCK				0x14
#define   IOSF_PORT_DPIO_2			0x1a
#define   IOSF_PORT_FLISDSI			0x1b
#define   IOSF_PORT_GPIO_SC			0x48
#define   IOSF_PORT_GPIO_SUS			0xa8
#define   IOSF_PORT_CCU				0xa9
#define   CHV_IOSF_PORT_GPIO_N			0x13
#define   CHV_IOSF_PORT_GPIO_SE			0x48
#define   CHV_IOSF_PORT_GPIO_E			0xa8
#define   CHV_IOSF_PORT_GPIO_SW			0xb2
#define VLV_IOSF_DATA				_MMIO(VLV_DISPLAY_BASE + 0x2104)
#define VLV_IOSF_ADDR				_MMIO(VLV_DISPLAY_BASE + 0x2108)

/* DPIO registers */
#define DPIO_DEVFN			0

/*
 * Fence registers
 * [0-7]  @ 0x2000 gen2,gen3
 * [8-15] @ 0x3000 945,g33,pnv
 *
 * [0-15] @ 0x3000 gen4,gen5
 *
 * [0-15] @ 0x100000 gen6,vlv,chv
 * [0-31] @ 0x100000 gen7+
 */
#define FENCE_REG(i)			_MMIO(0x2000 + (((i) & 8) << 9) + ((i) & 7) * 4)
#define   I830_FENCE_START_MASK		0x07f80000
#define   I830_FENCE_TILING_Y_SHIFT	12
#define   I830_FENCE_SIZE_BITS(size)	((ffs((size) >> 19) - 1) << 8)
#define   I830_FENCE_PITCH_SHIFT	4
#define   I830_FENCE_REG_VALID		(1 << 0)
#define   I915_FENCE_MAX_PITCH_VAL	4
#define   I830_FENCE_MAX_PITCH_VAL	6
#define   I830_FENCE_MAX_SIZE_VAL	(1 << 8)

#define   I915_FENCE_START_MASK		0x0ff00000
#define   I915_FENCE_SIZE_BITS(size)	((ffs((size) >> 20) - 1) << 8)

#define FENCE_REG_965_LO(i)		_MMIO(0x03000 + (i) * 8)
#define FENCE_REG_965_HI(i)		_MMIO(0x03000 + (i) * 8 + 4)
#define   I965_FENCE_PITCH_SHIFT	2
#define   I965_FENCE_TILING_Y_SHIFT	1
#define   I965_FENCE_REG_VALID		(1 << 0)
#define   I965_FENCE_MAX_PITCH_VAL	0x0400

#define FENCE_REG_GEN6_LO(i)		_MMIO(0x100000 + (i) * 8)
#define FENCE_REG_GEN6_HI(i)		_MMIO(0x100000 + (i) * 8 + 4)
#define   GEN6_FENCE_PITCH_SHIFT	32
#define   GEN7_FENCE_MAX_PITCH_VAL	0x0800


/* control register for cpu gtt access */
#define TILECTL				_MMIO(0x101000)
#define   TILECTL_SWZCTL			(1 << 0)
#define   TILECTL_TLBPF			(1 << 1)
#define   TILECTL_TLB_PREFETCH_DIS	(1 << 2)
#define   TILECTL_BACKSNOOP_DIS		(1 << 3)

/*
 * Instruction and interrupt control regs
 */
#define PGTBL_CTL	_MMIO(0x02020)
#define   PGTBL_ADDRESS_LO_MASK	0xfffff000 /* bits [31:12] */
#define   PGTBL_ADDRESS_HI_MASK	0x000000f0 /* bits [35:32] (gen4) */
#define PGTBL_ER	_MMIO(0x02024)
#define PRB0_BASE	(0x2030 - 0x30)
#define PRB1_BASE	(0x2040 - 0x30) /* 830,gen3 */
#define PRB2_BASE	(0x2050 - 0x30) /* gen3 */
#define SRB0_BASE	(0x2100 - 0x30) /* gen2 */
#define SRB1_BASE	(0x2110 - 0x30) /* gen2 */
#define SRB2_BASE	(0x2120 - 0x30) /* 830 */
#define SRB3_BASE	(0x2130 - 0x30) /* 830 */
#define RENDER_RING_BASE	0x02000
#define BSD_RING_BASE		0x04000
#define GEN6_BSD_RING_BASE	0x12000
#define GEN8_BSD2_RING_BASE	0x1c000
#define GEN11_BSD_RING_BASE	0x1c0000
#define GEN11_BSD2_RING_BASE	0x1c4000
#define GEN11_BSD3_RING_BASE	0x1d0000
#define GEN11_BSD4_RING_BASE	0x1d4000
#define XEHP_BSD5_RING_BASE	0x1e0000
#define XEHP_BSD6_RING_BASE	0x1e4000
#define XEHP_BSD7_RING_BASE	0x1f0000
#define XEHP_BSD8_RING_BASE	0x1f4000
#define VEBOX_RING_BASE		0x1a000
#define GEN11_VEBOX_RING_BASE		0x1c8000
#define GEN11_VEBOX2_RING_BASE		0x1d8000
#define XEHP_VEBOX3_RING_BASE		0x1e8000
#define XEHP_VEBOX4_RING_BASE		0x1f8000
#define MTL_GSC_RING_BASE		0x11a000
#define GEN12_COMPUTE0_RING_BASE	0x1a000
#define GEN12_COMPUTE1_RING_BASE	0x1c000
#define GEN12_COMPUTE2_RING_BASE	0x1e000
#define GEN12_COMPUTE3_RING_BASE	0x26000
#define BLT_RING_BASE		0x22000
#define XEHPC_BCS1_RING_BASE	0x3e0000
#define XEHPC_BCS2_RING_BASE	0x3e2000
#define XEHPC_BCS3_RING_BASE	0x3e4000
#define XEHPC_BCS4_RING_BASE	0x3e6000
#define XEHPC_BCS5_RING_BASE	0x3e8000
#define XEHPC_BCS6_RING_BASE	0x3ea000
#define XEHPC_BCS7_RING_BASE	0x3ec000
#define XEHPC_BCS8_RING_BASE	0x3ee000
#define DG1_GSC_HECI1_BASE	0x00258000
#define DG1_GSC_HECI2_BASE	0x00259000
#define DG2_GSC_HECI1_BASE	0x00373000
#define DG2_GSC_HECI2_BASE	0x00374000
#define MTL_GSC_HECI1_BASE	0x00116000
#define MTL_GSC_HECI2_BASE	0x00117000

#define HECI_H_CSR(base)	_MMIO((base) + 0x4)
#define   HECI_H_CSR_IE		REG_BIT(0)
#define   HECI_H_CSR_IS		REG_BIT(1)
#define   HECI_H_CSR_IG		REG_BIT(2)
#define   HECI_H_CSR_RDY	REG_BIT(3)
#define   HECI_H_CSR_RST	REG_BIT(4)

#define HECI_H_GS1(base)	_MMIO((base) + 0xc4c)
#define   HECI_H_GS1_ER_PREP	REG_BIT(0)

/*
 * The FWSTS register values are FW defined and can be different between
 * HECI1 and HECI2
 */
#define HECI_FWSTS1				0xc40
#define   HECI1_FWSTS1_CURRENT_STATE			REG_GENMASK(3, 0)
#define   HECI1_FWSTS1_CURRENT_STATE_RESET		0
#define   HECI1_FWSTS1_PROXY_STATE_NORMAL		5
#define   HECI1_FWSTS1_INIT_COMPLETE			REG_BIT(9)
#define HECI_FWSTS2				0xc48
#define HECI_FWSTS3				0xc60
#define HECI_FWSTS4				0xc64
#define HECI_FWSTS5				0xc68
#define   HECI1_FWSTS5_HUC_AUTH_DONE	(1 << 19)
#define HECI_FWSTS6				0xc6c

/* the FWSTS regs are 1-based, so we use -base for index 0 to get an invalid reg */
#define HECI_FWSTS(base, x) _MMIO((base) + _PICK(x, -(base), \
						    HECI_FWSTS1, \
						    HECI_FWSTS2, \
						    HECI_FWSTS3, \
						    HECI_FWSTS4, \
						    HECI_FWSTS5, \
						    HECI_FWSTS6))

#define HSW_GTT_CACHE_EN	_MMIO(0x4024)
#define   GTT_CACHE_EN_ALL	0xF0007FFF
#define GEN7_WR_WATERMARK	_MMIO(0x4028)
#define GEN7_GFX_PRIO_CTRL	_MMIO(0x402C)
#define ARB_MODE		_MMIO(0x4030)
#define   ARB_MODE_SWIZZLE_SNB	(1 << 4)
#define   ARB_MODE_SWIZZLE_IVB	(1 << 5)
#define GEN7_GFX_PEND_TLB0	_MMIO(0x4034)
#define GEN7_GFX_PEND_TLB1	_MMIO(0x4038)
/* L3, CVS, ZTLB, RCC, CASC LRA min, max values */
#define GEN7_LRA_LIMITS(i)	_MMIO(0x403C + (i) * 4)
#define GEN7_LRA_LIMITS_REG_NUM	13
#define GEN7_MEDIA_MAX_REQ_COUNT	_MMIO(0x4070)
#define GEN7_GFX_MAX_REQ_COUNT		_MMIO(0x4074)

#define GEN7_ERR_INT	_MMIO(0x44040)
#define   ERR_INT_POISON		(1 << 31)
#define   ERR_INT_INVALID_GTT_PTE	(1 << 29)
#define   ERR_INT_INVALID_PTE_DATA	(1 << 28)
#define   ERR_INT_SPRITE_C_FAULT	(1 << 23)
#define   ERR_INT_PRIMARY_C_FAULT	(1 << 22)
#define   ERR_INT_CURSOR_C_FAULT	(1 << 21)
#define   ERR_INT_SPRITE_B_FAULT	(1 << 20)
#define   ERR_INT_PRIMARY_B_FAULT	(1 << 19)
#define   ERR_INT_CURSOR_B_FAULT	(1 << 18)
#define   ERR_INT_SPRITE_A_FAULT	(1 << 17)
#define   ERR_INT_PRIMARY_A_FAULT	(1 << 16)
#define   ERR_INT_CURSOR_A_FAULT	(1 << 15)
#define   ERR_INT_MMIO_UNCLAIMED	(1 << 13)
#define   ERR_INT_PIPE_CRC_DONE_C	(1 << 8)
#define   ERR_INT_FIFO_UNDERRUN_C	(1 << 6)
#define   ERR_INT_PIPE_CRC_DONE_B	(1 << 5)
#define   ERR_INT_FIFO_UNDERRUN_B	(1 << 3)
#define   ERR_INT_PIPE_CRC_DONE_A	(1 << 2)
#define   ERR_INT_PIPE_CRC_DONE(pipe)	(1 << (2 + (pipe) * 3))
#define   ERR_INT_FIFO_UNDERRUN_A	(1 << 0)
#define   ERR_INT_FIFO_UNDERRUN(pipe)	(1 << ((pipe) * 3))

#define FPGA_DBG		_MMIO(0x42300)
#define   FPGA_DBG_RM_NOCLAIM	REG_BIT(31)

#define CLAIM_ER		_MMIO(VLV_DISPLAY_BASE + 0x2028)
#define   CLAIM_ER_CLR		REG_BIT(31)
#define   CLAIM_ER_OVERFLOW	REG_BIT(16)
#define   CLAIM_ER_CTR_MASK	REG_GENMASK(15, 0)

#define VLV_GU_CTL0	_MMIO(VLV_DISPLAY_BASE + 0x2030)
#define VLV_GU_CTL1	_MMIO(VLV_DISPLAY_BASE + 0x2034)
#define SCPD0		_MMIO(0x209c) /* 915+ only */
#define  SCPD_FBC_IGNORE_3D			(1 << 6)
#define  CSTATE_RENDER_CLOCK_GATE_DISABLE	(1 << 5)
#define GEN2_IER	_MMIO(0x20a0)
#define GEN2_IIR	_MMIO(0x20a4)
#define GEN2_IMR	_MMIO(0x20a8)
#define GEN2_ISR	_MMIO(0x20ac)

#define GEN2_IRQ_REGS		I915_IRQ_REGS(GEN2_IMR, \
					      GEN2_IER, \
					      GEN2_IIR)

#define VLV_GUNIT_CLOCK_GATE	_MMIO(VLV_DISPLAY_BASE + 0x2060)
#define   GINT_DIS		(1 << 22)
#define   GCFG_DIS		(1 << 8)
#define VLV_GUNIT_CLOCK_GATE2	_MMIO(VLV_DISPLAY_BASE + 0x2064)
#define VLV_IIR_RW	_MMIO(VLV_DISPLAY_BASE + 0x2084)
#define VLV_IER		_MMIO(VLV_DISPLAY_BASE + 0x20a0)
#define VLV_IIR		_MMIO(VLV_DISPLAY_BASE + 0x20a4)
#define VLV_IMR		_MMIO(VLV_DISPLAY_BASE + 0x20a8)
#define VLV_ISR		_MMIO(VLV_DISPLAY_BASE + 0x20ac)
#define VLV_PCBR	_MMIO(VLV_DISPLAY_BASE + 0x2120)
#define VLV_PCBR_ADDR_SHIFT	12

#define EIR		_MMIO(0x20b0)
#define EMR		_MMIO(0x20b4)
#define ESR		_MMIO(0x20b8)
#define   GM45_ERROR_PAGE_TABLE				(1 << 5)
#define   GM45_ERROR_MEM_PRIV				(1 << 4)
#define   I915_ERROR_PAGE_TABLE				(1 << 4)
#define   GM45_ERROR_CP_PRIV				(1 << 3)
#define   I915_ERROR_MEMORY_REFRESH			(1 << 1)
#define   I915_ERROR_INSTRUCTION			(1 << 0)

#define GEN2_ERROR_REGS		I915_ERROR_REGS(EMR, EIR)

#define INSTPM	        _MMIO(0x20c0)
#define   INSTPM_SELF_EN (1 << 12) /* 915GM only */
#define   INSTPM_AGPBUSY_INT_EN (1 << 11) /* gen3: when disabled, pending interrupts
					will not assert AGPBUSY# and will only
					be delivered when out of C3. */
#define   INSTPM_FORCE_ORDERING				(1 << 7) /* GEN6+ */
#define   INSTPM_TLB_INVALIDATE	(1 << 9)
#define   INSTPM_SYNC_FLUSH	(1 << 5)
#define MEM_MODE	_MMIO(0x20cc)
#define   MEM_DISPLAY_B_TRICKLE_FEED_DISABLE (1 << 3) /* 830 only */
#define   MEM_DISPLAY_A_TRICKLE_FEED_DISABLE (1 << 2) /* 830/845 only */
#define   MEM_DISPLAY_TRICKLE_FEED_DISABLE (1 << 2) /* 85x only */
#define FW_BLC		_MMIO(0x20d8)
#define FW_BLC2		_MMIO(0x20dc)
#define FW_BLC_SELF	_MMIO(0x20e0) /* 915+ only */
#define   FW_BLC_SELF_EN_MASK      REG_BIT(31)
#define   FW_BLC_SELF_FIFO_MASK    REG_BIT(16) /* 945 only */
#define   FW_BLC_SELF_EN           REG_BIT(15) /* 945 only */
#define MM_BURST_LENGTH     0x00700000
#define MM_FIFO_WATERMARK   0x0001F000
#define LM_BURST_LENGTH     0x00000700
#define LM_FIFO_WATERMARK   0x0000001F
#define MI_ARB_STATE	_MMIO(0x20e4) /* 915+ only */

/*
 * Make render/texture TLB fetches lower priority than associated data
 * fetches. This is not turned on by default.
 */
#define   MI_ARB_RENDER_TLB_LOW_PRIORITY	(1 << 15)

/* Isoch request wait on GTT enable (Display A/B/C streams).
 * Make isoch requests stall on the TLB update. May cause
 * display underruns (test mode only)
 */
#define   MI_ARB_ISOCH_WAIT_GTT			(1 << 14)

/* Block grant count for isoch requests when block count is
 * set to a finite value.
 */
#define   MI_ARB_BLOCK_GRANT_MASK		(3 << 12)
#define   MI_ARB_BLOCK_GRANT_8			(0 << 12)	/* for 3 display planes */
#define   MI_ARB_BLOCK_GRANT_4			(1 << 12)	/* for 2 display planes */
#define   MI_ARB_BLOCK_GRANT_2			(2 << 12)	/* for 1 display plane */
#define   MI_ARB_BLOCK_GRANT_0			(3 << 12)	/* don't use */

/* Enable render writes to complete in C2/C3/C4 power states.
 * If this isn't enabled, render writes are prevented in low
 * power states. That seems bad to me.
 */
#define   MI_ARB_C3_LP_WRITE_ENABLE		(1 << 11)

/* This acknowledges an async flip immediately instead
 * of waiting for 2TLB fetches.
 */
#define   MI_ARB_ASYNC_FLIP_ACK_IMMEDIATE	(1 << 10)

/* Enables non-sequential data reads through arbiter
 */
#define   MI_ARB_DUAL_DATA_PHASE_DISABLE	(1 << 9)

/* Disable FSB snooping of cacheable write cycles from binner/render
 * command stream
 */
#define   MI_ARB_CACHE_SNOOP_DISABLE		(1 << 8)

/* Arbiter time slice for non-isoch streams */
#define   MI_ARB_TIME_SLICE_MASK		(7 << 5)
#define   MI_ARB_TIME_SLICE_1			(0 << 5)
#define   MI_ARB_TIME_SLICE_2			(1 << 5)
#define   MI_ARB_TIME_SLICE_4			(2 << 5)
#define   MI_ARB_TIME_SLICE_6			(3 << 5)
#define   MI_ARB_TIME_SLICE_8			(4 << 5)
#define   MI_ARB_TIME_SLICE_10			(5 << 5)
#define   MI_ARB_TIME_SLICE_14			(6 << 5)
#define   MI_ARB_TIME_SLICE_16			(7 << 5)

/* Low priority grace period page size */
#define   MI_ARB_LOW_PRIORITY_GRACE_4KB		(0 << 4)	/* default */
#define   MI_ARB_LOW_PRIORITY_GRACE_8KB		(1 << 4)

/* Disable display A/B trickle feed */
#define   MI_ARB_DISPLAY_TRICKLE_FEED_DISABLE	(1 << 2)

/* Set display plane priority */
#define   MI_ARB_DISPLAY_PRIORITY_A_B		(0 << 0)	/* display A > display B */
#define   MI_ARB_DISPLAY_PRIORITY_B_A		(1 << 0)	/* display B > display A */

#define MI_STATE	_MMIO(0x20e4) /* gen2 only */
#define   MI_AGPBUSY_INT_EN			(1 << 1) /* 85x only */
#define   MI_AGPBUSY_830_MODE			(1 << 0) /* 85x only */



#define GT_BLT_FLUSHDW_NOTIFY_INTERRUPT		(1 << 26)
#define GT_BLT_CS_ERROR_INTERRUPT		(1 << 25)
#define GT_BLT_USER_INTERRUPT			(1 << 22)
#define GT_BSD_CS_ERROR_INTERRUPT		(1 << 15)
#define GT_BSD_USER_INTERRUPT			(1 << 12)
#define GT_RENDER_L3_PARITY_ERROR_INTERRUPT_S1	(1 << 11) /* hsw+; rsvd on snb, ivb, vlv */
#define GT_WAIT_SEMAPHORE_INTERRUPT		REG_BIT(11) /* bdw+ */
#define GT_CONTEXT_SWITCH_INTERRUPT		(1 <<  8)
#define GT_RENDER_L3_PARITY_ERROR_INTERRUPT	(1 <<  5) /* !snb */
#define GT_RENDER_PIPECTL_NOTIFY_INTERRUPT	(1 <<  4)
#define GT_CS_MASTER_ERROR_INTERRUPT		REG_BIT(3)
#define GT_RENDER_SYNC_STATUS_INTERRUPT		(1 <<  2)
#define GT_RENDER_DEBUG_INTERRUPT		(1 <<  1)
#define GT_RENDER_USER_INTERRUPT		(1 <<  0)

#define PM_VEBOX_CS_ERROR_INTERRUPT		(1 << 12) /* hsw+ */
#define PM_VEBOX_USER_INTERRUPT			(1 << 10) /* hsw+ */

#define GT_PARITY_ERROR(dev_priv) \
	(GT_RENDER_L3_PARITY_ERROR_INTERRUPT | \
	 (IS_HASWELL(dev_priv) ? GT_RENDER_L3_PARITY_ERROR_INTERRUPT_S1 : 0))



#define GEN6_BSD_RNCID			_MMIO(0x12198)

#define GEN7_FF_THREAD_MODE		_MMIO(0x20a0)
#define   GEN7_FF_SCHED_MASK		0x0077070
#define   GEN8_FF_DS_REF_CNT_FFME	(1 << 19)
#define   GEN12_FF_TESSELATION_DOP_GATE_DISABLE BIT(19)
#define   GEN7_FF_TS_SCHED_HS1		(0x5 << 16)
#define   GEN7_FF_TS_SCHED_HS0		(0x3 << 16)
#define   GEN7_FF_TS_SCHED_LOAD_BALANCE	(0x1 << 16)
#define   GEN7_FF_TS_SCHED_HW		(0x0 << 16) /* Default */
#define   GEN7_FF_VS_REF_CNT_FFME	(1 << 15)
#define   GEN7_FF_VS_SCHED_HS1		(0x5 << 12)
#define   GEN7_FF_VS_SCHED_HS0		(0x3 << 12)
#define   GEN7_FF_VS_SCHED_LOAD_BALANCE	(0x1 << 12) /* Default */
#define   GEN7_FF_VS_SCHED_HW		(0x0 << 12)
#define   GEN7_FF_DS_SCHED_HS1		(0x5 << 4)
#define   GEN7_FF_DS_SCHED_HS0		(0x3 << 4)
#define   GEN7_FF_DS_SCHED_LOAD_BALANCE	(0x1 << 4)  /* Default */
#define   GEN7_FF_DS_SCHED_HW		(0x0 << 4)

#define ILK_DISPLAY_CHICKEN1	_MMIO(0x42000)
#define   ILK_FBCQ_DIS			REG_BIT(22)
#define   ILK_PABSTRETCH_DIS		REG_BIT(21)
#define   ILK_SABSTRETCH_DIS		REG_BIT(20)
#define   IVB_PRI_STRETCH_MAX_MASK	REG_GENMASK(21, 20)
#define   IVB_PRI_STRETCH_MAX_X8	REG_FIELD_PREP(IVB_PRI_STRETCH_MAX_MASK, 0)
#define   IVB_PRI_STRETCH_MAX_X4	REG_FIELD_PREP(IVB_PRI_STRETCH_MAX_MASK, 1)
#define   IVB_PRI_STRETCH_MAX_X2	REG_FIELD_PREP(IVB_PRI_STRETCH_MAX_MASK, 2)
#define   IVB_PRI_STRETCH_MAX_X1	REG_FIELD_PREP(IVB_PRI_STRETCH_MAX_MASK, 3)
#define   IVB_SPR_STRETCH_MAX_MASK	REG_GENMASK(19, 18)
#define   IVB_SPR_STRETCH_MAX_X8	REG_FIELD_PREP(IVB_SPR_STRETCH_MAX_MASK, 0)
#define   IVB_SPR_STRETCH_MAX_X4	REG_FIELD_PREP(IVB_SPR_STRETCH_MAX_MASK, 1)
#define   IVB_SPR_STRETCH_MAX_X2	REG_FIELD_PREP(IVB_SPR_STRETCH_MAX_MASK, 2)
#define   IVB_SPR_STRETCH_MAX_X1	REG_FIELD_PREP(IVB_SPR_STRETCH_MAX_MASK, 3)

#define DPLL_TEST	_MMIO(0x606c)
#define   DPLLB_TEST_SDVO_DIV_1		(0 << 22)
#define   DPLLB_TEST_SDVO_DIV_2		(1 << 22)
#define   DPLLB_TEST_SDVO_DIV_4		(2 << 22)
#define   DPLLB_TEST_SDVO_DIV_MASK	(3 << 22)
#define   DPLLB_TEST_N_BYPASS		(1 << 19)
#define   DPLLB_TEST_M_BYPASS		(1 << 18)
#define   DPLLB_INPUT_BUFFER_ENABLE	(1 << 16)
#define   DPLLA_TEST_N_BYPASS		(1 << 3)
#define   DPLLA_TEST_M_BYPASS		(1 << 2)
#define   DPLLA_INPUT_BUFFER_ENABLE	(1 << 0)

#define D_STATE		_MMIO(0x6104)
#define  DSTATE_GFX_RESET_I830			(1 << 6)
#define  DSTATE_PLL_D3_OFF			(1 << 3)
#define  DSTATE_GFX_CLOCK_GATING		(1 << 1)
#define  DSTATE_DOT_CLOCK_GATING		(1 << 0)

#define DSPCLK_GATE_D			_MMIO(0x6200)
#define VLV_DSPCLK_GATE_D		_MMIO(VLV_DISPLAY_BASE + 0x6200)
# define DPUNIT_B_CLOCK_GATE_DISABLE		(1 << 30) /* 965 */
# define VSUNIT_CLOCK_GATE_DISABLE		(1 << 29) /* 965 */
# define VRHUNIT_CLOCK_GATE_DISABLE		(1 << 28) /* 965 */
# define VRDUNIT_CLOCK_GATE_DISABLE		(1 << 27) /* 965 */
# define AUDUNIT_CLOCK_GATE_DISABLE		(1 << 26) /* 965 */
# define DPUNIT_A_CLOCK_GATE_DISABLE		(1 << 25) /* 965 */
# define DPCUNIT_CLOCK_GATE_DISABLE		(1 << 24) /* 965 */
# define PNV_GMBUSUNIT_CLOCK_GATE_DISABLE	(1 << 24) /* pnv */
# define TVRUNIT_CLOCK_GATE_DISABLE		(1 << 23) /* 915-945 */
# define TVCUNIT_CLOCK_GATE_DISABLE		(1 << 22) /* 915-945 */
# define TVFUNIT_CLOCK_GATE_DISABLE		(1 << 21) /* 915-945 */
# define TVEUNIT_CLOCK_GATE_DISABLE		(1 << 20) /* 915-945 */
# define DVSUNIT_CLOCK_GATE_DISABLE		(1 << 19) /* 915-945 */
# define DSSUNIT_CLOCK_GATE_DISABLE		(1 << 18) /* 915-945 */
# define DDBUNIT_CLOCK_GATE_DISABLE		(1 << 17) /* 915-945 */
# define DPRUNIT_CLOCK_GATE_DISABLE		(1 << 16) /* 915-945 */
# define DPFUNIT_CLOCK_GATE_DISABLE		(1 << 15) /* 915-945 */
# define DPBMUNIT_CLOCK_GATE_DISABLE		(1 << 14) /* 915-945 */
# define DPLSUNIT_CLOCK_GATE_DISABLE		(1 << 13) /* 915-945 */
# define DPLUNIT_CLOCK_GATE_DISABLE		(1 << 12) /* 915-945 */
# define DPOUNIT_CLOCK_GATE_DISABLE		(1 << 11)
# define DPBUNIT_CLOCK_GATE_DISABLE		(1 << 10)
# define DCUNIT_CLOCK_GATE_DISABLE		(1 << 9)
# define DPUNIT_CLOCK_GATE_DISABLE		(1 << 8)
# define VRUNIT_CLOCK_GATE_DISABLE		(1 << 7) /* 915+: reserved */
# define OVHUNIT_CLOCK_GATE_DISABLE		(1 << 6) /* 830-865 */
# define DPIOUNIT_CLOCK_GATE_DISABLE		(1 << 6) /* 915-945 */
# define OVFUNIT_CLOCK_GATE_DISABLE		(1 << 5)
# define OVBUNIT_CLOCK_GATE_DISABLE		(1 << 4)
/*
 * This bit must be set on the 830 to prevent hangs when turning off the
 * overlay scaler.
 */
# define OVRUNIT_CLOCK_GATE_DISABLE		(1 << 3)
# define OVCUNIT_CLOCK_GATE_DISABLE		(1 << 2)
# define OVUUNIT_CLOCK_GATE_DISABLE		(1 << 1)
# define ZVUNIT_CLOCK_GATE_DISABLE		(1 << 0) /* 830 */
# define OVLUNIT_CLOCK_GATE_DISABLE		(1 << 0) /* 845,865 */

#define RENCLK_GATE_D1		_MMIO(0x6204)
# define BLITTER_CLOCK_GATE_DISABLE		(1 << 13) /* 945GM only */
# define MPEG_CLOCK_GATE_DISABLE		(1 << 12) /* 945GM only */
# define PC_FE_CLOCK_GATE_DISABLE		(1 << 11)
# define PC_BE_CLOCK_GATE_DISABLE		(1 << 10)
# define WINDOWER_CLOCK_GATE_DISABLE		(1 << 9)
# define INTERPOLATOR_CLOCK_GATE_DISABLE	(1 << 8)
# define COLOR_CALCULATOR_CLOCK_GATE_DISABLE	(1 << 7)
# define MOTION_COMP_CLOCK_GATE_DISABLE		(1 << 6)
# define MAG_CLOCK_GATE_DISABLE			(1 << 5)
/* This bit must be unset on 855,865 */
# define MECI_CLOCK_GATE_DISABLE		(1 << 4)
# define DCMP_CLOCK_GATE_DISABLE		(1 << 3)
# define MEC_CLOCK_GATE_DISABLE			(1 << 2)
# define MECO_CLOCK_GATE_DISABLE		(1 << 1)
/* This bit must be set on 855,865. */
# define SV_CLOCK_GATE_DISABLE			(1 << 0)
# define I915_MPEG_CLOCK_GATE_DISABLE		(1 << 16)
# define I915_VLD_IP_PR_CLOCK_GATE_DISABLE	(1 << 15)
# define I915_MOTION_COMP_CLOCK_GATE_DISABLE	(1 << 14)
# define I915_BD_BF_CLOCK_GATE_DISABLE		(1 << 13)
# define I915_SF_SE_CLOCK_GATE_DISABLE		(1 << 12)
# define I915_WM_CLOCK_GATE_DISABLE		(1 << 11)
# define I915_IZ_CLOCK_GATE_DISABLE		(1 << 10)
# define I915_PI_CLOCK_GATE_DISABLE		(1 << 9)
# define I915_DI_CLOCK_GATE_DISABLE		(1 << 8)
# define I915_SH_SV_CLOCK_GATE_DISABLE		(1 << 7)
# define I915_PL_DG_QC_FT_CLOCK_GATE_DISABLE	(1 << 6)
# define I915_SC_CLOCK_GATE_DISABLE		(1 << 5)
# define I915_FL_CLOCK_GATE_DISABLE		(1 << 4)
# define I915_DM_CLOCK_GATE_DISABLE		(1 << 3)
# define I915_PS_CLOCK_GATE_DISABLE		(1 << 2)
# define I915_CC_CLOCK_GATE_DISABLE		(1 << 1)
# define I915_BY_CLOCK_GATE_DISABLE		(1 << 0)

# define I965_RCZ_CLOCK_GATE_DISABLE		(1 << 30)
/* This bit must always be set on 965G/965GM */
# define I965_RCC_CLOCK_GATE_DISABLE		(1 << 29)
# define I965_RCPB_CLOCK_GATE_DISABLE		(1 << 28)
# define I965_DAP_CLOCK_GATE_DISABLE		(1 << 27)
# define I965_ROC_CLOCK_GATE_DISABLE		(1 << 26)
# define I965_GW_CLOCK_GATE_DISABLE		(1 << 25)
# define I965_TD_CLOCK_GATE_DISABLE		(1 << 24)
/* This bit must always be set on 965G */
# define I965_ISC_CLOCK_GATE_DISABLE		(1 << 23)
# define I965_IC_CLOCK_GATE_DISABLE		(1 << 22)
# define I965_EU_CLOCK_GATE_DISABLE		(1 << 21)
# define I965_IF_CLOCK_GATE_DISABLE		(1 << 20)
# define I965_TC_CLOCK_GATE_DISABLE		(1 << 19)
# define I965_SO_CLOCK_GATE_DISABLE		(1 << 17)
# define I965_FBC_CLOCK_GATE_DISABLE		(1 << 16)
# define I965_MARI_CLOCK_GATE_DISABLE		(1 << 15)
# define I965_MASF_CLOCK_GATE_DISABLE		(1 << 14)
# define I965_MAWB_CLOCK_GATE_DISABLE		(1 << 13)
# define I965_EM_CLOCK_GATE_DISABLE		(1 << 12)
# define I965_UC_CLOCK_GATE_DISABLE		(1 << 11)
# define I965_SI_CLOCK_GATE_DISABLE		(1 << 6)
# define I965_MT_CLOCK_GATE_DISABLE		(1 << 5)
# define I965_PL_CLOCK_GATE_DISABLE		(1 << 4)
# define I965_DG_CLOCK_GATE_DISABLE		(1 << 3)
# define I965_QC_CLOCK_GATE_DISABLE		(1 << 2)
# define I965_FT_CLOCK_GATE_DISABLE		(1 << 1)
# define I965_DM_CLOCK_GATE_DISABLE		(1 << 0)

#define RENCLK_GATE_D2		_MMIO(0x6208)
#define VF_UNIT_CLOCK_GATE_DISABLE		(1 << 9)
#define GS_UNIT_CLOCK_GATE_DISABLE		(1 << 7)
#define CL_UNIT_CLOCK_GATE_DISABLE		(1 << 6)

#define VDECCLK_GATE_D		_MMIO(0x620C)		/* g4x only */
#define  VCP_UNIT_CLOCK_GATE_DISABLE		(1 << 4)

#define RAMCLK_GATE_D		_MMIO(0x6210)		/* CRL only */
#define DEUC			_MMIO(0x6214)          /* CRL only */

#define BXT_RP_STATE_CAP        _MMIO(0x138170)
#define GEN9_RP_STATE_LIMITS	_MMIO(0x138148)

#define MTL_RP_STATE_CAP	_MMIO(0x138000)
#define MTL_MEDIAP_STATE_CAP	_MMIO(0x138020)
#define   MTL_RP0_CAP_MASK	REG_GENMASK(8, 0)
#define   MTL_RPN_CAP_MASK	REG_GENMASK(24, 16)

#define MTL_GT_RPE_FREQUENCY	_MMIO(0x13800c)
#define MTL_MPE_FREQUENCY	_MMIO(0x13802c)
#define   MTL_RPE_MASK		REG_GENMASK(8, 0)

#define GT0_PERF_LIMIT_REASONS		_MMIO(0x1381a8)
#define   GT0_PERF_LIMIT_REASONS_MASK	0xde3
#define   PROCHOT_MASK			REG_BIT(0)
#define   THERMAL_LIMIT_MASK		REG_BIT(1)
#define   RATL_MASK			REG_BIT(5)
#define   VR_THERMALERT_MASK		REG_BIT(6)
#define   VR_TDC_MASK			REG_BIT(7)
#define   POWER_LIMIT_4_MASK		REG_BIT(8)
#define   POWER_LIMIT_1_MASK		REG_BIT(10)
#define   POWER_LIMIT_2_MASK		REG_BIT(11)
#define   GT0_PERF_LIMIT_REASONS_LOG_MASK REG_GENMASK(31, 16)
#define MTL_MEDIA_PERF_LIMIT_REASONS	_MMIO(0x138030)

#define CHV_CLK_CTL1			_MMIO(0x101100)
#define VLV_CLK_CTL2			_MMIO(0x101104)
#define   CLK_CTL2_CZCOUNT_30NS_SHIFT	28

/*
 * GEN9 clock gating regs
 */
#define GEN9_CLKGATE_DIS_0		_MMIO(0x46530)
#define   DARBF_GATING_DIS		REG_BIT(27)
#define   MTL_PIPEDMC_GATING_DIS(pipe)	REG_BIT(15 - (pipe))
#define   PWM2_GATING_DIS		REG_BIT(14)
#define   PWM1_GATING_DIS		REG_BIT(13)

#define GEN9_CLKGATE_DIS_3		_MMIO(0x46538)
#define   TGL_VRH_GATING_DIS		REG_BIT(31)
#define   DPT_GATING_DIS		REG_BIT(22)

#define VLV_DPFLIPSTAT				_MMIO(VLV_DISPLAY_BASE + 0x70028)
#define   PIPEB_LINE_COMPARE_INT_EN			REG_BIT(29)
#define   PIPEB_HLINE_INT_EN			REG_BIT(28)
#define   PIPEB_VBLANK_INT_EN			REG_BIT(27)
#define   SPRITED_FLIP_DONE_INT_EN			REG_BIT(26)
#define   SPRITEC_FLIP_DONE_INT_EN			REG_BIT(25)
#define   PLANEB_FLIP_DONE_INT_EN			REG_BIT(24)
#define   PIPE_PSR_INT_EN			REG_BIT(22)
#define   PIPEA_LINE_COMPARE_INT_EN			REG_BIT(21)
#define   PIPEA_HLINE_INT_EN			REG_BIT(20)
#define   PIPEA_VBLANK_INT_EN			REG_BIT(19)
#define   SPRITEB_FLIP_DONE_INT_EN			REG_BIT(18)
#define   SPRITEA_FLIP_DONE_INT_EN			REG_BIT(17)
#define   PLANEA_FLIPDONE_INT_EN			REG_BIT(16)
#define   PIPEC_LINE_COMPARE_INT_EN			REG_BIT(13)
#define   PIPEC_HLINE_INT_EN			REG_BIT(12)
#define   PIPEC_VBLANK_INT_EN			REG_BIT(11)
#define   SPRITEF_FLIPDONE_INT_EN			REG_BIT(10)
#define   SPRITEE_FLIPDONE_INT_EN			REG_BIT(9)
#define   PLANEC_FLIPDONE_INT_EN			REG_BIT(8)

#define PCH_3DCGDIS0		_MMIO(0x46020)
# define MARIUNIT_CLOCK_GATE_DISABLE		(1 << 18)
# define SVSMUNIT_CLOCK_GATE_DISABLE		(1 << 1)

#define PCH_3DCGDIS1		_MMIO(0x46024)
# define VFMUNIT_CLOCK_GATE_DISABLE		(1 << 11)

/* Display Internal Timeout Register */
#define RM_TIMEOUT		_MMIO(0x42060)
#define RM_TIMEOUT_REG_CAPTURE	_MMIO(0x420E0)
#define  MMIO_TIMEOUT_US(us)	((us) << 0)

/* interrupts */
#define DE_MASTER_IRQ_CONTROL   (1 << 31)
#define DE_SPRITEB_FLIP_DONE    (1 << 29)
#define DE_SPRITEA_FLIP_DONE    (1 << 28)
#define DE_PLANEB_FLIP_DONE     (1 << 27)
#define DE_PLANEA_FLIP_DONE     (1 << 26)
#define DE_PLANE_FLIP_DONE(plane) (1 << (26 + (plane)))
#define DE_PCU_EVENT            (1 << 25)
#define DE_GTT_FAULT            (1 << 24)
#define DE_POISON               (1 << 23)
#define DE_PERFORM_COUNTER      (1 << 22)
#define DE_PCH_EVENT            (1 << 21)
#define DE_AUX_CHANNEL_A        (1 << 20)
#define DE_DP_A_HOTPLUG         (1 << 19)
#define DE_GSE                  (1 << 18)
#define DE_PIPEB_VBLANK         (1 << 15)
#define DE_PIPEB_EVEN_FIELD     (1 << 14)
#define DE_PIPEB_ODD_FIELD      (1 << 13)
#define DE_PIPEB_LINE_COMPARE   (1 << 12)
#define DE_PIPEB_VSYNC          (1 << 11)
#define DE_PIPEB_CRC_DONE	(1 << 10)
#define DE_PIPEB_FIFO_UNDERRUN  (1 << 8)
#define DE_PIPEA_VBLANK         (1 << 7)
#define DE_PIPE_VBLANK(pipe)    (1 << (7 + 8 * (pipe)))
#define DE_PIPEA_EVEN_FIELD     (1 << 6)
#define DE_PIPEA_ODD_FIELD      (1 << 5)
#define DE_PIPEA_LINE_COMPARE   (1 << 4)
#define DE_PIPEA_VSYNC          (1 << 3)
#define DE_PIPEA_CRC_DONE	(1 << 2)
#define DE_PIPE_CRC_DONE(pipe)	(1 << (2 + 8 * (pipe)))
#define DE_PIPEA_FIFO_UNDERRUN  (1 << 0)
#define DE_PIPE_FIFO_UNDERRUN(pipe)  (1 << (8 * (pipe)))

#define VLV_MASTER_IER			_MMIO(0x4400c) /* Gunit master IER */
#define   MASTER_INTERRUPT_ENABLE	(1 << 31)

#define DEISR   _MMIO(0x44000)
#define DEIMR   _MMIO(0x44004)
#define DEIIR   _MMIO(0x44008)
#define DEIER   _MMIO(0x4400c)

#define DE_IRQ_REGS		I915_IRQ_REGS(DEIMR, \
					      DEIER, \
					      DEIIR)

#define GTISR   _MMIO(0x44010)
#define GTIMR   _MMIO(0x44014)
#define GTIIR   _MMIO(0x44018)
#define GTIER   _MMIO(0x4401c)

#define GT_IRQ_REGS		I915_IRQ_REGS(GTIMR, \
					      GTIER, \
					      GTIIR)

#define GEN8_MASTER_IRQ			_MMIO(0x44200)
#define  GEN8_MASTER_IRQ_CONTROL	(1 << 31)
#define  GEN8_PCU_IRQ			(1 << 30)
#define  GEN8_DE_PCH_IRQ		(1 << 23)
#define  GEN8_DE_MISC_IRQ		(1 << 22)
#define  GEN8_DE_PORT_IRQ		(1 << 20)
#define  GEN8_DE_PIPE_C_IRQ		(1 << 18)
#define  GEN8_DE_PIPE_B_IRQ		(1 << 17)
#define  GEN8_DE_PIPE_A_IRQ		(1 << 16)
#define  GEN8_DE_PIPE_IRQ(pipe)		(1 << (16 + (pipe)))
#define  GEN8_GT_VECS_IRQ		(1 << 6)
#define  GEN8_GT_GUC_IRQ		(1 << 5)
#define  GEN8_GT_PM_IRQ			(1 << 4)
#define  GEN8_GT_VCS1_IRQ		(1 << 3) /* NB: VCS2 in bspec! */
#define  GEN8_GT_VCS0_IRQ		(1 << 2) /* NB: VCS1 in bpsec! */
#define  GEN8_GT_BCS_IRQ		(1 << 1)
#define  GEN8_GT_RCS_IRQ		(1 << 0)

#define GEN8_GT_ISR(which) _MMIO(0x44300 + (0x10 * (which)))
#define GEN8_GT_IMR(which) _MMIO(0x44304 + (0x10 * (which)))
#define GEN8_GT_IIR(which) _MMIO(0x44308 + (0x10 * (which)))
#define GEN8_GT_IER(which) _MMIO(0x4430c + (0x10 * (which)))

#define GEN8_GT_IRQ_REGS(which)		I915_IRQ_REGS(GEN8_GT_IMR(which), \
						      GEN8_GT_IER(which), \
						      GEN8_GT_IIR(which))

#define GEN8_RCS_IRQ_SHIFT 0
#define GEN8_BCS_IRQ_SHIFT 16
#define GEN8_VCS0_IRQ_SHIFT 0  /* NB: VCS1 in bspec! */
#define GEN8_VCS1_IRQ_SHIFT 16 /* NB: VCS2 in bpsec! */
#define GEN8_VECS_IRQ_SHIFT 0
#define GEN8_WD_IRQ_SHIFT 16

#define GEN8_PCU_ISR _MMIO(0x444e0)
#define GEN8_PCU_IMR _MMIO(0x444e4)
#define GEN8_PCU_IIR _MMIO(0x444e8)
#define GEN8_PCU_IER _MMIO(0x444ec)

#define GEN8_PCU_IRQ_REGS		I915_IRQ_REGS(GEN8_PCU_IMR, \
						      GEN8_PCU_IER, \
						      GEN8_PCU_IIR)

#define GEN11_GU_MISC_ISR	_MMIO(0x444f0)
#define GEN11_GU_MISC_IMR	_MMIO(0x444f4)
#define GEN11_GU_MISC_IIR	_MMIO(0x444f8)
#define GEN11_GU_MISC_IER	_MMIO(0x444fc)
#define  GEN11_GU_MISC_GSE	(1 << 27)

#define GEN11_GU_MISC_IRQ_REGS		I915_IRQ_REGS(GEN11_GU_MISC_IMR, \
						      GEN11_GU_MISC_IER, \
						      GEN11_GU_MISC_IIR)

#define  GEN11_MASTER_IRQ		(1 << 31)
#define  GEN11_PCU_IRQ			(1 << 30)
#define  GEN11_GU_MISC_IRQ		(1 << 29)
#define  GEN11_DISPLAY_IRQ		(1 << 16)
#define  GEN11_GT_DW_IRQ(x)		(1 << (x))
#define  GEN11_GT_DW1_IRQ		(1 << 1)
#define  GEN11_GT_DW0_IRQ		(1 << 0)

#define DG1_MSTR_TILE_INTR		_MMIO(0x190008)
#define   DG1_MSTR_IRQ			REG_BIT(31)
#define   DG1_MSTR_TILE(t)		REG_BIT(t)

#define ILK_DISPLAY_CHICKEN2	_MMIO(0x42004)
/* Required on all Ironlake and Sandybridge according to the B-Spec. */
#define   ILK_ELPIN_409_SELECT	REG_BIT(25)
#define   ILK_DPARB_GATE	REG_BIT(22)
#define   ILK_VSDPFD_FULL	REG_BIT(21)

#define ILK_DSPCLK_GATE_D	_MMIO(0x42020)
#define   ILK_VRHUNIT_CLOCK_GATE_DISABLE	REG_BIT(28)
#define   ILK_DPFCUNIT_CLOCK_GATE_DISABLE	REG_BIT(9)
#define   ILK_DPFCRUNIT_CLOCK_GATE_DISABLE	REG_BIT(8)
#define   ILK_DPFDUNIT_CLOCK_GATE_ENABLE	REG_BIT(7)
#define   ILK_DPARBUNIT_CLOCK_GATE_ENABLE	REG_BIT(5)

#define IVB_CHICKEN3		_MMIO(0x4200c)
#define   CHICKEN3_DGMG_REQ_OUT_FIX_DISABLE	REG_BIT(5)
#define   CHICKEN3_DGMG_DONE_FIX_DISABLE	REG_BIT(2)

#define CHICKEN_PAR1_1		_MMIO(0x42080)
#define   IGNORE_KVMR_PIPE_A		REG_BIT(23)
#define   KBL_ARB_FILL_SPARE_22		REG_BIT(22)
#define   DIS_RAM_BYPASS_PSR2_MAN_TRACK	REG_BIT(16)
#define   SKL_DE_COMPRESSED_HASH_MODE	REG_BIT(15)
#define   HSW_MASK_VBL_TO_PIPE_IN_SRD	REG_BIT(15) /* hsw/bdw */
#define   FORCE_ARB_IDLE_PLANES		REG_BIT(14)
#define   SKL_EDP_PSR_FIX_RDWRAP	REG_BIT(3)
#define   IGNORE_PSR2_HW_TRACKING	REG_BIT(1)

#define CHICKEN_PAR2_1		_MMIO(0x42090)
#define   KVM_CONFIG_CHANGE_NOTIFICATION_SELECT	REG_BIT(14)

#define _CHICKEN_PIPESL_1_A	0x420b0
#define _CHICKEN_PIPESL_1_B	0x420b4
#define CHICKEN_PIPESL_1(pipe)	_MMIO_PIPE(pipe, _CHICKEN_PIPESL_1_A, _CHICKEN_PIPESL_1_B)
#define   HSW_PRI_STRETCH_MAX_MASK	REG_GENMASK(28, 27)
#define   HSW_PRI_STRETCH_MAX_X8	REG_FIELD_PREP(HSW_PRI_STRETCH_MAX_MASK, 0)
#define   HSW_PRI_STRETCH_MAX_X4	REG_FIELD_PREP(HSW_PRI_STRETCH_MAX_MASK, 1)
#define   HSW_PRI_STRETCH_MAX_X2	REG_FIELD_PREP(HSW_PRI_STRETCH_MAX_MASK, 2)
#define   HSW_PRI_STRETCH_MAX_X1	REG_FIELD_PREP(HSW_PRI_STRETCH_MAX_MASK, 3)
#define   HSW_SPR_STRETCH_MAX_MASK	REG_GENMASK(26, 25)
#define   HSW_SPR_STRETCH_MAX_X8	REG_FIELD_PREP(HSW_SPR_STRETCH_MAX_MASK, 0)
#define   HSW_SPR_STRETCH_MAX_X4	REG_FIELD_PREP(HSW_SPR_STRETCH_MAX_MASK, 1)
#define   HSW_SPR_STRETCH_MAX_X2	REG_FIELD_PREP(HSW_SPR_STRETCH_MAX_MASK, 2)
#define   HSW_SPR_STRETCH_MAX_X1	REG_FIELD_PREP(HSW_SPR_STRETCH_MAX_MASK, 3)
#define   HSW_FBCQ_DIS			REG_BIT(22)
#define   HSW_UNMASK_VBL_TO_REGS_IN_SRD REG_BIT(15) /* hsw */
#define   SKL_PSR_MASK_PLANE_FLIP	REG_BIT(11) /* skl+ */
#define   SKL_PLANE1_STRETCH_MAX_MASK	REG_GENMASK(1, 0)
#define   SKL_PLANE1_STRETCH_MAX_X8	REG_FIELD_PREP(SKL_PLANE1_STRETCH_MAX_MASK, 0)
#define   SKL_PLANE1_STRETCH_MAX_X4	REG_FIELD_PREP(SKL_PLANE1_STRETCH_MAX_MASK, 1)
#define   SKL_PLANE1_STRETCH_MAX_X2	REG_FIELD_PREP(SKL_PLANE1_STRETCH_MAX_MASK, 2)
#define   SKL_PLANE1_STRETCH_MAX_X1	REG_FIELD_PREP(SKL_PLANE1_STRETCH_MAX_MASK, 3)
#define   BDW_UNMASK_VBL_TO_REGS_IN_SRD	REG_BIT(0) /* bdw */

#define DISP_ARB_CTL	_MMIO(0x45000)
#define   DISP_FBC_MEMORY_WAKE		REG_BIT(31)
#define   DISP_TILE_SURFACE_SWIZZLING	REG_BIT(13)
#define   DISP_FBC_WM_DIS		REG_BIT(15)

#define GEN8_CHICKEN_DCPR_1			_MMIO(0x46430)
#define   _LATENCY_REPORTING_REMOVED_PIPE_D	REG_BIT(31)
#define   SKL_SELECT_ALTERNATE_DC_EXIT		REG_BIT(30)
#define   _LATENCY_REPORTING_REMOVED_PIPE_C	REG_BIT(25)
#define   _LATENCY_REPORTING_REMOVED_PIPE_B	REG_BIT(24)
#define   _LATENCY_REPORTING_REMOVED_PIPE_A	REG_BIT(23)
#define   LATENCY_REPORTING_REMOVED(pipe)	_PICK((pipe), \
						      _LATENCY_REPORTING_REMOVED_PIPE_A, \
						      _LATENCY_REPORTING_REMOVED_PIPE_B, \
						      _LATENCY_REPORTING_REMOVED_PIPE_C, \
						      _LATENCY_REPORTING_REMOVED_PIPE_D)
#define   ICL_DELAY_PMRSP			REG_BIT(22)
#define   DISABLE_FLR_SRC			REG_BIT(15)
#define   MASK_WAKEMEM				REG_BIT(13)
#define   DDI_CLOCK_REG_ACCESS			REG_BIT(7)

#define GMD_ID_DISPLAY				_MMIO(0x510a0)
#define   GMD_ID_ARCH_MASK			REG_GENMASK(31, 22)
#define   GMD_ID_RELEASE_MASK			REG_GENMASK(21, 14)
#define   GMD_ID_STEP				REG_GENMASK(5, 0)

/* PCH */

#define SDEISR  _MMIO(0xc4000)
#define SDEIMR  _MMIO(0xc4004)
#define SDEIIR  _MMIO(0xc4008)
#define SDEIER  _MMIO(0xc400c)

/* Icelake PPS_DATA and _ECC DIP Registers.
 * These are available for transcoders B,C and eDP.
 * Adding the _A so as to reuse the _MMIO_TRANS2
 * definition, with which it offsets to the right location.
 */

#define _TRANSA_CHICKEN1	 0xf0060
#define _TRANSB_CHICKEN1	 0xf1060
#define TRANS_CHICKEN1(pipe)	_MMIO_PIPE(pipe, _TRANSA_CHICKEN1, _TRANSB_CHICKEN1)
#define   TRANS_CHICKEN1_HDMIUNIT_GC_DISABLE	REG_BIT(10)
#define   TRANS_CHICKEN1_DP0UNIT_GC_DISABLE	REG_BIT(4)

#define _TRANSA_CHICKEN2	 0xf0064
#define _TRANSB_CHICKEN2	 0xf1064
#define TRANS_CHICKEN2(pipe)	_MMIO_PIPE(pipe, _TRANSA_CHICKEN2, _TRANSB_CHICKEN2)
#define   TRANS_CHICKEN2_TIMING_OVERRIDE		REG_BIT(31)
#define   TRANS_CHICKEN2_FDI_POLARITY_REVERSED		REG_BIT(29)
#define   TRANS_CHICKEN2_FRAME_START_DELAY_MASK		REG_GENMASK(28, 27)
#define   TRANS_CHICKEN2_FRAME_START_DELAY(x)		REG_FIELD_PREP(TRANS_CHICKEN2_FRAME_START_DELAY_MASK, (x)) /* 0-3 */
#define   TRANS_CHICKEN2_DISABLE_DEEP_COLOR_COUNTER	REG_BIT(26)
#define   TRANS_CHICKEN2_DISABLE_DEEP_COLOR_MODESWITCH	REG_BIT(25)

#define SOUTH_CHICKEN1		_MMIO(0xc2000)
#define  FDIA_PHASE_SYNC_SHIFT_OVR	19
#define  FDIA_PHASE_SYNC_SHIFT_EN	18
#define  INVERT_DDIE_HPD			REG_BIT(28)
#define  INVERT_DDID_HPD_MTP			REG_BIT(27)
#define  INVERT_TC4_HPD				REG_BIT(26)
#define  INVERT_TC3_HPD				REG_BIT(25)
#define  INVERT_TC2_HPD				REG_BIT(24)
#define  INVERT_TC1_HPD				REG_BIT(23)
#define  INVERT_DDID_HPD			(1 << 18)
#define  INVERT_DDIC_HPD			(1 << 17)
#define  INVERT_DDIB_HPD			(1 << 16)
#define  INVERT_DDIA_HPD			(1 << 15)
#define  FDI_PHASE_SYNC_OVR(pipe) (1 << (FDIA_PHASE_SYNC_SHIFT_OVR - ((pipe) * 2)))
#define  FDI_PHASE_SYNC_EN(pipe) (1 << (FDIA_PHASE_SYNC_SHIFT_EN - ((pipe) * 2)))
#define  FDI_BC_BIFURCATION_SELECT	(1 << 12)
#define  CHASSIS_CLK_REQ_DURATION_MASK	(0xf << 8)
#define  CHASSIS_CLK_REQ_DURATION(x)	((x) << 8)
#define  SBCLK_RUN_REFCLK_DIS		(1 << 7)
#define  ICP_SECOND_PPS_IO_SELECT	REG_BIT(2)
#define  SPT_PWM_GRANULARITY		(1 << 0)
#define SOUTH_CHICKEN2		_MMIO(0xc2004)
#define  FDI_MPHY_IOSFSB_RESET_STATUS	(1 << 13)
#define  FDI_MPHY_IOSFSB_RESET_CTL	(1 << 12)
#define  LPT_PWM_GRANULARITY		(1 << 5)
#define  DPLS_EDP_PPS_FIX_DIS		(1 << 0)

#define SOUTH_DSPCLK_GATE_D	_MMIO(0xc2020)
#define  PCH_GMBUSUNIT_CLOCK_GATE_DISABLE (1 << 31)
#define  PCH_DPLUNIT_CLOCK_GATE_DISABLE (1 << 30)
#define  PCH_DPLSUNIT_CLOCK_GATE_DISABLE (1 << 29)
#define  PCH_DPMGUNIT_CLOCK_GATE_DISABLE (1 << 15)
#define  PCH_CPUNIT_CLOCK_GATE_DISABLE (1 << 14)
#define  CNP_PWM_CGE_GATING_DISABLE (1 << 13)
#define  PCH_LP_PARTITION_LEVEL_DISABLE  (1 << 12)

#define  VLV_PMWGICZ				_MMIO(0x1300a4)

#define  HSW_EDRAM_CAP				_MMIO(0x120010)
#define    EDRAM_ENABLED			0x1
#define    EDRAM_NUM_BANKS(cap)			(((cap) >> 1) & 0xf)
#define    EDRAM_WAYS_IDX(cap)			(((cap) >> 5) & 0x7)
#define    EDRAM_SETS_IDX(cap)			(((cap) >> 8) & 0x3)

#define GEN6_PCODE_MAILBOX			_MMIO(0x138124)
#define   GEN6_PCODE_READY			(1 << 31)
#define   GEN6_PCODE_MB_PARAM2			REG_GENMASK(23, 16)
#define   GEN6_PCODE_MB_PARAM1			REG_GENMASK(15, 8)
#define   GEN6_PCODE_MB_COMMAND			REG_GENMASK(7, 0)
#define   GEN6_PCODE_ERROR_MASK			0xFF
#define     GEN6_PCODE_SUCCESS			0x0
#define     GEN6_PCODE_ILLEGAL_CMD		0x1
#define     GEN6_PCODE_MIN_FREQ_TABLE_GT_RATIO_OUT_OF_RANGE 0x2
#define     GEN6_PCODE_TIMEOUT			0x3
#define     GEN6_PCODE_UNIMPLEMENTED_CMD	0xFF
#define     GEN7_PCODE_TIMEOUT			0x2
#define     GEN7_PCODE_ILLEGAL_DATA		0x3
#define     GEN11_PCODE_ILLEGAL_SUBCOMMAND	0x4
#define     GEN11_PCODE_LOCKED			0x6
#define     GEN11_PCODE_REJECTED		0x11
#define     GEN7_PCODE_MIN_FREQ_TABLE_GT_RATIO_OUT_OF_RANGE 0x10
#define   GEN6_PCODE_WRITE_RC6VIDS		0x4
#define   GEN6_PCODE_READ_RC6VIDS		0x5
#define     GEN6_ENCODE_RC6_VID(mv)		(((mv) - 245) / 5)
#define     GEN6_DECODE_RC6_VID(vids)		(((vids) * 5) + 245)
#define   BDW_PCODE_DISPLAY_FREQ_CHANGE_REQ	0x18
#define   GEN9_PCODE_READ_MEM_LATENCY		0x6
#define     GEN9_MEM_LATENCY_LEVEL_3_7_MASK	REG_GENMASK(31, 24)
#define     GEN9_MEM_LATENCY_LEVEL_2_6_MASK	REG_GENMASK(23, 16)
#define     GEN9_MEM_LATENCY_LEVEL_1_5_MASK	REG_GENMASK(15, 8)
#define     GEN9_MEM_LATENCY_LEVEL_0_4_MASK	REG_GENMASK(7, 0)
#define   SKL_PCODE_LOAD_HDCP_KEYS		0x5
#define   SKL_PCODE_CDCLK_CONTROL		0x7
#define     SKL_CDCLK_PREPARE_FOR_CHANGE	0x3
#define     SKL_CDCLK_READY_FOR_CHANGE		0x1
#define   GEN6_PCODE_WRITE_MIN_FREQ_TABLE	0x8
#define   GEN6_PCODE_READ_MIN_FREQ_TABLE	0x9
#define   GEN6_READ_OC_PARAMS			0xc
#define   ICL_PCODE_MEM_SUBSYSYSTEM_INFO	0xd
#define     ICL_PCODE_MEM_SS_READ_GLOBAL_INFO	(0x0 << 8)
#define     ICL_PCODE_MEM_SS_READ_QGV_POINT_INFO(point)	(((point) << 16) | (0x1 << 8))
#define     ADL_PCODE_MEM_SS_READ_PSF_GV_INFO	((0) | (0x2 << 8))
#define   DISPLAY_TO_PCODE_CDCLK_MAX		0x28D
#define   DISPLAY_TO_PCODE_VOLTAGE_MASK		REG_GENMASK(1, 0)
#define	  DISPLAY_TO_PCODE_VOLTAGE_MAX		DISPLAY_TO_PCODE_VOLTAGE_MASK
#define   DISPLAY_TO_PCODE_CDCLK_VALID		REG_BIT(27)
#define   DISPLAY_TO_PCODE_PIPE_COUNT_VALID	REG_BIT(31)
#define   DISPLAY_TO_PCODE_CDCLK_MASK		REG_GENMASK(25, 16)
#define   DISPLAY_TO_PCODE_PIPE_COUNT_MASK	REG_GENMASK(30, 28)
#define   DISPLAY_TO_PCODE_CDCLK(x)		REG_FIELD_PREP(DISPLAY_TO_PCODE_CDCLK_MASK, (x))
#define   DISPLAY_TO_PCODE_PIPE_COUNT(x)	REG_FIELD_PREP(DISPLAY_TO_PCODE_PIPE_COUNT_MASK, (x))
#define   DISPLAY_TO_PCODE_VOLTAGE(x)		REG_FIELD_PREP(DISPLAY_TO_PCODE_VOLTAGE_MASK, (x))
#define   DISPLAY_TO_PCODE_UPDATE_MASK(cdclk, num_pipes, voltage_level) \
		((DISPLAY_TO_PCODE_CDCLK(cdclk)) | \
		(DISPLAY_TO_PCODE_PIPE_COUNT(num_pipes)) | \
		(DISPLAY_TO_PCODE_VOLTAGE(voltage_level)))
#define   ICL_PCODE_SAGV_DE_MEM_SS_CONFIG	0xe
#define     ICL_PCODE_REP_QGV_MASK		REG_GENMASK(1, 0)
#define     ICL_PCODE_REP_QGV_SAFE		REG_FIELD_PREP(ICL_PCODE_REP_QGV_MASK, 0)
#define     ICL_PCODE_REP_QGV_POLL		REG_FIELD_PREP(ICL_PCODE_REP_QGV_MASK, 1)
#define     ICL_PCODE_REP_QGV_REJECTED		REG_FIELD_PREP(ICL_PCODE_REP_QGV_MASK, 2)
#define     ADLS_PCODE_REP_PSF_MASK		REG_GENMASK(3, 2)
#define     ADLS_PCODE_REP_PSF_SAFE		REG_FIELD_PREP(ADLS_PCODE_REP_PSF_MASK, 0)
#define     ADLS_PCODE_REP_PSF_POLL		REG_FIELD_PREP(ADLS_PCODE_REP_PSF_MASK, 1)
#define     ADLS_PCODE_REP_PSF_REJECTED		REG_FIELD_PREP(ADLS_PCODE_REP_PSF_MASK, 2)
#define     ICL_PCODE_REQ_QGV_PT_MASK		REG_GENMASK(7, 0)
#define     ICL_PCODE_REQ_QGV_PT(x)		REG_FIELD_PREP(ICL_PCODE_REQ_QGV_PT_MASK, (x))
#define     ADLS_PCODE_REQ_PSF_PT_MASK		REG_GENMASK(10, 8)
#define     ADLS_PCODE_REQ_PSF_PT(x)		REG_FIELD_PREP(ADLS_PCODE_REQ_PSF_PT_MASK, (x))
#define   GEN6_PCODE_READ_D_COMP		0x10
#define   GEN6_PCODE_WRITE_D_COMP		0x11
#define   ICL_PCODE_EXIT_TCCOLD			0x12
#define   HSW_PCODE_DE_WRITE_FREQ_REQ		0x17
#define   DISPLAY_IPS_CONTROL			0x19
#define   TGL_PCODE_TCCOLD			0x26
#define     TGL_PCODE_EXIT_TCCOLD_DATA_L_EXIT_FAILED	REG_BIT(0)
#define     TGL_PCODE_EXIT_TCCOLD_DATA_L_BLOCK_REQ	0
#define     TGL_PCODE_EXIT_TCCOLD_DATA_L_UNBLOCK_REQ	REG_BIT(0)
            /* See also IPS_CTL */
#define     IPS_PCODE_CONTROL			(1 << 30)
#define   HSW_PCODE_DYNAMIC_DUTY_CYCLE_CONTROL	0x1A
#define   GEN9_PCODE_SAGV_CONTROL		0x21
#define     GEN9_SAGV_DISABLE			0x0
#define     GEN9_SAGV_IS_DISABLED		0x1
#define     GEN9_SAGV_ENABLE			0x3
#define   DG1_PCODE_STATUS			0x7E
#define     DG1_UNCORE_GET_INIT_STATUS		0x0
#define     DG1_UNCORE_INIT_STATUS_COMPLETE	0x1
#define   PCODE_POWER_SETUP			0x7C
#define     POWER_SETUP_SUBCOMMAND_READ_I1	0x4
#define     POWER_SETUP_SUBCOMMAND_WRITE_I1	0x5
#define	    POWER_SETUP_I1_WATTS		REG_BIT(31)
#define	    POWER_SETUP_I1_SHIFT		6	/* 10.6 fixed point format */
#define	    POWER_SETUP_I1_DATA_MASK		REG_GENMASK(15, 0)
#define     POWER_SETUP_SUBCOMMAND_G8_ENABLE	0x6
#define GEN12_PCODE_READ_SAGV_BLOCK_TIME_US	0x23
#define   XEHP_PCODE_FREQUENCY_CONFIG		0x6e	/* pvc */
/* XEHP_PCODE_FREQUENCY_CONFIG sub-commands (param1) */
#define     PCODE_MBOX_FC_SC_READ_FUSED_P0	0x0
#define     PCODE_MBOX_FC_SC_READ_FUSED_PN	0x1
/* PCODE_MBOX_DOMAIN_* - mailbox domain IDs */
/*   XEHP_PCODE_FREQUENCY_CONFIG param2 */
#define     PCODE_MBOX_DOMAIN_NONE		0x0
#define     PCODE_MBOX_DOMAIN_MEDIAFF		0x3
#define GEN6_PCODE_DATA				_MMIO(0x138128)
#define   GEN6_PCODE_FREQ_IA_RATIO_SHIFT	8
#define   GEN6_PCODE_FREQ_RING_RATIO_SHIFT	16
#define GEN6_PCODE_DATA1			_MMIO(0x13812C)

#define MTL_PCODE_STOLEN_ACCESS			_MMIO(0x138914)
#define   STOLEN_ACCESS_ALLOWED			0x1

/* IVYBRIDGE DPF */
#define GEN7_L3CDERRST1(slice)		_MMIO(0xB008 + (slice) * 0x200) /* L3CD Error Status 1 */
#define   GEN7_L3CDERRST1_ROW_MASK	(0x7ff << 14)
#define   GEN7_PARITY_ERROR_VALID	(1 << 13)
#define   GEN7_L3CDERRST1_BANK_MASK	(3 << 11)
#define   GEN7_L3CDERRST1_SUBBANK_MASK	(7 << 8)
#define GEN7_PARITY_ERROR_ROW(reg) \
		(((reg) & GEN7_L3CDERRST1_ROW_MASK) >> 14)
#define GEN7_PARITY_ERROR_BANK(reg) \
		(((reg) & GEN7_L3CDERRST1_BANK_MASK) >> 11)
#define GEN7_PARITY_ERROR_SUBBANK(reg) \
		(((reg) & GEN7_L3CDERRST1_SUBBANK_MASK) >> 8)
#define   GEN7_L3CDERRST1_ENABLE	(1 << 7)

/* These are the 4 32-bit write offset registers for each stream
 * output buffer.  It determines the offset from the
 * 3DSTATE_SO_BUFFERs that the next streamed vertex output goes to.
 */
#define GEN7_SO_WRITE_OFFSET(n)		_MMIO(0x5280 + (n) * 4)

#define GEN9_TIMESTAMP_OVERRIDE				_MMIO(0x44074)
#define  GEN9_TIMESTAMP_OVERRIDE_US_COUNTER_DIVIDER_SHIFT	0
#define  GEN9_TIMESTAMP_OVERRIDE_US_COUNTER_DIVIDER_MASK	0x3ff
#define  GEN9_TIMESTAMP_OVERRIDE_US_COUNTER_DENOMINATOR_SHIFT	12
#define  GEN9_TIMESTAMP_OVERRIDE_US_COUNTER_DENOMINATOR_MASK	(0xf << 12)

#define GGC				_MMIO(0x108040)
#define   GMS_MASK			REG_GENMASK(15, 8)
#define   GGMS_MASK			REG_GENMASK(7, 6)

#define GEN6_GSMBASE			_MMIO(0x108100)
#define GEN6_DSMBASE			_MMIO(0x1080C0)
#define   GEN6_BDSM_MASK		REG_GENMASK64(31, 20)
#define   GEN11_BDSM_MASK		REG_GENMASK64(63, 20)

#define XEHP_CLOCK_GATE_DIS		_MMIO(0x101014)
#define   SGSI_SIDECLK_DIS		REG_BIT(17)
#define   SGGI_DIS			REG_BIT(15)
#define   SGR_DIS			REG_BIT(13)

#define PRIMARY_SPI_TRIGGER			_MMIO(0x102040)
#define PRIMARY_SPI_ADDRESS			_MMIO(0x102080)
#define PRIMARY_SPI_REGIONID			_MMIO(0x102084)
#define SPI_STATIC_REGIONS			_MMIO(0x102090)
#define   OPTIONROM_SPI_REGIONID_MASK		REG_GENMASK(7, 0)
#define OROM_OFFSET				_MMIO(0x1020c0)
#define   OROM_OFFSET_MASK			REG_GENMASK(20, 16)

#define MTL_MEM_SS_INFO_GLOBAL			_MMIO(0x45700)
#define   MTL_N_OF_ENABLED_QGV_POINTS_MASK	REG_GENMASK(11, 8)
#define   MTL_N_OF_POPULATED_CH_MASK		REG_GENMASK(7, 4)
#define   MTL_DDR_TYPE_MASK			REG_GENMASK(3, 0)

#define MTL_MEDIA_GSI_BASE		0x380000

// GT Interrupt registers (Gen11/Gen12)
#define GEN11_GFX_MSTR_IRQ_MASK  0x190014


// Example RCS interrupt bits (you can refine according to actual HW docs)
#define RCS_INTR_USER            (1 << 0)
#define RCS_INTR_CTX_SWITCH      (1 << 1)
#define RCS_INTR_FAULT           (1 << 2)
#define RCS_INTR_COMPLETE        (1 << 3)

// Tiger Lake Gen12 RCS base = 0x2C000
#define RCS_BASE = 0x2C000;
#define RCS0_MODE = RCS_BASE + 0xD8;
#define RCS0_RESET_CTRL = RCS_BASE + 0x8;


// Workaround registers for Gen12
#define GEN12_RCS0_INSTDONE_EXT = 0x2090;
#define GEN12_RCS0_EU_ATTN = 0x2094;

#define ELSP0_LO 0x2100
#define ELSP0_HI 0x2104
#define EXECLIST_STATUS_LO 0x2108
#define EXECLIST_STATUS_HI 0x210C


#define EXECLIST_CTRL   0x20C0

#define CSB_ADDR_LO     0x20E0
#define CSB_ADDR_HI     0x20E4

// Basic RCS ring registers (Tiger Lake / Gen11 style offsets you already use)

#define RCS0_IER             0x2604
#define RCS0_IMR             0x260C
#define RCS0_IIR             0x2620
#define RCS0_ICR             0x2624

// General registers — adjust if your mmio view uses other offsets
#define FORCEWAKE_ACK        0x130044

// Execlist / Submission registers (Gen8+ style)
#define EXECLIST_CONTROL     0x21000
#define EXECLIST_STATUS      0x21004
#define EXECLIST_LIST_ADDRESS 0x21008

// Snapshot / debug
#define GT_IIR               0x8200

// Fence page / status page base — we'll allocate a small fence GEM and place a 4 byte fence at offset 0
// No register for fence; we will poll user-space visible memory

// Small helpers
#define RING_CTL_ENABLE      (1u << 0)


// CORRECTED MI packet definitions
#ifndef MI_INSTR
#define MI_INSTR(opcode, flags) (((opcode) << 23) | (flags))
#endif

#ifndef MI_BATCH_BUFFER_START
#define MI_BATCH_BUFFER_START  MI_INSTR(0x31, 1)  // Flag bit 0 for address space
#endif


#ifndef MI_FLUSH_DW
#define MI_FLUSH_DW           MI_INSTR(0x26, 0)
#endif

#ifndef MI_NOOP
#define MI_NOOP               (0 << 23)
#endif




static constexpr uint32_t RING_BASE_LO = 0x2000;
static constexpr uint32_t RING_BASE_HI = 0x2004;
static constexpr uint32_t RING_HEAD    = 0x2010;
static constexpr uint32_t RING_TAIL    = 0x2018;
static constexpr uint32_t RING_CTL     = 0x2020;


// -----------------------------
// Tiger Lake / Gen12 — RCS0 base
// -----------------------------
#define TGL_RCS0_BASE                0x2C000

// Basic ring regs (RCS0 block relative)
#define RCS0_RING_TAIL               (TGL_RCS0_BASE + 0x30)   // tail
#define RCS0_RING_HEAD               (TGL_RCS0_BASE + 0x34)   // head
#define RCS0_RING_START              (TGL_RCS0_BASE + 0x38)   // start/base
#define RCS0_RING_CTL                (TGL_RCS0_BASE + 0x3C)   // ctl
#define RCS0_RING_MODE               (TGL_RCS0_BASE + 0xD8)   // ring mode
#define RCS0_RESET_CTRL              (TGL_RCS0_BASE + 0x1C0)  // reset/control

#define RCS0_GFX_MODE     (0x2C000 + 0xD0)
#define RCS0_GFX_MODE2    (0x2C000 + 0xD4)


// -----------------------------
// Execlist registers (GEN12 TGL fixed offsets)
// These are NOT relative to per-engine small offsets in some docs: use the
// absolute 0x2C29x / 0x2C2Ax values on TGL.
// -----------------------------
#define RCS0_EXECLIST_SUBMITPORT_LO  0x2C290  // ELSP0 LO  (submit port)
#define RCS0_EXECLIST_SUBMITPORT_HI  0x2C294  // ELSP0 HI

#define RCS0_EXECLIST_SQ_CONTENTS    0x2C2A8  // optional

// NOTE: some older docs label these ELSP0_LO / ELSP0_HI etc. Those names map to
// the RCS0_EXECLIST_SUBMITPORT_* above.

// -----------------------------
// Forcewake / GTT / GuC region (common addresses used by drivers)
// -----------------------------
// Request forcewake (engine domain specific; you used 0xA188 previously)
#define FORCEWAKE_REQ                0xA188

// Forcewake ACK (global ack register used in many drivers)
#define FORCEWAKE_ACK                0x130044

// GuC control registers (GEN11+)
#define GEN11_GUC_CTL                0x1C0B0
#define GEN11_GUC_STATUS             0x1C0B4
#define GEN11_GUC_FW_ADDR_LO         0x1C0C4
#define GEN11_GUC_FW_ADDR_HI         0x1C0C8
#define GEN11_GUC_RESET              0x1C0C0

// Misc
#define GTT_FLUSH_REG                0x1082C0   // GTT flush (used in your code)


#define GEN11_FORCEWAKE_RENDER       0xA278 //working 0x0000A188
#define GEN11_FORCEWAKE_RENDER_ACK   0xA27C //working 0x0000A18C




#define GEN11_FORCEWAKE_GT           0x0000A270
#define GEN11_FORCEWAKE_GT_ACK       0x0000A274


#define RCS0_CSB_LO                0x2300
#define RCS0_CSB_HI                0x2304


#define GEN11_GFX_MSTR_IRQ        0x190014

#define RCS_INTR_CTX_SWITCH        (1 << 1)


// =============== GEN12 Tiger Lake CSB registers ===============
#define RCS0_CSB_ADDR_LO      0x22A0
#define RCS0_CSB_ADDR_HI      0x22A4
#define RCS0_CSB_CTRL         0x22A8








// ========================
// GEN12 / Tiger Lake ELSP
// ========================


#define RCS0_ELSP1_LO              0x2258
#define RCS0_ELSP1_HI              0x225C


// Execlist Status
#define RCS0_EXECLIST_STATUS_LO    0x2C20C
#define RCS0_EXECLIST_STATUS_HI    0x2C210


// Execlist Control
#define RCS0_EXECLIST_CONTROL      0x2C228
#define RCS0_CTX_STATUS              0x2234  // Context status

#define GEN12_ELSP_DESC_DWORDS 16


// Arbitration Control
#define RCS0_EXECLIST_ARB_CTL      0x2C22C


#define RCS0_EXECLIST_PREEMPT        0x2510
#define RCS0_EXECLIST_CONTEXT_CONTROL 0x244C

#define GEN6_RC_CONTROL              0xA090    // RC6 control
#define GEN9_PG_ENABLE               0xA210    // Power gating enable
#define RCS0_SCRATCH1                0x24D04    // Scratch register for testing



// MI / PIPE opcodes used in test batch
enum {
    MI_STORE_DWORD_IMM      = (0x20u << 23),
    MI_BATCH_BUFFER_END     = (0x0Au << 23),

    // Global GTT bit for MI / PIPE addresses
    MI_USE_GGTT             = (1u << 22),

    // PIPE_CONTROL (Gen11/12-style)
    PIPE_CONTROL_OPCODE     = (0x7Au << 23),

    // Common PIPE_CONTROL bits (simplified)
    PIPE_CONTROL_CS_STALL        = (1u << 20), // stall before the op
    PIPE_CONTROL_POST_SYNC_WRITE = (1u << 14), // post-sync: write immediate data



#define GEN12_RCS0_RBSTART      0x23C30
#define GEN12_RCS0_RBHEAD       0x23C38
#define GEN12_RCS0_RBTAIL       0x23C3C











};

















#endif // FAKE_IRIS_XE_REG_H
