# obs-multiple-image-source
Multiple-image-source is an obs-studio plugin based on slideshow source.This plugin also provides notation drawing feature on source.  
The URL of complete project is https://github.com/nemoofnemo/obs-studio
 
## path
 you can find source code in /obs-studio/plugins/image-source.
 
## register in image-source.c
	... 
	extern struct obs_source_info slideshow_info;
	extern struct obs_source_info color_source_info;
	extern struct obs_source_info multiple_image_source_info;
	
	//pen source is protype of multiple-image-source.
	//extern struct obs_source_info pen_source_info;
	
	bool obs_module_load(void)
	{
		obs_register_source(&image_source_info);
		obs_register_source(&color_source_info);
		obs_register_source(&slideshow_info);
		obs_register_source(&multiple_image_source_info);
		//obs_register_source(&pen_source_info);
		return true;
	}
	-eof

## create source
    OBSScene scene = GetCurrentScene();
    if (!scene)
        return;

    obs_data_t * settings = obs_data_create();
	obs_data_array_t * arr = obs_data_array_create();
	obs_data_t * item = obs_data_create();
	obs_data_set_string(item, "value", "C:/Users/nemo/Pictures/Saved Pictures/5d4b1990f19600ae67ab5cd778c0ebd4.jpg");
	obs_data_array_push_back(arr, item);
	obs_data_release(item);
	item = obs_data_create();
	obs_data_set_string(item, "value", "C:/Users/nemo/Pictures/Saved Pictures/498da2d89f1878e408225405ac794feb.jpg");
	obs_data_array_push_back(arr, item);
	obs_data_release(item);
	item = obs_data_create();
	obs_data_set_string(item, "value", "C:/Users/nemo/Pictures/Saved Pictures/b8ce878cf1f7c2f5db3da241cdbda9fe.jpg");
	obs_data_array_push_back(arr, item);
	obs_data_release(item);
	obs_data_set_array(settings, "files", arr);
	obs_data_array_release(arr);

	obs_source_t * source = obs_source_create("multiple_image_source", "ppt", settings, NULL);
	obs_data_release(settings);

	if (source) {
		obs_scene_atomic_update(scene, [](void *data, obs_scene_t *scene){
			obs_scene_add(scene, (obs_source_t *)data);
		}, source);
	}

	obs_source_update(source, settings);
    obs_source_release(source);

## todo
  i'm working on painting polyline.  
  next:  
  change color,  
  line  
  circle  
  arrow  
  text  
  .....
