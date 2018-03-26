#include <inc/error.h>
#include <inc/memlayout.h>
#include <inc/string.h>
#include <kern/e1000.h>
#include <kern/pmap.h>

// LAB 6: Your driver code here
#define E1000_CTRL     0x00000  /* Device Control - RW */
#define E1000_CTRL_DUP 0x00004  /* Device Control Duplicate (Shadow) - RW */
#define E1000_STATUS   0x00008  /* Device Status - RO */
/* ~ */
#define E1000_TCTL     0x00400  /* TX Control - RW */
#define E1000_TCTL_EXT 0x00404  /* Extended TX Control - RW */
#define E1000_TIPG     0x00410  /* TX Inter-packet gap -RW */
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

#define E1000_TCTL_EN_MASK     0x00000002    /* enable tx */
#define E1000_TCTL_PSP_MASK    0x00000008    /* pad short packets */
#define E1000_TCTL_CT_MASK     0x00000ff0    /* collision threshold */
#define E1000_TCTL_COLD_MASK   0x003ff000    /* collision distance */

/* Transmit Descriptor bit definitions */
#define E1000_TXD_CMD_EOP    0x01 /* End of Packet */
#define E1000_TXD_CMD_RS     0x08 /* Report Status */
#define E1000_TXD_STAT_DD    0x00000001 /* Descriptor Done */

#define RADDR(x) ((x) / 4)
#define TX_N 16 // 16 * 16 = 256 < 4K

#define TX_ARRAY (MMIOLIM + PGSIZE)
#define TX_BASE (TX_ARRAY + PGSIZE)

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

volatile uint32_t *e1000_addr;

int
attach_e1000(struct pci_func *pcif)
{
    int ret;

    pci_func_enable(pcif);

    uint32_t base, size;
    base = pcif->reg_base[0];
    size = pcif->reg_size[0];
    e1000_addr = mmio_map_region(base, size);
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
    }

    /*e1000_addr[RADDR(E1000_TDT)] = 1; // test only*/
    e1000_addr[RADDR(E1000_TDLEN)] = sizeof(struct Tx_Desc) * TX_N;

    e1000_addr[RADDR(E1000_TCTL)] |= (E1000_TCTL_EN & E1000_TCTL_EN_MASK); 
    e1000_addr[RADDR(E1000_TCTL)] |= (E1000_TCTL_PSP & E1000_TCTL_PSP_MASK);
    e1000_addr[RADDR(E1000_TCTL)] |= (E1000_TCTL_CT & E1000_TCTL_CT_MASK);
    e1000_addr[RADDR(E1000_TCTL)] |= (E1000_TCTL_COLD & E1000_TCTL_COLD_MASK);

    // See table 13-77
    e1000_addr[RADDR(E1000_TIPG)] = 10 + (8 << 10) + (6 << 20);

    return 0;
}

int
transmit_packets(char *data, int len)
{
    uint32_t tail_index = e1000_addr[RADDR(E1000_TDT)];
    assert(tail_index < TX_N && tail_index >= 0);

    struct Tx_Desc *tail = &tx_descs[tail_index];
    bool is_rs_set = (tail->cmd & E1000_TXD_CMD_RS) != 0;
    bool is_dd_set = (tail->status & E1000_TXD_STAT_DD) != 0;

    /*cprintf("The packet is:");*/
    /*for (int i = 0; i < len; i++)*/
        /*cprintf("%x", data[i]);*/
    /*cprintf("\n");*/
    if (!is_rs_set || is_dd_set) {
        memcpy((uint32_t *)(TX_BASE + PGSIZE * tail_index), data, len);
        tail->cmd |= (E1000_TXD_CMD_RS | E1000_TXD_CMD_EOP);
        tail->length = len;
        tail->status &= ~E1000_TXD_STAT_DD;
        e1000_addr[RADDR(E1000_TDT)] = (tail_index + 1) % TX_N;
    } else {
        // let caller wait
        return -E_QUEUE_FULL;
    }

    return 0;
}
