#include "kvm/kvm.h"
#include "kvm/devices.h"
#include "kvm/fdt.h"
#include "kvm/irq.h"
#include "kvm/term.h"
#include "kvm/util-init.h"

#include <stdio.h>

struct pl011_dev {
	struct kvm *kvm;
	struct device_header hdr;

	u64 base;
	u16 irq;

	
};

#ifdef CONFIG_HAS_LIBFDT
static u32 generate_apb_clock_fdt_node(void *fdt)
{
	u32 clk_phandle = PHANDLE_CLK;

	/*
	 * clk24mhz {
	 * 	compatible = "fixed-clock";
	 *	#clock-cells = <0>;
	 *	clock-frequency = <24000000>;
	 *	clock-output-names = "clk24mhz";
	 *	phandle = <clk_phandle>;
	 * };
	 */

	_FDT(fdt_begin_node(fdt, "clk24mhz"));
	_FDT(fdt_property_string(fdt, "compatible", "fixed-clock"));
	_FDT(fdt_property_cell(fdt, "#clock-cells", 0));
	_FDT(fdt_property_cell(fdt, "clock-frequency", 24000000));
	_FDT(fdt_property_string(fdt, "clock-output-names", "clk24mhz"));
	
	_FDT(fdt_property_cell(fdt, "phandle", clk_phandle));
	_FDT(fdt_end_node(fdt));

	return clk_phandle;
}

static void generate_pl011_fdt_node(void *fdt,
				    struct device_header *hdr,
				    fdt_gen_irq generate_irq_prop)
{
	int ret;
	u32 clk_phandles[2];
	char *node_name;

	const char *clock_names = "uartclk\0apb_pclk";
	const char *pl011_compat = "arm,pl011\0arm,primecell";
	
	struct pl011_dev *dev = container_of(hdr, struct pl011_dev, hdr);
	u64 reg_property[] = {
		cpu_to_fdt64(dev->base),
		cpu_to_fdt64(ARM_PL011_SIZE),
	};

	/*
	 * uart@dev->base {
	 *	compatible = "arm,pl011", "arm,primecell";
	 * 	reg = <dev->base 0x1000>;
	 *	interrupts = <0 dev->irq IRQ_TYPE_LEVEL_HIGH>;
	 * 	clocks = <&clk24mhz>, <&clk24mhz>;
	 *	clock-names = "uartclk", "apb_pclk";
	 * };
	 */

	ret = asprintf(&node_name, "uart@%llx", dev->base);
	if (ret == -1)
		return;

	clk_phandles[0] = clk_phandles[1] = generate_apb_clock_fdt_node(fdt);
	
	_FDT(fdt_begin_node(fdt, node_name));
	_FDT(fdt_property(fdt, "compatible",
			  pl011_compat, sizeof(pl011_compat)));
	_FDT(fdt_property(fdt, "reg", reg_property, sizeof(reg_property)));
	generate_irq_prop(fdt, dev->irq, IRQ_TYPE_LEVEL_HIGH);
	_FDT(fdt_property(fdt, "clocks", clk_phandles, sizeof(clk_phandles)));
	_FDT(fdt_property(fdt, "clock-names", clock_names, sizeof(clock_names)));
	
	_FDT(fdt_end_node(fdt));

	free(node_name);
}
#else
static void generate_pl011_fdt_node(void *fdt,
				    struct device_header *hdr,
				    fdt_gen_irq generate_irq_prop)
{
	die("Unable to generate device tree nodes without libfdt\n");
}
#endif

static void pl011__mmio_callback(struct kvm_cpu *vcpu, u64 addr, u8 *data,
				 u32 len, u8 is_write, void *ptr)
{
	struct pl011_dev *dev = ptr;
}

static int pl011__init(struct kvm *kvm)
{
	int ret;
	struct pl011_dev *dev;

	if (kvm->cfg.active_console != CONSOLE_PL011)
		return 0;

	dev = malloc(sizeof(*dev));
	if (!dev)
		return -ENOMEM;

	dev->kvm = kvm;
	dev->hdr = (struct device_header ) {
		.bus_type = DEVICE_BUS_MMIO,
		.data = generate_pl011_fdt_node,
	};

	dev->base = ARM_PL011_BASE;
	dev->irq = irq__alloc_line();
	
	ret = kvm__register_mmio(kvm, dev->base, ARM_PL011_SIZE,
				 false, pl011__mmio_callback, dev);
	if (ret) {
		free(dev);
		return ret;
	}

	device__register(&dev->hdr);
	return 0;
}
dev_init(pl011__init);

static int pl011__exit(struct kvm *kvm)
{
	/* TODO: free the pl011_dev structure */
	/* device__unregister(); */
	return 0;
}
dev_exit(pl011__exit);
