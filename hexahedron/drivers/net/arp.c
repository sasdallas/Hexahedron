/**
 * @file hexahedron/drivers/net/arp.c
 * @brief Address Resolution Protocol handler
 * 
 * @todo Cache flushing
 * @todo Support for multiple ARP protocols in ptype, currently only IPv4
 * 
 * @copyright
 * This file is part of the Hexahedron kernel, which is part of reduceOS.
 * It is released under the terms of the BSD 3-clause license.
 * Please see the LICENSE file in the main repository for more details.
 * 
 * Copyright (C) 2024 Samuel Stuart
 */

#include <kernel/drivers/net/arp.h>
#include <kernel/drivers/net/ipv4.h>
#include <kernel/task/process.h>
#include <kernel/drivers/clock.h>
#include <kernel/misc/spinlock.h>
#include <kernel/mem/alloc.h>
#include <structs/hashmap.h>
#include <kernel/debug.h>
#include <kernel/panic.h>
#include <arpa/inet.h>
#include <errno.h>

/* ARP table - consists of arp_table_entry_t structures */
hashmap_t *arp_map = NULL;

/* ARP table lock */
spinlock_t arp_lock = { 0 };

/* Log method */
#define LOG(status, ...) dprintf_module(status, "NETWORK:ARP", __VA_ARGS__)

/* Log NIC */
#define LOG_NIC(status, nn, ...) LOG(status, "[NIC:%s] ", NIC(nn)->name); dprintf(NOHEADER, __VA_ARGS__)


/**
 * @brief Get an entry from the cache table
 * @param address The requested address
 * @returns A table entry or NULL
 */
arp_table_entry_t *arp_get_entry(in_addr_t address) {
    if (!arp_map) return NULL;

    spinlock_acquire(&arp_lock);
    arp_table_entry_t *entry =  (arp_table_entry_t*)hashmap_get(arp_map, (void*)(uintptr_t)address);
    spinlock_release(&arp_lock);

    return entry;
}

/**
 * @brief Manually add an entry to the cache table
 * @param address The address of the entry
 * @param mac The MAC the entry resolves to
 * @param type The type of the entry, e.g. @c ARP_TYPE_ETHERNET
 * @param nic The NIC the entry corresponds to
 * @returns 0 on success
 */
int arp_add_entry(in_addr_t address, uint8_t *mac, int type, fs_node_t *nic) {
    if (!nic || !mac) return 1;

    // First allocate the entry
    arp_table_entry_t *entry = kmalloc(sizeof(arp_table_entry_t));
    entry->address = address;
    memcpy(entry->hwmac, mac, 6);
    entry->hwtype = type;
    entry->nic = nic;

    spinlock_acquire(&arp_lock);
    hashmap_set(arp_map, (void*)(uintptr_t)address, (void*)entry);
    spinlock_release(&arp_lock);

    return 0;
}

/**
 * @brief Remove an entry from the cache table
 * @param address The address to remove
 * @returns 0 on success
 * 
 * @warning The entry is freed upon removal
 */
int arp_remove_entry(in_addr_t address) {
    if (!arp_map) return 1;
    
    spinlock_acquire(&arp_lock);
    arp_table_entry_t *entry = (arp_table_entry_t*)hashmap_get(arp_map, (void*)(uintptr_t)address);
    if (!entry) return 1;

    // Remove & free
    hashmap_remove(arp_map, (void*)(uintptr_t)address);
    kfree(entry);

    spinlock_release(&arp_lock);
    return 0;
}

/**
 * @brief Request to search for an IP address (non-blocking)
 * @param nic The NIC to search on
 * @param address The IP address of the target
 * 
 * @returns 0 on successful send
 */
int arp_request(fs_node_t *node, in_addr_t address) {
    if (!node || !NIC(node)) return 1;
    if (!arp_map) return 1;
 
    nic_t *nic = NIC(node);
 
    LOG_NIC(DEBUG, node, " ARP: Request to find address %s %08x\n", inet_ntoa((struct in_addr){ .s_addr = address }), address);

    // Construct an ARP pakcet
    arp_packet_t *packet = kmalloc(sizeof(arp_packet_t));
    memset(packet, 0, sizeof(arp_packet_t));

    packet->hlen = 6;                   // MAC
    packet->plen = sizeof(in_addr_t);   // IP

    packet->oper = htons(ARP_OPERATION_REQUEST);
    packet->ptype = htons(IPV4_PACKET_TYPE);    // TODO
    packet->htype = htons(ARP_HTYPE_ETHERNET);  // TODO

    // Setup TPA, SHA, and (maybe) SPA
    packet->tpa = address;
    memcpy(packet->sha, nic->mac, 6);
    if (nic->ipv4_address) packet->spa = nic->ipv4_address;

    // Send the packet!
    ethernet_send(node, (void*)packet, ARP_PACKET_TYPE, ETHERNET_BROADCAST_MAC, sizeof(arp_packet_t));

    // Free packet
    kfree(packet);

    return 0;
}

/**
 * @brief Request to search for an IP address (blocking)
 * @param nic The NIC to search on
 * @param address The IP address of the target
 * 
 * @returns 0 on success. Timeout, by default, is 20s
 */
