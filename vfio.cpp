#include <stdio.h>
#include <unistd.h>
#include <stdint.h>
#include <stdlib.h>
#include <linux/vfio.h>
#include <inttypes.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <string>
#include <iostream>
#include <cstring>  
struct args
{
	__u64	iova;				/* IO virtual address */
	__u64	size;
	std::string iommu_group;
	std::string bdf;

};
int main(int argc, char *argv[]) 
{
    int ret = 0;
	
    std::cout << "Program name: " << argv[0] << std::endl;
		struct args para={0};
    for (int i = 1; i < argc; ++i) 
	{
	
        std::cout << "Argument " << i << ": " << argv[i] << std::endl;
		if(!std::strcmp(argv[i], "-help"))
		{
		    printf("-iova:         input the map ddress\n");
			printf("-size:         input the map length\n");
			printf("-device:       input the bdf\n");
			printf("-immmo_group:  input the iommu_group\n");
			return 0;	
		}
		if(!std::strcmp(argv[i], "-iova"))
		{
			i++;
			char *address = argv[i];
			sscanf(address, "%" SCNx64, &para.iova);
			printf("The address is: %" PRIx64 "\n", para.iova);
		}
		if(!std::strcmp(argv[i], "-size"))
		{
		    i++;
			para.size = atoi(argv[i]);
            std::cout << "the size is "<< para.size << std::endl;
		}
		if(!std::strcmp(argv[i], "-immmo_group"))
		{
		  	i++;
			para.iommu_group = argv[i];
            std::cout << "iommu group is "<< para.iommu_group << std::endl;

		}
        if(!std::strcmp(argv[i] ,  "-device"))
		{
		    i++;
			char *input_string = argv[i];
			para.bdf = input_string ;
		}
			
    }
	int container, group, device, i;
	struct vfio_group_status group_status =
					{ .argsz = sizeof(group_status) };
	struct vfio_iommu_type1_info iommu_info = { .argsz = sizeof(iommu_info) };
	struct vfio_iommu_type1_dma_map dma_map = { .argsz = sizeof(dma_map) };
	struct vfio_device_info device_info = { .argsz = sizeof(device_info) };

	/* Create a new container */
	container = open("/dev/vfio/vfio", O_RDWR);

        
	if (ioctl(container, VFIO_GET_API_VERSION) != VFIO_API_VERSION)
		/* Unknown API version */
    {
        std::cout << "get api version failed! "<< std::endl;
		return 1;
    }
	if (!ioctl(container, VFIO_CHECK_EXTENSION, VFIO_TYPE1_IOMMU))
		/* Doesn't support the IOMMU driver we want. */
    {
           std::cout << "check extension failed! "<< std::endl;
		   return 1;
    }
		
	/* Open the group */
    std::string iommu_group;
    if(!para.iommu_group.empty())
    {
	    iommu_group = "/dev/vfio/" + para.iommu_group;
    }
    else 
    {
        iommu_group = "/dev/vfio/14";
    }
    std::cout << "iommu  open group  "<<iommu_group << std::endl;
    group = open(iommu_group.c_str(), O_RDWR);

    if (group < 0) 
    {
	    /* if file not found, it's not an error */
	    if (errno != ENOENT) 
		{
	        std::cout << "Cannot open "<<iommu_group  << " " <<  strerror(errno);
	        return -1;
	    }
    }

	/* Test the group is viable and available */
    ret = ioctl(group, VFIO_GROUP_GET_STATUS, &group_status);
    if(ret)
    {
        std::cout << "group get status failed  ! "<< std::endl;
        printf("Cannot set status  error %i (%s)\n", errno, strerror(errno));
        close(group);
    }

	if (!(group_status.flags & VFIO_GROUP_FLAGS_VIABLE))
		/* Group is not viable (ie, not all devices bound for vfio) */
    {
        std::cout << "group flag sucess ! "<< std::endl;
        close(group);
  
    }
	/* Add the group to the container */
    ret = ioctl(group, VFIO_GROUP_SET_CONTAINER, &container);
    if (ret) 
	{
        std::cout << "group set container failed ! close group! "<< std::endl;
        close(group);
	    return 1;
    }

	/* Enable the IOMMU model we want */
	if (ioctl(container, VFIO_SET_IOMMU, VFIO_TYPE1_IOMMU))
    {
        std::cout << "group set iommu failed ! close group! "<< std::endl;
        close(group);
        return 1;;

    }

	/* Get addition IOMMU info */
	ioctl(container, VFIO_IOMMU_GET_INFO, &iommu_info);
	
	dma_map.size = 1024 * 1024;
    dma_map.iova = 0; /* 1MB starting at 0x0 from device view */
    if(para.size)
    {
	    dma_map.size = para.size; 
    }
    if(para.iova!= dma_map.iova)
    {
	    dma_map.iova = para.iova;  /* 1MB starting at 0x0 from device view */
    }

	/* Allocate some space and setup a DMA mapping */
	dma_map.vaddr =(__u64)mmap(0, dma_map.size, PROT_READ | PROT_WRITE,
			     MAP_PRIVATE | MAP_ANONYMOUS, 0, 0);
   
	dma_map.flags = VFIO_DMA_MAP_FLAG_READ | VFIO_DMA_MAP_FLAG_WRITE;

     printf("the dma map vaddr is : %llx,the iova is % llx,the size is %d\n",dma_map.vaddr,dma_map.iova,dma_map.size);
	if(ioctl(container, VFIO_IOMMU_MAP_DMA, &dma_map))
    {
        std::cout << "container dma map failed! "<< std::endl;
        close(group);
        return 1;
    }

	/* Get a file descriptor for the device */
    if(para.bdf.empty())
    {
        para.bdf = "0000:05:00.0"; //
    }
    std::cout << "device  is " << para.bdf << std::endl;

    std::string  lspciString="lspci -s " + para.bdf + " -v";
    system(lspciString.c_str());

    device = ioctl(group, VFIO_GROUP_GET_DEVICE_FD, para.bdf.c_str());
    if (device < 0) 
	{
		printf ("get vfio dev fd failed ! error %i (%s)\n" ,errno , strerror(errno));
		close(group);
        return -1;
    }


	/* Test and setup the device */
	ioctl(device, VFIO_DEVICE_GET_INFO, &device_info);

	for (i = 0; i < device_info.num_regions; i++) 
	{
		struct vfio_region_info reg = { .argsz = sizeof(reg) };
        
		reg.index = i;
		
		ioctl(device, VFIO_DEVICE_GET_REGION_INFO, &reg);

		/* Setup mappings... read/write offsets, mmaps
		 * For PCI devices, config space is a region */
	}

	for (i = 0; i < device_info.num_irqs; i++) 
	{
		struct vfio_irq_info irq = { .argsz = sizeof(irq) };

		irq.index = i;

		ioctl(device, VFIO_DEVICE_GET_IRQ_INFO, &irq);

		/* Setup IRQs... eventfds, VFIO_DEVICE_SET_IRQS */
	}

	while(1)
	{
            char input[5];
	    sleep(150);
	    printf("could you want EXIT ?enter [yes|no]? : ");
            scanf("%s", input);
	    if(!std::strcmp(input, "yes"))
	    {
		break;
	    }
	}

	//start to unmap the iova
	struct vfio_iommu_type1_dma_unmap dma_unmap;
	memset(&dma_unmap, 0, sizeof(dma_unmap));
	dma_unmap.argsz = sizeof(struct vfio_iommu_type1_dma_unmap);
	dma_unmap.size = dma_map.size;
	dma_unmap.iova = dma_map.iova;

	ret = ioctl(container, VFIO_IOMMU_UNMAP_DMA,&dma_unmap);
	if (ret) 
	{
		printf ("umap dma failed ! error %i (%s)\n" ,errno , strerror(errno));
		return -1;
	}
		
	close(group);

	return 0;
}
