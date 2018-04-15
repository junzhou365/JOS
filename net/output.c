#include "ns.h"

extern union Nsipc nsipcbuf;

static void
print_packet(char *data, int len)
{
    cprintf("The sent packet len is: %d, content is:", len);
    for (int i = 0; i < len; i++)
        cprintf("%x", data[i]);
    cprintf("\n");
}

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

        print_packet(pkt->jp_data, pkt->jp_len);

        while ((r = sys_send_packets(pkt->jp_data, pkt->jp_len)) < 0)
            ;
    }
}