int arp_search(fs_node_t *nic, in_addr_t address) {
    if (arp_request(nic, address)) return 1;

    // TODO: Process blocking
    if (current_cpu->current_process) kernel_panic_extended(UNSUPPORTED_FUNCTION_ERROR, "arp", "*** Cannot block yet\n");

    // Sleep
    clock_sleep(500);
    if (arp_get_entry(address) == NULL) {
        // Keep sleeping
        int timeout = 19500;
        while (timeout) {
            clock_sleep(500);
            timeout -= 500;
            if (arp_get_entry(address)) return 0;
        }

        if (timeout <= 0) {
            LOG_NIC(WARN, nic, " ARP: Timed out, address not found\n");
            return 1; // Error :(
        }
    }

    // Success! Got the entry :)
    return 0;
}

/**
 * @brief Send a reply packet
 * @param packet The previous packet received
 * @param nic_node The NIC node to send on
 */
static void arp_reply(arp_packet_t *packet, fs_node_t *nic_node) {
    // Allocate a response packet
    arp_packet_t *resp = kmalloc(sizeof(arp_packet_t));
    memset(resp, 0, sizeof(arp_packet_t));

    resp->hlen = 6;                   // MAC
    resp->plen = sizeof(in_addr_t);   // IP

    resp->oper = htons(ARP_OPERATION_REPLY);
    resp->ptype = htons(IPV4_PACKET_TYPE);    // TODO
    resp->htype = htons(ARP_HTYPE_ETHERNET);  // TODO

    // SHA and THA
    memcpy(resp->sha, NIC(nic_node)->mac, 6);
    memcpy(resp->tha, packet->sha, 6);
    
    // SPA and TPA
    resp->spa = NIC(nic_node)->ipv4_address;
    resp->tpa = packet->spa;

    // Send the packet
    ethernet_send(nic_node, (void*)resp, ARP_PACKET_TYPE, packet->sha, sizeof(arp_packet_t));
    kfree(resp);
}

/**
 * @brief   Hack stolen from ToaruOS. @c inet_ntoa seems to not like being called, so while I work on that this is a TEMPORARY solution
 *          Sorry :(
 */
static void ip_ntoa(const uint32_t src_addr, char * out) {
	snprintf(out, 16, "%d.%d.%d.%d",
		(src_addr & 0xFF000000) >> 24,
		(src_addr & 0xFF0000) >> 16,
		(src_addr & 0xFF00) >> 8,
		(src_addr & 0xFF));
}

/**
 * @brief Handle ARP packet
 */
int arp_handle_packet(void *frame, fs_node_t *nic_node, size_t size) {
    arp_packet_t *packet = (arp_packet_t*)frame;
    LOG_NIC(DEBUG, nic_node, " ARP: htype=%04x ptype=%04x op=%04x hlen=%d plen=%d\n", ntohs(packet->htype), ntohs(packet->ptype), ntohs(packet->oper), packet->hlen, packet->plen);

    nic_t *nic = NIC(nic_node);

    // What do they want from us?
    if (ntohs(packet->ptype) == IPV4_PACKET_TYPE) {
        // Check the operation requested
        if (ntohs(packet->oper) == ARP_OPERATION_REQUEST) {
            // They're looking for someone. Who?
			char tpa[17];
			ip_ntoa(ntohl(packet->tpa), tpa);

            char spa[17];
            ip_ntoa(ntohl(packet->spa), spa);

            LOG_NIC(DEBUG, nic_node, " ARP: Request from " MAC_FMT " (IP %s) for IP %s\n", spa, MAC(packet->sha), tpa);

            // Cache them
            arp_add_entry((in_addr_t)packet->spa, packet->sha, ARP_TYPE_ETHERNET, nic_node);

            // Ok, are they looking for us?
            if (nic->ipv4_address && packet->tpa == nic->ipv4_address) {
                // Yes, they are. Construct a response packet and send it back
                LOG_NIC(DEBUG, nic_node, " ARP: Request from " MAC_FMT " (IP: %s) with us (" MAC_FMT ", IP %s)\n", MAC(packet->sha), spa, MAC(nic->mac), inet_ntoa((struct in_addr){ .s_addr = nic->ipv4_address } ));
                arp_reply(packet, nic_node);
            }
        } else {
            char spa[17];
            ip_ntoa(ntohl(packet->spa), spa);
            LOG_NIC(DEBUG, nic_node, " ARP: Response from " MAC_FMT " to show they are IP %s\n", MAC(packet->sha), spa);

            // Cache!
            arp_add_entry((in_addr_t)packet->spa, packet->sha, ARP_TYPE_ETHERNET, nic_node);
        }
    } else {
        LOG_NIC(DEBUG, nic_node, " ARP: Invalid protocol type %04x\n", ntohs(packet->ptype));
    }

    return 0;
}

/**
 * @brief Initialize the ARP system
 */
void arp_init() {
    arp_map = hashmap_create_int("arp route map", 20);
    ethernet_registerHandler(ARP_PACKET_TYPE, arp_handle_packet);
}