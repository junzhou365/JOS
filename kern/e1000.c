#include <inc/error.h>
#include <inc/memlayout.h>
#include <inc/string.h>
#include <kern/e1000.h>
#include <kern/picirq.h>
#include <kern/pmap.h>

// LAB 6: Your driver code here
// These constants are loosely copied from
// https://pdos.csail.mit.edu/6.828/2017/labs/lab6/e1000_hw.h

#define E1000_CTRL     0x00000  /* Device Control - RW */
#define E1000_CTRL_DUP 0x00004  /* Device Control Duplicate (Shadow) - RW */
#define E1000_STATUS   0x00008  /* Device Status - RO */
/* ~ */
#define E1000_TCTL     0x00400  /* TX Control - RW */
#define E1000_TCTL_EXT 0x00404  /* Extended TX Control - RW */
#define E1000_TIPG     0x00410  /* TX Inter-packet gap -RW */
/* ~ */
#define E1000_RXDCTL   0x02828  /* RX Descriptor Control queue 0 - RW */
/* ~ */
#define E1000_TDBAL    0x03800  /* TX Descriptor Base Address Low - RW */
#define E1000_TDBAH    0x03804  /* TX Descriptor Base Address High - RW */
#define E1000_TDLEN    0x03808  /* TX Descriptor Length - RW */
#define E1000_TDH      0x03810  /* TX Descriptor Head - RW */
#define E1000_TDT      0x03818  /* TX Descripotr Tail - RW */

/* Transmit Control */
#define E1000_TCTL_EN     0x00000002    /* enable tx */
#define E1000_TCTL_PSP    0x00000008    /* pad short packets */
#define E1000_TCTL_CT     0x00000100    /* collision threshold */
#define E1000_TCTL_COLD   0x00040000    /* collision distance */

/* Transmit Descriptor bit definitions */
#define E1000_TXD_CMD_EOP    0x01 /* End of Packet */
#define E1000_TXD_CMD_RS     0x08 /* Report Status */
#define E1000_TXD_STAT_DD    0x00000001 /* Descriptor Done */

/* -- Receive -- */
#define E1000_RCTL     0x00100  /* RX Control - RW */
#define E1000_RSRPD    0x02C00  /* RX Small Packet Detect - RW */

/* Receive Address */
#define E1000_RAH_AV  0x80000000        /* Receive descriptor valid */

/* these buffer sizes are valid if E1000_RCTL_BSEX is 0 */
#define E1000_RCTL_EN             0x00000002    /* enable */
#define E1000_RCTL_UPE            0x00000008    /* unicast promiscuous enable */
#define E1000_RCTL_SZ_2048        0x00000000    /* rx buffer size 2048 */
#define E1000_RCTL_RDMTS_EIGTH    0x00000200    /* rx desc min threshold size */
#define E1000_RCTL_SECRC          0x04000000    /* Strip Ethernet CRC */

#define E1000_RDBAL    0x02800  /* RX Descriptor Base Address Low - RW */
#define E1000_RDBAH    0x02804  /* RX Descriptor Base Address High - RW */
#define E1000_RDLEN    0x02808  /* RX Descriptor Length - RW */
#define E1000_RDH      0x02810  /* RX Descriptor Head - RW */
#define E1000_RDT      0x02818  /* RX Descriptor Tail - RW */

#define E1000_RXD_STAT_DD       0x01    /* Descriptor Done */
#define E1000_RXD_STAT_EOP      0x02    /* End of Packet */

#define E1000_RA       0x05400  /* Receive Address - RW Array */
#define E1000_RAL0     E1000_RA
#define E1000_RAH0     (E1000_RAL0 + 4)

/* -- Interrupt Read -- */
#define E1000_ICR      0x000C0  /* Interrupt Cause Read - R/clr */

#define E1000_ICR_TXDW          0x00000001 /* Transmit desc written back */
#define E1000_ICR_RXSEQ         0x00000008 /* rx sequence error */
#define E1000_ICR_RXDMT0        0x00000010 /* rx desc min. threshold (0) */
#define E1000_ICR_RXO           0x00000040 /* rx overrun */
#define E1000_ICR_RXT0          0x00000080 /* rx timer intr (ring 0) */
#define E1000_ICR_SRPD          0x00010000
/* -- Interrupt Set -- */
#define E1000_ICS      0x000C8  /* Interrupt Cause Set - WO */

