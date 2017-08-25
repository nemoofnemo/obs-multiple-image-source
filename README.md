# obs-multiple-image-source
 an obs-plugin based on slide-show.
 
## register in image-source.c

	... 
	extern struct obs_source_info slideshow_info;
	extern struct obs_source_info color_source_info;
	extern struct obs_source_info multiple_image_source_info;
	extern struct obs_source_info pen_source_info;

	bool obs_module_load(void)
	{
		obs_register_source(&image_source_info);
		obs_register_source(&color_source_info);
		obs_register_source(&slideshow_info);
		obs_register_source(&multiple_image_source_info);
		obs_register_source(&pen_source_info);
		return true;
	}
	-eof
 
	 todo:paint shapes
