#include "ns.h"

extern union Nsipc nsipcbuf;

void
input(envid_t ns_envid)
{
	binaryname = "ns_input";

	// LAB 6: Your code here:
	// 	- read a packet from the device driver
	//	- send it to the network server
	// Hint: When you IPC a page to the network server, it will be
	// reading from it for a while, so don't immediately receive
	// another packet in to the same physical page.

    int r;
    int perm = 0;
    struct jif_pkt *pkt;

    while (1) {
        pkt = (struct jif_pkt *)&nsipcbuf;
        if ((r = sys_page_alloc(0, pkt, PTE_P | PTE_U | PTE_W)) < 0)
            panic("Failed to alloc page in input");

        while ((r = sys_recv_packets(pkt->jp_data, &pkt->jp_len, false /*wait*/)) < 0)
            sys_yield();

        ipc_send(ns_envid, NSREQ_INPUT, &nsipcbuf, PTE_U | PTE_P);
    }
}