#define E1000_ICS_TXDW      E1000_ICR_TXDW      /* Transmit desc written back */
#define E1000_ICS_RXDMT0    E1000_ICR_RXDMT0    /* rx desc min. threshold */
#define E1000_ICS_RXO       E1000_ICR_RXO       /* rx overrun */
#define E1000_ICS_SRPD      E1000_ICR_SRPD
#define E1000_ICS_RXT0      E1000_ICR_RXT0      /* rx timer intr */

/* -- Mask -- */
#define E1000_IMS      0x000D0  /* Interrupt Mask Set - RW */

#define E1000_IMS_TXDW      E1000_ICR_TXDW      /* Transmit desc written back */
#define E1000_IMS_RXDMT0    E1000_ICR_RXDMT0    /* rx desc min. threshold */
#define E1000_IMS_RXO       E1000_ICR_RXO       /* rx overrun */
#define E1000_IMS_SRPD      E1000_ICR_SRPD
#define E1000_IMS_RXSEQ     E1000_ICR_RXSEQ     /* rx sequence error */
#define E1000_IMS_RXT0      E1000_ICR_RXT0      /* rx timer intr */

/* Mask Clear */
#define E1000_IMC      0x000D8  /* Interrupt Mask Clear - WO */

#define E1000_IMC_RXT0      E1000_ICR_RXT0      /* rx timer intr */
#define E1000_IMC_TXDW      E1000_ICR_TXDW      /* Transmit desc written back */
#define E1000_IMC_SRPD      E1000_ICR_SRPD

#define RADDR(x) ((x) / 4)
// number of descriptors. N * 16 % 128 must be 0
#define TX_N 16 // 16 * 16 = 256 < 4K
#define R_N 128 // 16 * 128 = 2048 < 4K

#define TX_ARRAY (MMIOLIM + PGSIZE)
#define TX_BASE (TX_ARRAY + PGSIZE)
#define R_ARRAY (TX_BASE + (PGSIZE * TX_N))
#define R_BASE (R_ARRAY + PGSIZE)

struct Tx_Desc
{
	uint64_t addr;
	uint16_t length;
	uint8_t cso;
	uint8_t cmd;
	uint8_t status;
	uint8_t css;
	uint16_t special;
};

struct Tx_Desc *tx_descs = (struct Tx_Desc *)TX_ARRAY;

struct R_Desc
{
    uint64_t addr;
    uint16_t length;
    uint16_t checksum;
    uint8_t status;
    uint8_t errors;
    uint16_t special;
};

struct R_Desc *r_descs = (struct R_Desc *)R_ARRAY;

volatile uint32_t *e1000_addr;

envid_t sender_id; // we have only one sender and one receiver env
envid_t receiver_id;

