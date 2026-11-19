#ifndef HCI_ADV_H
#define HCI_ADV_H
#include <stddef.h>
#include <stdint.h>
typedef void (*hci_adv_callback)(const uint8_t raw[24], int rssi, void *data);
/* H4 LE legacy advertising event; target address is Bluetooth wire order.
 * Returns matched payload count, zero for unrelated events, -1 if malformed. */
int hci_adv_parse(const uint8_t *packet, size_t size, const uint8_t target[6],
                  hci_adv_callback callback, void *data);
#endif
