# obs-multiple-image-source
Multiple-image-source is an obs-studio plugin based on slideshow source.This plugin also provides notation drawing feature on source.  
The URL of complete project is https://github.com/nemoofnemo/obs-studio

基于obs-slideshow的显示多张图片的插件，核心功能是像演示ppt一样演示多张图片并可以在源上绘制图形。
 
## path
you can find source code in /obs-studio/plugins/image-source.  
make sure CMakeLists.txt in image-source is correct.

## register in image-source.c
	... 
	extern struct obs_source_info slideshow_info;
	extern struct obs_source_info color_source_info;
	extern struct obs_source_info multiple_image_source_info;
	
	//pen source is protype of multiple-image-source.
	//another implementation.
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
add file to source first.

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

## handle mouse event
for example, you can handle mouse event in windows-basic-preview.cpp.  
like this:

    void OBSBasicPreview:mousePressEvent(QMouseEvent *event){
		......
		
		vec2 pos = GetMouseEventPos(event);
		float mouse_x = 0.0f;
		float mouse_y = 0.0f;
		mouse_x = pos.x;
		mouse_y = pos.y;

		OBSScene scene = main->GetCurrentScene();
		if (scene){
			obs_sceneitem_t * item = obs_scene_find_source(scene, "ppt");
			if (item){
				obs_source_t * s = obs_sceneitem_get_source(item);
				OBSData settings = obs_source_get_settings(s);
				obs_data_set_int(settings, "mouse_x", (int)mouse_x);
				obs_data_set_int(settings, "mouse_y", (int)mouse_y);
				obs_data_set_string(settings, "create_shape", "polyline");
				obs_data_set_bool(settings, "need_update", true);
				obs_data_release(settings);
			}
		}
		
		.....
	}
	
	...functions...
	
	void OBSBasicPreview::mouseMoveEvent(QMouseEvent *event){
	    .....
		
		vec2 pos = GetMouseEventPos(event);
		float mouse_x = 0.0f;
		float mouse_y = 0.0f;
		mouse_x = pos.x;
		mouse_y = pos.y;

		OBSBasic *main = reinterpret_cast<OBSBasic*>(App()->GetMainWindow());
		OBSScene scene = main->GetCurrentScene();
		if (scene){
			obs_sceneitem_t * item = obs_scene_find_source(scene, "ppt");
			if (item){
				obs_source_t * s = obs_sceneitem_get_source(item);
				OBSData settings = obs_source_get_settings(s);
				obs_data_set_int(settings, "mouse_x", (int)mouse_x);
				obs_data_set_int(settings, "mouse_y", (int)mouse_y);
				obs_data_set_bool(settings, "need_update", true);
				obs_data_release(settings);
				qDebug("update paint event");
			}
		}
		
		.....
	}
	

## todo
  i'm working on painting polyline.  
  next:  
  change color,  
  line  
  circle  
  arrow  
  text  
  .....  
  
