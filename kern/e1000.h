#ifndef JOS_KERN_E1000_H
#define JOS_KERN_E1000_H

#include <kern/env.h>
#include <kern/pci.h>

int attach_e1000(struct pci_func *pcif);
void network_intr();
int transmit_packets(char *, int, envid_t);
int receive_packets(char *, int*, envid_t, bool);

#endif	// JOS_KERN_E1000_H
