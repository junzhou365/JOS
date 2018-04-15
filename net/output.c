#include "ns.h"

extern union Nsipc nsipcbuf;

void
output(envid_t ns_envid)
{
	binaryname = "ns_output";

	// LAB 6: Your code here:
	// 	- read a packet from the network server
	//	- send the packet to the device driver
    int r;
    int perm = 0;
    struct jif_pkt *pkt;

    while (1) {
        if ((r = ipc_recv(&ns_envid, &nsipcbuf, &perm)) < 0)
            panic("recv panics");

        if (r != NSREQ_OUTPUT)
            panic("Wrong IPC for output");

        if ((perm & ~PTE_SYSCALL) != 0)
            panic("Perm error for output");

        pkt = (struct jif_pkt *)&nsipcbuf;

        while ((r = sys_send_packets(pkt->jp_data, pkt->jp_len)) < 0)
            ;
    }
}