int
attach_e1000(struct pci_func *pcif)
{
    int ret;

    pci_func_enable(pcif);

    uint32_t base, size;
    base = pcif->reg_base[0];
    size = pcif->reg_size[0];
    uint8_t irq_line = pcif->irq_line;
    irq_setmask_8259A(irq_mask_8259A & ~(1<<irq_line));

    e1000_addr = mmio_map_region(base, size);
    memset((void *)base, 0, size);
    // see lab 6 for the number that indicates the status is ok.
    assert(e1000_addr[RADDR(E1000_STATUS)] == 0x80080783);

    // initialize the transmission array
    assert(page_lookup(kern_pgdir, tx_descs, NULL) == NULL);
    struct PageInfo *page;
    page = page_alloc(ALLOC_ZERO);
    if (page == NULL ||
        (ret = page_insert(kern_pgdir, page, tx_descs, PTE_P | PTE_W) < 0))
        panic("Cannot allocate page for tx array");

    e1000_addr[RADDR(E1000_TDBAL)] = page2pa(page);

    for (int i = 0; i < TX_N; i++) {
        page = page_alloc(ALLOC_ZERO);
        if (page == NULL ||
            (ret = page_insert(kern_pgdir, page, (void *)(TX_BASE + PGSIZE * i), PTE_P | PTE_W) < 0))
            panic("Cannot allocate page for tx");

        tx_descs[i].addr = page2pa(page);
        tx_descs[i].cmd = E1000_TXD_CMD_RS;
        tx_descs[i].status = E1000_TXD_STAT_DD;
    }

    e1000_addr[RADDR(E1000_TDLEN)] = sizeof(struct Tx_Desc) * TX_N;

    e1000_addr[RADDR(E1000_TCTL)] |= E1000_TCTL_EN;
    e1000_addr[RADDR(E1000_TCTL)] |= E1000_TCTL_PSP;
    e1000_addr[RADDR(E1000_TCTL)] |= E1000_TCTL_CT;
    e1000_addr[RADDR(E1000_TCTL)] |= E1000_TCTL_COLD;

    // See table 13-77
    e1000_addr[RADDR(E1000_TIPG)] = 10 + (8 << 10) + (6 << 20);

    /* initialize receive */

    // QEMU's default MAC address of 52:54:00:12:34:56. From lab6
    e1000_addr[RADDR(E1000_RAL0)] = 0x12005452;
    e1000_addr[RADDR(E1000_RAH0)] = 0x5634 | E1000_RAH_AV;

    // initialize the receive array
    assert(page_lookup(kern_pgdir, r_descs, NULL) == NULL);
    struct PageInfo *r_page;
    r_page = page_alloc(ALLOC_ZERO);
    if (r_page == NULL ||
        (ret = page_insert(kern_pgdir, r_page, r_descs, PTE_P | PTE_W) < 0))
        panic("Cannot allocate page for r_array");

    e1000_addr[RADDR(E1000_RDBAL)] = page2pa(r_page);

    for (int i = 0; i < R_N; i++) {
        r_page = page_alloc(ALLOC_ZERO);
        if (r_page == NULL ||
            (ret = page_insert(kern_pgdir, r_page,
                (void *)(R_BASE + PGSIZE * i), PTE_P | PTE_W) < 0))
            panic("Cannot allocate page for r");

        r_descs[i].addr = page2pa(r_page);
    }
    size_t r_len = sizeof(struct R_Desc) * R_N;
    e1000_addr[RADDR(E1000_RDLEN)] = r_len;
    e1000_addr[RADDR(E1000_RDH)] = 0;
    e1000_addr[RADDR(E1000_RDT)] = R_N-1;

    /*e1000_addr[RADDR(E1000_RCTL)] |=*/
        /*E1000_RCTL_SZ_2048 | E1000_RCTL_SECRC | E1000_RCTL_RDMTS_EIGTH | E1000_RCTL_UPE;*/
    e1000_addr[RADDR(E1000_RCTL)] |= E1000_RCTL_SZ_2048 | E1000_RCTL_SECRC;
    e1000_addr[RADDR(E1000_RCTL)] |= E1000_RCTL_EN;

    e1000_addr[RADDR(E1000_RSRPD)] = R_N * 16;
    e1000_addr[RADDR(E1000_RXDCTL)] = 1 << 16;
    e1000_addr[RADDR(E1000_IMS)] |= E1000_IMS_RXT0 | E1000_IMS_SRPD;

    return 0;
}

int
transmit_packets(char *data, int len, envid_t id)
{
    uint32_t tail_index = e1000_addr[RADDR(E1000_TDT)];
    assert(tail_index < TX_N && tail_index >= 0);

    struct Tx_Desc *tail = &tx_descs[tail_index];
    bool is_dd_set = (tail->status & E1000_TXD_STAT_DD) != 0;

    if (is_dd_set) {
        cprintf("*** ==== *** sending\n");
        // If we assert this intr after setting another desc, there will be
        // another hard intr
        e1000_addr[RADDR(E1000_ICR)] |= E1000_ICR_TXDW;
        memcpy((uint32_t *)(TX_BASE + PGSIZE * tail_index), data, len);
        tail->cmd |= (E1000_TXD_CMD_RS | E1000_TXD_CMD_EOP);
        tail->length = len;
        tail->status = 0;
        e1000_addr[RADDR(E1000_TDT)] = (tail_index + 1) % TX_N;
    } else {
        // let caller sleep
        cprintf("*** ==== *** send sleeps\n");
        /*e1000_addr[RADDR(E1000_IMS)] |= E1000_IMS_TXDW;*/
        sender_id = id;
        return -E_NET_TRAN_QUEUE_FULL;
    }

    cprintf("*** ==== *** send done\n");
    return 0;
}

