#include "platform/graphics_options.h"
#include "platform/user_data.h"
#include <stdio.h>
#include <string.h>
#define CHECK(x) do { if (!(x)) { fprintf(stderr, "Failed line %d: %s\n", __LINE__, #x); return 1; } } while (0)
int main(int argc, char **argv) {
    CHECK(argc==2 && SDL_Init(0));
    char *root=NULL;
    CHECK(SDL_asprintf(&root,"%s-%llu",argv[1],(unsigned long long)SDL_GetPerformanceCounter())>=0);
    CHECK(hm_user_data_start_at(root));
    CHECK(hm_user_data_set_setting("unrelated","42"));
    hm_graphics_options options;
    hm_graphics_defaults(&options);
    char row[64], detail[64];
    hm_graphics_row_value(&options, 2, row, sizeof(row));
    CHECK(!strcmp(row, "Windowed"));
    hm_graphics_row_value(&options, 3, row, sizeof(row));
    hm_graphics_value(&options, 3, detail, sizeof(detail));
    CHECK(!strcmp(row, "640 x 480") && !strcmp(row, detail));
    CHECK(options.antialiasing==HM_AA_OFF);
    options.linear = 1;
    CHECK(hm_graphics_scene_filter_mode(&options, 0) == 1);
    CHECK(hm_graphics_scene_filter_mode(&options, 1) == 0);
    options.linear = 2;
    CHECK(hm_graphics_scene_filter_mode(&options, 1) == 2);
    options.linear = 0;
    hm_graphics_profile_set(&options, HM_GRAPHICS_PROFILE_MODERN);
    for(int mode=HM_AA_OFF;mode<HM_AA_COUNT;++mode) {
        options.antialiasing=mode;
        options.width=1280;options.height=720;options.mode=2;
        options.linear=2;options.vsync=0;options.fps_numerator=-1;
        CHECK(hm_graphics_save(&options));
        hm_graphics_options loaded;
        hm_graphics_load(&loaded);
        CHECK(loaded.antialiasing==mode && loaded.width==1280 && loaded.height==720);
        CHECK(loaded.mode==2 && loaded.linear==2 && loaded.vsync==0 && loaded.fps_numerator==-1);
    }
    char json[2048];
    CHECK(hm_user_data_read_settings(json,sizeof(json))>0 && strstr(json,"\"unrelated\":42"));
    CHECK(hm_user_data_set_nested_setting("enhance","antialiasing","999"));
    hm_graphics_load(&options);
    CHECK(options.antialiasing==HM_AA_OFF);
    options.antialiasing=HM_AA_MSAA4;options.renderer=1;
    CHECK(hm_graphics_save(&options));
    hm_graphics_load(&options);
    CHECK(options.antialiasing==HM_AA_OFF && options.renderer==1);
    options.renderer=0;options.aa_capabilities=15;
    for(int mode=1;mode<=HM_AA_COUNT;++mode) {
        CHECK(hm_graphics_change(NULL,NULL,&options,6,1));
        CHECK(options.antialiasing==mode%HM_AA_COUNT);
    }
    options.antialiasing=HM_AA_MSAA2;
    CHECK(!hm_graphics_change(NULL,NULL,&options,11,1) && options.renderer==0);
    options.renderer=1;options.antialiasing=HM_AA_FXAA;
    CHECK(hm_graphics_change(NULL,NULL,&options,6,1) && options.antialiasing==HM_AA_OFF);
    options.aa_capabilities=1;
    CHECK(!hm_graphics_change(NULL,NULL,&options,6,1) && options.antialiasing==HM_AA_OFF);
    options.linear=0;
    CHECK(hm_graphics_change(NULL,NULL,&options,7,1) && options.linear==1);
    CHECK(hm_graphics_change(NULL,NULL,&options,7,1) && options.linear==2);
    CHECK(hm_graphics_change(NULL,NULL,&options,7,1) && options.linear==0);
    options.renderer = 0;
    options.scaling = 1; options.widescreen = 1; options.linear = 2;
    options.antialiasing = HM_AA_FXAA;
    hm_graphics_profile_set(&options, HM_GRAPHICS_PROFILE_NATIVE);
    CHECK(!options.pgxp && options.dither && !options.scaling && !options.widescreen && !options.linear);
    CHECK(hm_graphics_save(&options));
    hm_graphics_load(&options);
    CHECK(options.profile == HM_GRAPHICS_PROFILE_NATIVE && !options.scaling && !options.linear);
    hm_graphics_profile_set(&options, HM_GRAPHICS_PROFILE_ENHANCE);
    CHECK(options.pgxp && !options.dither && !options.scaling && !options.linear);
    hm_graphics_profile_set(&options, HM_GRAPHICS_PROFILE_MODERN);
#ifdef __ANDROID__
    CHECK(options.pgxp && !options.dither && !options.scaling && options.widescreen &&
          options.linear == 1);
#else
    CHECK(options.pgxp && !options.dither && options.scaling && options.widescreen && options.linear == 2);
#endif
    CHECK(options.antialiasing == HM_AA_FXAA);
    hm_user_data_stop();SDL_free(root);SDL_Quit();
    return 0;
}
