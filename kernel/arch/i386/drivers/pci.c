/**
 * Code for the Peripheral Component Interconnect.
 * 
 * Refer to:
 * https://wiki.osdev.org/PCI
 * https://www.youtube.com/watch?v=GE7iO2vlLD4
 * 
 * @author Samuel Pires
*/

#include <kernel/arch/i386/drivers/pci.h>

#include <kernel/system.h>
#include <kernel/mm/mm.h>
#include <kernel/arch/i386/io.h>


#define CONFIG_ADDRESS 	0xCF8
#define CONFIG_DATA		0xCFC

#define FORM_CONFIG_ADDRESS(bus, device, function, offset) \
	(1 << 31) | \
	(((bus) & 0xFF) << 16) | \
	(((device) & 0xFF) << 11) | \
	(((function) & 0xFF) << 8) | \
	((offset) & 0xFC)


list_t connected_devices_list;


static uint16_t pci_config_read(uint8_t bus, uint8_t device, uint8_t func, uint8_t offset);
static void pci_config_write(uint8_t bus, uint8_t device, uint8_t func, uint8_t offset, uint32_t data) __attribute__((unused));

static void detect_connected_devices(void);
static pci_device_descriptor_t* create_device_descriptor(uint8_t bus, uint8_t device, uint8_t function);



/* Global Functions */

list_t* pci_get_connected_devices(void)
{
	if (LIST_IS_NULL(connected_devices_list))
		detect_connected_devices();

	return &connected_devices_list;
}



/* Helper Functions */

/**
 * Reads configuration data from a PCI configuration address.
 * 
 * @param bus the bus number
 * @param device the device number
 * @param function the function number
 * @param offest the register offset
 * 
 * @return the data read from the speficied PCI configuration address
*/
static uint16_t pci_config_read(uint8_t bus, uint8_t device, uint8_t function, uint8_t offset)
{
    uint32_t address = FORM_CONFIG_ADDRESS(bus, device, function, offset);
    outd(0xCF8, address);
    return (uint16_t) (ind(0xCFC) >> (8 * (offset % 4)));
}

/**
 * Sends configuration data to a PCI configuration address.
 * 
 * @param bus the bus number
 * @param device the device number
 * @param func the function number
 * @param offest the register offset
 * @param data the data to send
*/
static void pci_config_write(uint8_t bus, uint8_t device, uint8_t function, uint8_t offset, uint32_t data)
{
    uint32_t address = FORM_CONFIG_ADDRESS(bus, device, function, offset);
    outd(0xCF8, address);
    outd(0xCFC, data);
}


#define DEVICE_IS_CONNECTED(bus, device)			(pci_config_read(bus, device, 0, 0) != 0xFFFF)
#define DEVICE_HAS_FUNCTION(bus, device, function)	(pci_config_read(bus, device, function, 0) != 0xFFFF)
#define IS_MULTIFUNCTION_DEVICE(bus, device)		(pci_config_read(bus, device, 0, 0x0E) & 0x80)

/**
 * Initializes pci_device_descriptor_cache and connected_devices_list,
 * detects the devices connected to the PCI, and adds them to the list.
*/
static void detect_connected_devices(void)
{
	ASSERT(LIST_IS_NULL(connected_devices_list));

	/* Initialize list and cache */
	LIST_INIT(connected_devices_list);


	/* Loop through each possible device ID to find the ones connected */
	for (int bus = 0; bus < 8; bus++) {
        for (int device = 0; device < 32; device++)
		{
			/* Ignore non-connected devices */
			if (!DEVICE_IS_CONNECTED(bus, device))
				continue;

            int numFunctions = IS_MULTIFUNCTION_DEVICE(bus, device) ? 8 : 1;
            for (int function = 0; function < numFunctions; function++)
			{
				/* Ignore non-existent functions */
				if (!DEVICE_HAS_FUNCTION(bus, device, function))
					continue;

				pci_device_descriptor_t* pdd = create_device_descriptor(bus, device, function);
				list_add_last(&connected_devices_list, &pdd->list);
			}
		}
	}
}

/**
 * Allocates and initializes a device descriptor.
 * 
 * This assumes the device is connected to the PCI, otherwise it will contain
 * invalid data (all 1s).
 * 
 * @param bus the bus number
 * @param device the device number
 * @param function the function number
 * 
 * @return the device descriptor
*/
static pci_device_descriptor_t* create_device_descriptor(uint8_t bus, uint8_t device, uint8_t function)
{
	pci_device_descriptor_t* pdd = kmalloc(sizeof(pci_device_descriptor_t));

    pdd->bus = bus;
    pdd->device = device;
    pdd->function = function;

    pdd->vendor_id = pci_config_read(bus, device, function, 0x00);
    pdd->device_id = pci_config_read(bus, device, function, 0x02);

    pdd->class_id = pci_config_read(bus, device, function, 0x0B);
    pdd->subclass_id = pci_config_read(bus, device, function, 0x0A);
    pdd->interface_id = pci_config_read(bus, device, function, 0x09);

    pdd->revision = pci_config_read(bus, device, function, 0x08);
    pdd->interrupt = pci_config_read(bus, device, function, 0x3C);

    return pdd;
}