static void
print_packet(char *data, int len)
{
    cprintf("The <*received*> packet len is: %d, content is:", len);
    for (int i = 0; i < len; i++)
        cprintf("%x", data[i]);
    cprintf("\n");
}

int
receive_packets(char *data, int *len, envid_t id, bool wait)
{
    uint32_t tail_index = e1000_addr[RADDR(E1000_RDT)];
    uint32_t head_index = e1000_addr[RADDR(E1000_RDH)];
    uint32_t next = (tail_index + 1) % R_N;

    struct R_Desc *tail = &r_descs[next];
    bool is_dd_set = (tail->status & E1000_RXD_STAT_DD) != 0;
    bool is_eop = (tail->status & E1000_RXD_STAT_EOP) != 0;

    uint32_t addr = R_BASE + PGSIZE * next;
    /*cprintf("head: %d, tail: %d\n", head_index, tail_index);*/

    if (is_dd_set && is_eop) {
        /*cprintf("*** ==== *** receiving\n");*/
        /*e1000_addr[RADDR(E1000_ICR)] |= E1000_ICR_RXT0;*/
        memcpy(data, (char *)addr, tail->length);
        *len = tail->length;
        tail->status = 0; // zero out according to spec
        e1000_addr[RADDR(E1000_RDT)] = next;
    } else if(is_dd_set && !is_eop) {
        panic("EOP not set for incoming packets");
    } else {
        if (wait) {
            // let caller sleep
            cprintf("*** ==== *** recv sleeps\n");
            /*e1000_addr[RADDR(E1000_IMS)] |= E1000_IMS_RXT0;*/
            e1000_addr[RADDR(E1000_ICS)] |= E1000_ICS_RXT0;
            receiver_id = id;
        }
        return -E_NET_RECV_QUEUE_EMPTY;
    }

    cprintf("*** ==== *** recv done\n");
    return 0;
}

// Kind of awkward place
void
network_intr()
{
    uint32_t tail_index = e1000_addr[RADDR(E1000_RDT)];
    uint32_t head_index = e1000_addr[RADDR(E1000_RDH)];
    tail_index %= R_N;

    uint32_t icr = e1000_addr[RADDR(E1000_ICR)];
    bool is_write = (icr & E1000_ICR_TXDW) &&
        (e1000_addr[RADDR(E1000_IMS)] & E1000_IMS_TXDW);
    bool is_read = (icr & E1000_ICR_RXT0) &&
        (e1000_addr[RADDR(E1000_IMS)] & E1000_IMS_RXT0);
    cprintf("*** === *** New intr: icr: %x, Read: %d, Write: %d, \n",
        icr, is_read, is_write);

    int r;
    struct Env *e = NULL;
    if (is_read && !is_write) {
        envid2env(receiver_id, &e, false);
        e1000_addr[RADDR(E1000_ICR)] = E1000_ICR_RXT0;
        /*e1000_addr[RADDR(E1000_IMC)] = E1000_IMC_RXT0;*/
    } else if (is_write && !is_read) {
        envid2env(sender_id, &e, false);
        e1000_addr[RADDR(E1000_ICR)] = E1000_ICR_TXDW;
        /*e1000_addr[RADDR(E1000_IMC)] = E1000_IMC_TXDW;*/
    } else {
        panic("Bad interrupt");
    }

    if (e == NULL || e->env_net_intr_handler == 0)
        return;
    e->env_net_intr_handler(is_read, e->env_id);
}

