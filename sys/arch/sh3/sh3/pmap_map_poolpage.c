vaddr_t
pmap_map_poolpage(paddr_t pa)
{
	struct vm_page *pg;
	struct vm_page_md *pvh;
	struct pv_entry *pv;

	if (pa < 0x20000000) {
		return SH3_PHYS_TO_P1SEG(pa);
	}
	pg = PHYS_TO_VM_PAGE(pa);
	assert(pg);
	pvh = &pg->mdpage;
	if ((pv = SLIST_FIRST(&pvh->pvh_head)) != 0)
		return pv->pv_va;

	assert(kernel_map);
	uvm_flag_t mapflags = UVM_MAPFLAG(UVM_PROT_ALL, UVM_PROT_ALL,
	    UVM_INH_NONE, UVM_ADV_RANDOM, UVM_FLAG_NOMERGE);
	struct uvm_map_args args;
	uvm_map_prepare(kernel_map, 0, PAGE_SIZE, NULL,
	    UVM_UNKNOWN_OFFSET,  0, mapflags, &args);

	vaddr_t va = args.uma_start;
	printf ("%s: pa=%lx va=%lx\n", __FUNCTION__, pa, va);

	pmap_kenter_pa(va, VM_PAGE_TO_PHYS(pg),
	    VM_PROT_READ|VM_PROT_WRITE|PMAP_KMPAGE, 0);

	vm_map_unlock(kernel_map);
	return va;
}

paddr_t
pmap_unmap_poolpage(vaddr_t va)
{
	paddr_t pa;

	pmap_extract(&__pmap_kernel, va, &pa);
	if (pa < 0x20000000)
		return SH3_P1SEG_TO_PHYS(va);
	return pa;
}
